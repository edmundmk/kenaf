//
//  runtime.cpp
//
//  Created by Edmund Kapusniak on 01/11/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "kenaf/runtime.h"
#include "objects/array_object.h"
#include "objects/table_object.h"
#include "objects/cothread_object.h"
#include "vm/vm_context.h"
#include "vm/vm_execute.h"
#include "corlib/corobjects.h"
#include "corlib/corprint.h"
#include "corlib/cormath.h"

namespace kf
{

/*
    Thread-local VM state.
*/

static thread_local runtime* current_runtime = nullptr;

/*
    Runtime and context.
*/

struct runtime
{
    vm_context vm;
    intptr_t refcount;
    context* current_context;
};

struct context
{
    intptr_t refcount;
    struct runtime* runtime;
    vm_cothread_stack cothreads;
    lookup_object* global_object;
};

runtime* create_runtime()
{
    runtime* r = new runtime();
    r->refcount = 1;
    r->current_context = nullptr;
    vm_setup_object_model( &r->vm );
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
        delete r;
    }
}

context* create_context( runtime* r )
{
    context* c = new context();
    c->refcount = 1;
    c->runtime = r;
    c->cothreads.push_back( cothread_new( &r->vm ) );
    c->global_object = lookup_new( &r->vm, r->vm.prototypes[ LOOKUP_OBJECT ] );
    context* prev = make_current( c );
    expose_corobjects( &r->vm );
    expose_corprint();
    expose_cormath();
    make_current( prev );
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
        delete c;
    }
}

context* make_current( context* c )
{
    context* prev = nullptr;

    if ( current_runtime )
    {
        prev = current_runtime->current_context;
        assert( current_runtime->vm.cothreads == &prev->cothreads );
        assert( current_runtime->vm.global_object == prev->global_object );
        current_runtime->vm.cothreads = nullptr;
        current_runtime->vm.global_object = nullptr;
        current_runtime->current_context = nullptr;
        current_runtime = nullptr;
    }

    if ( c )
    {
        current_runtime = c->runtime;
        current_runtime->vm.cothreads = &c->cothreads;
        current_runtime->vm.global_object = c->global_object;
        current_runtime->current_context = c;
    }

    return prev;
}

value global_object()
{
    return box_object( current_runtime->vm.global_object );
}

/*
    Public API.
*/

inline vm_context* current() { return &current_runtime->vm; }

value retain( value v )
{
    if ( box_is_object_or_string( v ) )
    {
        object* o = unbox_object_or_string( v );
        object_header* h = header( o );
        if ( h->refcount == 0 )
        {
            vm_context* vm = current();
            assert( ! vm->roots.contains( o ) );
            vm->roots.insert_or_assign( o, 0 );
        }
        if ( ++h->refcount == 0 )
        {
            vm_context* vm = current();
            vm->roots.at( o ) += 1;
            h->refcount = 255;
        }
    }
    return v;
}

void release( value v )
{
    if ( box_is_object_or_string( v ) )
    {
        object* o = unbox_object_or_string( v );
        object_header* h = header( o );
        assert( h->refcount > 0 );
        if ( h->refcount == 255 )
        {
            vm_context* vm = current();
            size_t& refcount = vm->roots.at( o );
            if ( refcount > 0 )
            {
                refcount -= 1;
            }
            else
            {
                h->refcount -= 1;
            }
        }
        else if ( --h->refcount == 0 )
        {
            vm_context* vm = current();
            vm->roots.erase( o );
        }
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

value null_value()
{
    return boxed_null;
}

value true_value()
{
    return boxed_true;
}

value false_value()
{
    return boxed_false;
}

value superof( value v )
{
    vm_context* vm = current();
    return box_object( vm_superof( vm, v ) );
}

bool test( value v )
{
    return v.v > 1 && v.v != box_number( +0.0 ).v && v.v != box_number( -0.0 ).v;
}

value bool_value( bool b )
{
    return b ? boxed_true : boxed_false;
}

bool get_bool( value v )
{
    if ( ! is_bool( v ) ) throw std::exception();
    return v.v == boxed_true.v;
}

value number_value( double n )
{
    return box_number( n );
}

double get_number( value v )
{
    if ( ! is_number( v ) ) throw std::exception();
    return unbox_number( v );
}

value create_lookup()
{
    vm_context* vm = current();
    return box_object( lookup_new( vm, vm->prototypes[ LOOKUP_OBJECT ] ) );
}

value create_lookup( value prototype )
{
    if ( ! is_lookup( prototype ) ) throw std::exception();
    return box_object( lookup_new( current(), (lookup_object*)unbox_object( prototype ) ) );
}

value get_key( value lookup, std::string_view k )
{
    vm_context* vm = current();
    selector sel = {};
    string_object* skey = string_key( vm, k.data(), k.size() );
    return lookup_getkey( vm, vm_keyerof( vm, lookup ), skey, &sel );
}

void set_key( value lookup, std::string_view k, value v )
{
    vm_context* vm = current();
    selector sel = {};
    if ( ! is_lookup( lookup ) ) throw std::exception();
    string_object* skey = string_key( vm, k.data(), k.size() );
    lookup_setkey( vm, (lookup_object*)unbox_object( lookup ), skey, &sel, v );
}

bool has_key( value lookup, std::string_view k )
{
    vm_context* vm = current();
    if ( ! is_lookup( lookup ) ) return false;
    string_object* skey = string_key( vm, k.data(), k.size() );
    return lookup_haskey( vm, (lookup_object*)unbox_object( lookup ), skey );
}

void del_key( value lookup, std::string_view k )
{
    vm_context* vm = current();
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

std::string_view get_text( value string )
{
    if ( ! is_string( string ) ) throw std::exception();
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
    if ( ! is_array( array ) ) throw std::exception();
    array_object* a = (array_object*)unbox_object( array );
    return a->length;
}

void array_resize( value array, size_t length )
{
    if ( ! is_array( array ) ) throw std::exception();
    array_resize( current(), (array_object*)unbox_object( array ), length );
}

void array_append( value array, value v )
{
    if ( ! is_array( array ) ) throw std::exception();
    array_append( current(), (array_object*)unbox_object( array ), v );
}

void array_clear( value array )
{
    if ( ! is_array( array ) ) throw std::exception();
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
    if ( ! is_table( table ) ) throw std::exception();
    table_object* t = (table_object*)unbox_object( table );
    return t->length;
}

void table_clear( value table )
{
    if ( ! is_table( table ) ) throw std::exception();
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
        if ( ! is_number( k ) ) throw std::exception();
        size_t index = (size_t)(uint64_t)unbox_number( k );
        return array_getindex( current(), (array_object*)unbox_object( table ), index );
    }
    else
    {
        throw std::exception();
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
        throw std::exception();
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
        if ( ! is_number( k ) ) throw std::exception();
        size_t index = (size_t)(uint64_t)unbox_number( k );
        array_setindex( current(), (array_object*)unbox_object( table ), index, v );
    }
    else
    {
        throw std::exception();
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
        throw std::exception();
    }
}

void del_index( value table, value k )
{
    if ( ! is_table( table ) ) throw std::exception();
    table_delindex( current(), (table_object*)unbox_object( table ), k );
}

value create_function( const void* code, size_t size )
{
    vm_context* vm = current();
    program_object* program = program_new( vm, code, size );
    return box_object( function_new( vm, program ) );
}

/*
    Native function interface.
*/

value create_function( native_function native, void* cookie, unsigned param_count, unsigned code_flags )
{
    return box_object( native_function_new( current(), native, cookie, param_count, code_flags ) );
}

value* arguments( frame* frame )
{
    cothread_object* cothread = (cothread_object*)frame->sp;
    assert( current()->cothreads->back() == frame->sp );
    return cothread->stack.data() + frame->fp + 1;
}

value* results( frame* frame, size_t count )
{
    cothread_object* cothread = (cothread_object*)frame->sp;
    assert( current()->cothreads->back() ==cothread );
    return vm_resize_stack( cothread, frame->fp, frame->fp + count );
}

size_t result( frame* frame, value v )
{
    value* r = results( frame, 1 );
    r[ 0 ] = v;
    return 1;
}

size_t rvoid( frame* frame )
{
    results( frame, 0 );
    return 0;
}

/*
    Function calls.
*/

stack_values push_frame( frame* frame, size_t argcount )
{
    vm_context* vm = current();
    cothread_object* cothread = vm->cothreads->back();
    unsigned fp = cothread->xp;
    cothread->stack_frames.push_back( { nullptr, fp, fp, 0, RESUME_CALL, 0, OP_STACK_MARK, 0 } );
    frame->sp = vm;
    frame->fp = fp;
    return { vm_resize_stack( cothread, fp, 1 + argcount ) + 1, argcount };
}

stack_values call_frame( frame* frame, value function )
{
    vm_context* vm = (vm_context*)frame->sp;
    cothread_object* cothread = vm->cothreads->back();

    vm_stack_frame* stack_frame = &cothread->stack_frames.back();
    assert( ! stack_frame->function );
    assert( stack_frame->fp == frame->fp );
    assert( stack_frame->fp < cothread->xp );

    unsigned xp = cothread->xp - frame->fp;
    value* r = cothread->stack.data() + frame->fp;
    r[ 0 ] = function;

    if ( ! box_is_object( function ) ) throw std::exception();
    type_code type = header( unbox_object( function ) )->type;
    if ( type == FUNCTION_OBJECT )
    {
        function_object* call_function = (function_object*)unbox_object( function );
        program_object* call_program = read( call_function->program );
        if ( ( call_program->code_flags & CODE_GENERATOR ) == 0 )
        {
            vm_exstate state = vm_call( vm, call_function, 0, xp );
            vm_execute( vm, state );
        }
        else
        {
            vm_exstate state = vm_call_generator( vm, call_function, 0, xp );
            assert( state.function == nullptr );
        }
    }
    else if ( type == NATIVE_FUNCTION_OBJECT )
    {
        native_function_object* call_function = (native_function_object*)unbox_object( function );
        vm_exstate state = vm_call_native( vm, call_function, 0, xp );
        assert( state.function == nullptr );
    }
    else if ( type == COTHREAD_OBJECT )
    {
        cothread_object* call_cothread = (cothread_object*)unbox_object( function );
        vm_exstate state = vm_call_cothread( vm, call_cothread, 0, xp );
        vm_execute( vm, state );
    }
    else
    {
        throw std::exception();
    }

    assert( vm->cothreads->back() == cothread );
    assert( frame->fp <= cothread->xp );
    return { cothread->stack.data() + frame->fp, cothread->xp - frame->fp };
}

void pop_frame( frame* frame )
{
    vm_context* vm = (vm_context*)frame->sp;
    cothread_object* cothread = vm->cothreads->back();

    vm_stack_frame* stack_frame = &cothread->stack_frames.back();
    assert( ! stack_frame->function );
    assert( stack_frame->fp == frame->fp );
    assert( stack_frame->fp <= cothread->xp );

    cothread->stack_frames.pop_back();
    cothread->xp = frame->fp;
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
    value result = resframe.count ? resframe.values[ 0 ] : boxed_null;
    return result;
}

value call( value function, std::initializer_list< value > arguments )
{
    return call( function, arguments.begin(), arguments.size() );
}

/*
    Exceptions.
*/

static char* format_message( const char* format, va_list ap )
{
    va_list aq;
    va_copy( aq, ap );
    int size = vsnprintf( nullptr, 0, format, aq );
    va_end( aq );
    char* message = (char*)malloc( size + 1 );
    message[ size ] = '\0';
    va_copy( aq, ap );
    vsnprintf( message, size, format, aq );
    va_end( aq );
    return message;
}

static char* KF_PRINTF_FORMAT( 1, 2 ) format_message( const char* format, ... )
{
    va_list ap;
    va_start( ap, format );
    char* message = format_message( format, ap );
    va_end( ap );
    return message;
}

static char* format_value( value v )
{
    if ( box_is_number( v ) )
    {
        return format_message( "%f", unbox_number( v ) );
    }
    else if ( box_is_string( v ) )
    {
        string_object* s = unbox_string( v );
        char* message = (char*)malloc( s->size + 1 );
        memcpy( message, s->text, s->size + 1 );
        return message;
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
        default: break;
        }
        return format_message( "<%s %p>", type_name, unbox_object( v ) );
    }
    else if ( box_is_bool( v ) )
    {
        return format_message( "%s", v.v == boxed_true.v ? "true" : "false" );
    }
    else
    {
        return format_message( "null" );
    }
}

exception::exception()
    :   _message( nullptr )
{
}

exception::exception( const char* format, ... )
{
    va_list ap;
    va_start( ap, format );
    _message = format_message( format, ap );
    va_end( ap );
}

exception::exception( const exception& e )
{
    size_t size = strlen( e._message );
    _message = (char*)malloc( size + 1 );
    memcpy( _message, e._message, size + 1 );
}

exception& exception::operator = ( const exception& e )
{
    free( _message );
    size_t size = strlen( e._message );
    _message = (char*)malloc( size + 1 );
    memcpy( _message, e._message, size + 1 );
    return *this;
}

exception::~exception()
{
    free( _message );
}

const char* exception::what() const noexcept
{
    return _message;
}

script_error::script_error( struct value v )
    :   _value( retain( v ) )
{
    _message = format_value( v );
}

script_error::script_error( const script_error& e )
    :   exception( e )
    ,   _value( retain( e._value ) )
{
}

script_error& script_error::operator = ( const script_error& e )
{
    exception::operator = ( e );
    release( _value );
    _value = retain( e._value );
    return *this;
}

script_error::~script_error()
{
    release( _value );
}

value script_error::value() const noexcept
{
    return _value;
}

type_error::type_error( const char* format, ... )
{
    va_list ap;
    va_start( ap, format );
    _message = format_message( format, ap );
    va_end( ap );
}

type_error::type_error( const type_error& e )
    :   exception( e )
{
}

type_error& type_error::operator = ( const type_error& e )
{
    exception::operator = ( e );
    return *this;
}

type_error::~type_error()
{
}

}

