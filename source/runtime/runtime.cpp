//
//  runtime.cpp
//
//  Created by Edmund Kapusniak on 01/11/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#define __STDC_FORMAT_MACROS

#include "kenaf/runtime.h"
#include <inttypes.h>
#include <stdarg.h>
#include "vmachine.h"
#include "call_stack.h"
#include "collector.h"
#include "execute.h"
#include "objects/array_object.h"
#include "objects/table_object.h"
#include "objects/function_object.h"
#include "objects/cothread_object.h"
#include "objects/u64val_object.h"
#include "corlib/corobjects.h"
#include "corlib/corprint.h"
#include "corlib/cormath.h"

namespace kf
{

static_assert( (unsigned)CODE_VARARG == (unsigned)FUNCTION_VARARG );
static_assert( (unsigned)CODE_DIRECT == (unsigned)FUNCTION_DIRECT );

/*
    Thread-local VM state.
*/

static thread_local runtime* current_runtime = nullptr;

/*
    Runtime and context.
*/

struct runtime
{
    vmachine vm;
    intptr_t refcount;
    context* current_context;
    error_handler handler;
    stack_trace* backtrace;
};

struct context
{
    vcontext vc;
    intptr_t refcount;
    struct runtime* runtime;
};

runtime* create_runtime()
{
    runtime* r = new runtime();
    r->refcount = 1;
    r->current_context = nullptr;
    r->handler = nullptr;
    r->backtrace = nullptr;
    start_collector( &r->vm );
    setup_object_model( &r->vm );
    return r;
}

runtime* retain_runtime( runtime* r )
{
    assert( r->refcount >= 1 );
    r->refcount += 1;
    return r;
}

void release_runtime( runtime* r )
{
    assert( r->refcount >= 1 );
    if ( --r->refcount == 0 )
    {
        if ( current_runtime == r )
        {
            make_current( nullptr );
        }

        for ( value v : r->vm.values )
        {
            if ( box_is_object_or_string( v ) )
            {
                object_release( &r->vm, unbox_object_or_string( v ) );
            }
        }

        stop_collector( &r->vm );
        delete r;
    }
}

void set_runtime_value( runtime* r, size_t index, value v )
{
    r->vm.values.resize( std::max( r->vm.values.size(), index + 1 ), null_value );
    value u = r->vm.values.at( index );
    if ( box_is_object_or_string( v ) ) object_retain( &r->vm, unbox_object_or_string( v ) );
    if ( box_is_object_or_string( u ) ) object_release( &r->vm, unbox_object_or_string( u ) );
    r->vm.values[ index ] = v;
}

value get_runtime_value( runtime* r, size_t index )
{
    if ( index < r->vm.values.size() )
        return r->vm.values[ index ];
    else
        return null_value;
}

context* create_context( runtime* r )
{
    // Create context.
    context* c = new context();
    c->refcount = 1;
    c->runtime = retain_runtime( r );
    c->vc.cothread = cothread_new( &r->vm );
    c->vc.global_object = lookup_new( &r->vm, r->vm.prototypes[ LOOKUP_OBJECT ] );
    link_vcontext( &r->vm, &c->vc );

    // Set up builtins.
    context* prev = make_current( c );
    expose_corobjects( &r->vm );
    expose_corprint();
    expose_cormath();
    make_current( prev );

    // Done.
    return c;
}

context* retain_context( context* c )
{
    assert( c->refcount >= 1 );
    c->refcount += 1;
    return c;
}

void release_context( context* c )
{
    assert( c->refcount >= 1 );
    if ( --c->refcount == 0 )
    {
        if ( current_runtime == c->runtime && c->runtime->current_context == c )
        {
            make_current( nullptr );
        }

        for ( value v : c->vc.values )
        {
            if ( box_is_object_or_string( v ) )
            {
                object_release( &c->runtime->vm, unbox_object_or_string( v ) );
            }
        }

        unlink_vcontext( &c->runtime->vm, &c->vc );
        release_runtime( c->runtime );
        delete c;
    }
}

void set_context_value( size_t index, value v )
{
    context* c = current_runtime->current_context;
    c->vc.values.resize( std::max( c->vc.values.size(), index + 1 ), null_value );
    value u = c->vc.values.at( index );
    if ( box_is_object_or_string( v ) ) object_retain( &c->runtime->vm, unbox_object_or_string( v ) );
    if ( box_is_object_or_string( u ) ) object_release( &c->runtime->vm, unbox_object_or_string( u ) );
    c->vc.values[ index ] = v;
}

value get_context_value( size_t index )
{
    context* c = current_runtime->current_context;
    if ( index < c->vc.values.size() )
        return c->vc.values[ index ];
    else
        return null_value;
}

context* make_current( context* c )
{
    context* prev = nullptr;

    if ( current_runtime )
    {
        prev = current_runtime->current_context;
        assert( current_runtime->vm.c == &prev->vc );
        current_runtime->vm.c = nullptr;
        current_runtime->current_context = nullptr;
        current_runtime = nullptr;
    }

    if ( c )
    {
        current_runtime = c->runtime;
        c->vc.cothread = mark_cothread( &current_runtime->vm, c->vc.cothread );
        current_runtime->vm.c = &c->vc;
        current_runtime->current_context = c;
    }

    return prev;
}

value global_object()
{
    return box_object( current_runtime->vm.c->global_object );
}

/*
    Public API.
*/

inline vmachine* current() { return &current_runtime->vm; }

value retain( value v )
{
    if ( box_is_object_or_string( v ) )
    {
        object_retain( current(), unbox_object_or_string( v ) );
    }
    return v;
}

void release( value v )
{
    if ( box_is_object_or_string( v ) )
    {
        object_release( current(), unbox_object_or_string( v ) );
    }
}

value_kind kindof( value v )
{
    if ( box_is_number( v ) )
    {
        return NUMBER;
    }
    else if ( box_is_string( v ) )
    {
        return STRING;
    }
    else if ( box_is_object( v ) )
    {
        return (value_kind)( header( unbox_object( v ) )->type );
    }
    else if ( box_is_bool( v ) )
    {
        return BOOL_VALUE;
    }
    else if ( box_is_u64val( v ) )
    {
        return U64VAL;
    }
    else
    {
        return NULL_VALUE;
    }
}

bool is_lookup( value v )
{
    return box_is_object_type( v, LOOKUP_OBJECT );
}

bool is_string( value v )
{
    return box_is_string( v );
}

bool is_array( value v )
{
    return box_is_object_type( v, ARRAY_OBJECT );
}

bool is_table( value v )
{
    return box_is_object_type( v, TABLE_OBJECT );
}

bool is_function( value v )
{
    return box_is_object_type( v, FUNCTION_OBJECT ) || box_is_object_type( v, NATIVE_FUNCTION_OBJECT );
}

bool is_cothread( value v )
{
    return box_is_object_type( v, COTHREAD_OBJECT );
}

bool is_u64val( value v )
{
    return box_is_u64val( v ) || box_is_object_type( v, U64VAL_OBJECT );
}

bool is_number( value v )
{
    return box_is_number( v );
}

bool is_bool( value v )
{
    return box_is_bool( v );
}

bool is_null( value v )
{
    return box_is_null( v );
}

value superof( value v )
{
    vmachine* vm = current();
    return box_object( value_superof( vm, v ) );
}

bool test( value v )
{
    return v.v > 1 && v.v != box_number( +0.0 ).v && v.v != box_number( -0.0 ).v;
}

value bool_value( bool b )
{
    return b ? true_value : false_value;
}

bool get_bool( value v )
{
    if ( ! is_bool( v ) ) raise_type_error( v, "a bool" );
    return v.v == true_value.v;
}

value number_value( double n )
{
    return box_number( n );
}

double get_number( value v )
{
    if ( ! is_number( v ) ) raise_type_error( v, "a number" );
    return unbox_number( v );
}

value u64val_value( uint64_t u )
{
    vmachine* vm = current();
    return u64val_value( vm, u );
}

uint64_t get_u64val( value v )
{
    uint64_t u;
    if ( u64val_check( v, &u ) )
        return u;
    else
        raise_type_error( v, "a u64val" );
}

value create_lookup()
{
    vmachine* vm = current();
    return box_object( lookup_new( vm, vm->prototypes[ LOOKUP_OBJECT ] ) );
}

value create_lookup( value prototype )
{
    if ( ! is_lookup( prototype ) ) raise_type_error( prototype, "a lookup object" );
    return box_object( lookup_new( current(), (lookup_object*)unbox_object( prototype ) ) );
}

value create_lookup( value prototype, std::string_view ksnames[], size_t kscount )
{
    if ( ! is_lookup( prototype ) ) raise_type_error( prototype, "a lookup object" );
    vmachine* vm = current();
    lookup_object* lookup = lookup_new( vm, (lookup_object*)unbox_object( prototype ) );
    for ( size_t index = 0; index < kscount; ++index )
    {
        lookup_addkeyslot( vm, lookup, index, ksnames[ index ] );
    }
    return box_object( lookup );
}

value get_keyslot( value lookup, size_t index )
{
    if ( ! is_lookup( lookup ) ) raise_type_error( lookup, "a lookup object" );
    return lookup_getkeyslot( current(), (lookup_object*)unbox_object( lookup ), index );
}

void set_keyslot( value lookup, size_t index, value v )
{
    if ( ! is_lookup( lookup ) ) raise_type_error( lookup, "a lookup object" );
    lookup_setkeyslot( current(), (lookup_object*)unbox_object( lookup ), index, v );
}

value get_key( value lookup, std::string_view k )
{
    vmachine* vm = current();
    selector sel = {};
    string_object* skey = string_key( vm, k.data(), k.size() );
    return lookup_getkey( vm, value_keyerof( vm, lookup ), skey, &sel );
}

void set_key( value lookup, std::string_view k, value v )
{
    vmachine* vm = current();
    selector sel = {};
    if ( ! is_lookup( lookup ) ) raise_type_error( lookup, "a lookup object" );
    string_object* skey = string_key( vm, k.data(), k.size() );
    lookup_setkey( vm, (lookup_object*)unbox_object( lookup ), skey, &sel, v );
}

bool has_key( value lookup, std::string_view k )
{
    vmachine* vm = current();
    if ( ! is_lookup( lookup ) ) return false;
    string_object* skey = string_key( vm, k.data(), k.size() );
    return lookup_haskey( vm, (lookup_object*)unbox_object( lookup ), skey );
}

void del_key( value lookup, std::string_view k )
{
    vmachine* vm = current();
    if ( ! is_lookup( lookup ) ) return;
    string_object* skey = string_key( vm, k.data(), k.size() );
    lookup_delkey( vm, (lookup_object*)unbox_object( lookup ), skey );
}

value create_string( std::string_view text )
{
    return box_string( string_new( current(), text.data(), text.size() ) );
}

value create_string_buffer( size_t size, char** out_text )
{
    string_object* s = string_new( current(), nullptr, size );
    *out_text = s->text;
    return box_string( s );
}

std::string_view get_string( value string )
{
    if ( ! is_string( string ) ) raise_type_error( string, "a string" );
    string_object* s = unbox_string( string );
    return std::string_view( s->text, s->size );
}

value create_array()
{
    return box_object( array_new( current(), 0 ) );
}

value create_array( size_t capacity )
{
    return box_object( array_new( current(), capacity ) );
}

size_t array_length( value array )
{
    if ( ! is_array( array ) ) raise_type_error( array, "an array" );
    array_object* a = (array_object*)unbox_object( array );
    return a->length;
}

void array_resize( value array, size_t length )
{
    if ( ! is_array( array ) ) raise_type_error( array, "an array" );
    array_resize( current(), (array_object*)unbox_object( array ), length );
}

void array_append( value array, value v )
{
    if ( ! is_array( array ) ) raise_type_error( array, "an array" );
    array_append( current(), (array_object*)unbox_object( array ), v );
}

void array_clear( value array )
{
    if ( ! is_array( array ) )raise_type_error( array, "an array" );
    array_clear( current(), (array_object*)unbox_object( array ) );
}

value create_table()
{
    return box_object( table_new( current(), 0 ) );
}

value create_table( size_t capacity )
{
    return box_object( table_new( current(), capacity ) );
}

size_t table_length( value table )
{
    if ( ! is_table( table ) ) raise_type_error( table, "a table" );
    table_object* t = (table_object*)unbox_object( table );
    return t->length;
}

void table_clear( value table )
{
    if ( ! is_table( table ) ) raise_type_error( table, "a table" );
    table_clear( current(), (table_object*)unbox_object( table ) );
}

value get_index( value table, value k )
{
    if ( is_table( table ) )
    {
        return table_getindex( current(), (table_object*)unbox_object( table ), k );
    }
    else if ( is_array( table ) )
    {
        if ( ! is_number( k ) ) raise_type_error( k, "a number" );
        size_t index = (size_t)(uint64_t)unbox_number( k );
        return array_getindex( current(), (array_object*)unbox_object( table ), index );
    }
    else
    {
        raise_type_error( table, "indexable" );
    }
}

value get_index( value array, size_t index )
{
    if ( is_array( array ) )
    {
        return array_getindex( current(), (array_object*)unbox_object( array ), index );
    }
    else if ( is_table( array ) )
    {
        return table_getindex( current(), (table_object*)unbox_object( array ), box_number( (double)index ) );
    }
    else
    {
        raise_type_error( array, "indexable" );
    }
}

void set_index( value table, value k, value v )
{
    if ( is_table( table ) )
    {
        table_setindex( current(), (table_object*)unbox_object( table ), k, v );
    }
    else if ( is_array( table ) )
    {
        if ( ! is_number( k ) ) raise_type_error( k, "a number" );
        size_t index = (size_t)(uint64_t)unbox_number( k );
        array_setindex( current(), (array_object*)unbox_object( table ), index, v );
    }
    else
    {
        raise_type_error( table, "indexable" );
    }
}

void set_index( value array, size_t index, value v )
{
    if ( is_array( array ) )
    {
        array_setindex( current(), (array_object*)unbox_object( array ), index, v );
    }
    else if ( is_table( array ) )
    {
        table_setindex( current(), (table_object*)unbox_object( array ), box_number( (double)index ), v );
    }
    else
    {
        raise_type_error( array, "indexable" );
    }
}

void del_index( value table, value k )
{
    if ( ! is_table( table ) ) raise_type_error( table, "a table" );
    table_delindex( current(), (table_object*)unbox_object( table ), k );
}

value create_function( const void* code, size_t size )
{
    vmachine* vm = current();
    program_object* program = program_new( vm, code, size );
    return box_object( function_new( vm, program ) );
}

/*
    Native function interface.
*/

value create_function( std::string_view name, native_function native, void* cookie, unsigned param_count, unsigned code_flags )
{
    return box_object( native_function_new( current(), name, native, cookie, param_count, code_flags ) );
}

stack_values push_results( frame* frame, size_t count )
{
    cothread_object* cothread = (cothread_object*)frame->sp;
    assert( current()->c->cothread == cothread );
    return { resize_stack( cothread, frame->bp, frame->bp + count ), count };
}

result return_results( frame* frame )
{
    cothread_object* cothread = (cothread_object*)frame->sp;
    assert( current()->c->cothread == cothread );
    assert( cothread->xp >= frame->bp );
    return cothread->xp - frame->bp;
}

result return_value( frame* frame, value v )
{
    cothread_object* cothread = (cothread_object*)frame->sp;
    assert( current()->c->cothread == cothread );
    value* r = resize_stack( cothread, frame->bp, frame->bp + 1 );
    r[ 0 ] = v;
    return 1;
}

size_t return_void( frame* frame )
{
    return 0;
}

/*
    Function calls.
*/

stack_values push_frame( frame* frame, size_t count )
{
    vmachine* vm = current();
    cothread_object* cothread = vm->c->cothread;
    unsigned bp = cothread->xp;
    cothread->stack_frames.push_back( { nullptr, bp, bp, 0, RESUME_CALL, 0, OP_STACK_MARK, 0 } );
    frame->sp = vm;
    frame->bp = bp;
    return { resize_stack( cothread, bp, 1 + count ) + 1, count };
}

stack_values call_frame( frame* frame, value function )
{
    vmachine* vm = (vmachine*)frame->sp;
    cothread_object* cothread = vm->c->cothread;

    stack_frame* stack_frame = &cothread->stack_frames.back();
    assert( ! stack_frame->function );
    assert( stack_frame->fp == frame->bp );
    assert( stack_frame->fp < cothread->xp );

    unsigned xp = cothread->xp - frame->bp;
    value* r = cothread->stack.data() + frame->bp;
    r[ 0 ] = function;

    xstate state;
    if ( ! call_value( vm, function, 0, xp, false, &state ) )
    {
        raise_type_error( function, "callable" );
    }

    if ( state.function )
    {
        execute( vm, state );
    }

    assert( vm->c->cothread == cothread );
    assert( frame->bp <= cothread->xp );
    return { cothread->stack.data() + frame->bp, cothread->xp - frame->bp };
}

void pop_frame( frame* frame )
{
    vmachine* vm = (vmachine*)frame->sp;
    cothread_object* cothread = vm->c->cothread;

    stack_frame* stack_frame = &cothread->stack_frames.back();
    assert( ! stack_frame->function );
    assert( stack_frame->fp == frame->bp );
    assert( stack_frame->fp <= cothread->xp );

    cothread->stack_frames.pop_back();
    cothread->xp = frame->bp;
    frame->sp = nullptr;
}

value call( value function, const value* arguments, size_t argcount )
{
    struct scope_frame : public frame
    {
        scope_frame() : frame{ nullptr, 0 } {}
        ~scope_frame() { if ( sp ) { pop_frame( this ); } }
    };

    scope_frame frame;
    stack_values argframe = push_frame( &frame, argcount );
    memcpy( argframe.values, arguments, argcount * sizeof( value ) );
    stack_values resframe = call_frame( &frame, function );
    value result = resframe.count ? resframe.values[ 0 ] : null_value;
    return result;
}

/*
    Errors.
*/

static std::string format_string( const char* format, va_list ap )
{
    va_list aq;

    va_copy( aq, ap );
    int size = vsnprintf( nullptr, 0, format, aq );
    va_end( aq );

    std::string s;
    s.resize( size + 1 );

    va_copy( aq, ap );
    vsnprintf( s.data(), s.size(), format, aq );
    va_end( aq );

    s.resize( size );
    return s;
}

static std::string format_string( const char* format, ... )
{
    va_list ap;
    va_start( ap, format );
    std::string s = format_string( format, ap );
    va_end( ap );
    return s;
}

static std::string format_value( value v )
{
    std::string s;
    if ( box_is_number( v ) )
    {
        s = format_string( "%f", unbox_number( v ) );
    }
    else if ( box_is_string( v ) )
    {
        string_object* string = unbox_string( v );
        s = std::string( string->text, string->size );
    }
    else if ( box_is_object( v ) )
    {
        const char* type_name = "object";
        switch ( header( unbox_object( v ) )->type )
        {
        case LOOKUP_OBJECT:             type_name = "lookup";           break;
        case ARRAY_OBJECT:              type_name = "array";            break;
        case TABLE_OBJECT:              type_name = "table";            break;
        case FUNCTION_OBJECT:           type_name = "function";         break;
        case NATIVE_FUNCTION_OBJECT:    type_name = "native function";  break;
        case COTHREAD_OBJECT:           type_name = "cothread";         break;
        case U64VAL_OBJECT:             type_name = "u64val";           break;
        default: break;
        }
        s = format_string( "<%s %p>", type_name, unbox_object( v ) );
    }
    else if ( box_is_bool( v ) )
    {
        s = v.v == true_value.v ? "true" : "false";
    }
    else if ( box_is_u64val( v ) )
    {
        s = format_string( "[%016" PRIX64 "]", unbox_u64val( v ) );
    }
    else
    {
        s = "null";
    }
    return s;
}

struct stack_trace
{
    intptr_t refcount;
    runtime* r;
    std::vector< std::string > frames;
};


stack_trace* create_stack_trace( runtime* r )
{
    stack_trace* s = new stack_trace();
    s->refcount = 1;
    s->r = retain_runtime( r );
    return s;
}

stack_trace* retain_stack_trace( stack_trace* s )
{
    assert( s->refcount >= 1 );
    s->refcount += 1;
    return s;
}

void release_stack_trace( stack_trace* s )
{
    assert( s->refcount >= 1 );
    if ( --s->refcount == 0 )
    {
        release_runtime( s->r );
        if ( s->r->backtrace == s )
        {
            s->r->backtrace = nullptr;
        }
        delete s;
    }
}

size_t stack_trace_count( stack_trace* s )
{
    return s->frames.size();
}

const char* stack_trace_frame( stack_trace* s, size_t index )
{
    return s->frames.at( index ).c_str();
}

void append_stack_trace( const char* format, ... )
{
    runtime* r = current_runtime;
    if ( r && r->backtrace )
    {
        va_list ap;
        va_start( ap, format );
        r->backtrace->frames.push_back( format_string( format, ap ) );
        va_end( ap );
    }
}

void install_handler( runtime* r, error_handler handler )
{
    r->handler = handler;
}

[[noreturn]] static void throw_error( error_kind error, const std::string& message, value raised )
{
    runtime* r = current_runtime;
    if ( r && r->handler )
    {
        r->backtrace = create_stack_trace( r );
        r->handler( error, message, r->backtrace, raised );
        release_stack_trace( r->backtrace );
    }

    throw std::runtime_error( message );
}

void raise_error( error_kind error, const char* format, ... )
{
    va_list ap;
    va_start( ap, format );
    std::string message = format_string( format, ap );
    va_end( ap );
    throw_error( error, message, null_value );
}

void raise_type_error( value v, std::string_view expected )
{
    std::string message = format_value( v );
    message += " is not ";
    message += expected;
    throw_error( ERROR_TYPE, message, null_value );
}

void throw_value( value raised )
{
    throw_error( ERROR_THROW, format_value( raised ), raised );
}

}

