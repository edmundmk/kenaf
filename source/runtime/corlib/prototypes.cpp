//
//  prototypes.cpp
//
//  Created by Edmund Kapusniak on 03/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "prototypes.h"
#include <stdlib.h>
#include <cmath>
#include "kenaf/runtime.h"
#include "../objects/array_object.h"
#include "../objects/cothread_object.h"
#include "../vm/vm_context.h"

namespace kf
{

/*

    global

    def superof( v ) end
    def getkey( o, key ) end
    def setkey( o, key ) end
    def haskey( o, key ) end
    def delkey( o, key ) end
    def keys( object ) end

    def object end

    def bool is object end

    def number is object
        def self( o ) return to_number( o ) end
    end

    def string is object
        def self( o ) return to_string( o ) end
    end

    def array is object
        def resize( n ) end
        def append( n ) end
        def extend( x ... ) end
        def pop() end
    end

    def table is object
        def has( k ) end
        def get( k, default/null ) end
        def del( k ) end
    end

    def function is object end

    def cothread is object
        def done() end
    end

*/

static size_t superof( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    return result( frame, superof( arguments[ 0 ] ) );
}

static size_t getkey( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    extern value get_key( value lookup, value key );
    return result( frame, get_key( arguments[ 0 ], arguments[ 1 ] ) );
}

static size_t setkey( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    extern void set_key( value lookup, value key, value v );
    set_key( arguments[ 0 ], arguments[ 1 ], arguments[ 2 ] );
    return rvoid( frame );
}

static size_t haskey( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    extern bool has_key( value lookup, value key );
    return result( frame, bool_value( has_key( arguments[ 0 ], arguments[ 1 ] ) ) );
}

static size_t delkey( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    extern void del_key( value lookup, value key );
    del_key( arguments[ 0 ], arguments[ 1 ] );
    return rvoid( frame );
}

static size_t number_self( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    value v = arguments[ 0 ];
    if ( ! is_number( v ) )
    {
        if ( is_string( v ) )
        {
            std::string_view text = get_text( v );
            char* endptr = nullptr;
            double n = strtod( text.data(), &endptr );
            if ( *endptr != '\0' ) throw std::exception();
            if ( n != n ) n = NAN;
            v = number_value( n );
        }
        else if ( is_bool( v ) )
        {
            v = number_value( get_bool( v ) ? 1.0 : 0.0 );
        }
        else
        {
            throw std::exception();
        }
    }
    return result( frame, v );
}

static size_t string_self( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    value v = arguments[ 0 ];
    if ( ! is_string( v ) )
    {
        if ( is_number( v ) )
        {
            double n = get_number( v );
            int size = snprintf( nullptr, 0, "%f", n );
            char* text = nullptr;
            v = create_string_buffer( size, &text );
            snprintf( text, size, "%f", n );
        }
        else if ( is_bool( v ) )
        {
            if ( get_bool( v ) )
                v = create_string( "true" );
            else
                v = create_string( "false" );
        }
        else
        {
            throw std::exception();
        }
    }
    return result( frame, v );
}

vm_context* current_context();

static size_t array_resize( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    value a = arguments[ 0 ];
    value n = arguments[ 1 ];
    if ( ! is_array( a ) ) throw std::exception();
    if ( ! is_number( n ) ) throw std::exception();
    array_resize( current_context(), (array_object*)unbox_object( a ), (size_t)(uint64_t)get_number( n ) );
    return rvoid( frame );
}

static size_t array_append( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    value a = arguments[ 0 ];
    if ( ! is_array( a ) ) throw std::exception();
    array_append( current_context(), (array_object*)unbox_object( a ), arguments[ 1 ] );
    return rvoid( frame );
}

static size_t array_extend( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    value a = arguments[ 0 ];
    if ( ! is_array( a ) ) throw std::exception();
    array_extend( current_context(), (array_object*)unbox_object( a ), arguments + 1, argcount - 1 );
    return rvoid( frame );
}

static size_t array_pop( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    value a = arguments[ 0 ];
    if ( ! is_array( a ) ) throw std::exception();
    array_object* array = (array_object*)unbox_object( a );
    if ( array->length == 0 ) throw std::exception();
    return result( frame, read( read( array->aslots )->slots[ --array->length ] ) );
}

static size_t table_has( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
}

static size_t table_get( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
}

static size_t table_del( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
}

static size_t cothread_done( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    value c = arguments[ 0 ];
    if ( ! is_cothread( c ) ) throw std::exception();
    cothread_object* cothread = (cothread_object*)unbox_object( c );
    return result( frame, bool_value( cothread->stack_frames.empty() ) );
}

void expose_prototypes( vm_context* vm )
{
    value global = global_object();
    set_key( global, "global", global );

    set_key( global, "superof", create_function( superof, nullptr, 1 ) );
    set_key( global, "getkey", create_function( getkey, nullptr, 2 ) );
    set_key( global, "setkey", create_function( setkey, nullptr, 3 ) );
    set_key( global, "haskey", create_function( haskey, nullptr, 2 ) );
    set_key( global, "delkey", create_function( delkey, nullptr, 2 ) );

    lookup_object* proto_object = vm->prototypes[ LOOKUP_OBJECT ];
    set_key( global, "object", box_object( proto_object ) );
    if ( ! lookup_sealed( vm, proto_object ) )
    {
        lookup_seal( vm, proto_object );
    }

    lookup_object* proto_bool = vm->prototypes[ BOOL_OBJECT ];
    set_key( global, "bool", box_object( proto_bool ) );
    if ( ! lookup_sealed( vm, proto_bool ) )
    {
        lookup_seal( vm, proto_bool );
    }

    lookup_object* proto_number = vm->prototypes[ NUMBER_OBJECT ];
    set_key( global, "number", box_object( proto_number ) );
    if ( ! lookup_sealed( vm, proto_number ) )
    {
        set_key( box_object( proto_number ), "self", create_function( number_self, nullptr, 2 ) );
        lookup_seal( vm, proto_number );
    }

    lookup_object* proto_string = vm->prototypes[ STRING_OBJECT ];
    set_key( global, "string", box_object( proto_string ) );
    if ( ! lookup_sealed( vm, proto_string ) )
    {
        set_key( box_object( proto_string ), "self", create_function( string_self, nullptr, 2 ) );
        lookup_seal( vm, proto_string );
    }

    lookup_object* proto_array = vm->prototypes[ ARRAY_OBJECT ];
    set_key( global, "array", box_object( proto_array ) );
    if ( ! lookup_sealed( vm, proto_array ) )
    {
        set_key( box_object( proto_array ), "resize", create_function( array_resize, nullptr, 2 ) );
        set_key( box_object( proto_array ), "append", create_function( array_append, nullptr, 2 ) );
        set_key( box_object( proto_array ), "extend", create_function( array_extend, nullptr, 1, PARAM_VARARG ) );
        set_key( box_object( proto_array ), "pop", create_function( array_pop, nullptr, 1 ) );
        lookup_seal( vm, proto_array );
    }

    lookup_object* proto_table = vm->prototypes[ TABLE_OBJECT ];
    set_key( global, "table", box_object( proto_table ) );
    if ( ! lookup_sealed( vm, proto_table ) )
    {
        set_key( box_object( proto_table ), "has", create_function( table_has, nullptr, 2 ) );
        set_key( box_object( proto_table ), "get", create_function( table_get, nullptr, 2, PARAM_VARARG ) );
        set_key( box_object( proto_table ), "del", create_function( table_del, nullptr, 2 ) );
        lookup_seal( vm, proto_table );
    }

    lookup_object* proto_function = vm->prototypes[ FUNCTION_OBJECT ];
    set_key( global, "function", box_object( proto_function ) );
    if ( ! lookup_sealed( vm, proto_function ) )
    {
        lookup_seal( vm, proto_function );
    }

    lookup_object* proto_cothread = vm->prototypes[ COTHREAD_OBJECT ];
    set_key( global, "cothread", box_object( proto_cothread ) );
    if ( ! lookup_sealed( vm, proto_cothread ) )
    {
        set_key( box_object( proto_cothread ), "done", create_function( cothread_done, nullptr, 1 ) );
        lookup_sealed( vm, proto_cothread );
    }
}

}
