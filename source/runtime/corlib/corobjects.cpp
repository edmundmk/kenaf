//
//  corobjects.cpp
//
//  Created by Edmund Kapusniak on 03/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "corobjects.h"
#include <stdlib.h>
#include <cmath>
#include "../objects/array_object.h"
#include "../objects/table_object.h"
#include "../objects/cothread_object.h"

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
        def insert( i, v ) end
        def remove( i ) end
        def pop() end
        def clear() end
    end

    def table is object
        def has( k ) end
        def get( k, default/null ) end
        def del( k ) end
        def clear() end
    end

    def function is object end

    def cothread is object
        def done() end
    end

    def u64val is object end

*/

static result superof( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    vmachine* vm = (vmachine*)cookie;
    return return_value( frame, box_object( value_superof( vm, arguments[ 0 ] ) ) );
}

static result getkey( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    vmachine* vm = (vmachine*)cookie;
    if ( ! box_is_string( arguments[ 1 ] ) ) raise_type_error( arguments[ 1 ], "a string" );
    selector sel = {};
    string_object* key = string_key( vm, unbox_string( arguments[ 1 ] ) );
    return return_value( frame, lookup_getkey( vm, value_keyerof( vm, arguments[ 0 ] ), key, &sel ) );
}

static result setkey( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    vmachine* vm = (vmachine*)cookie;
    if ( ! box_is_string( arguments[ 1 ] ) ) raise_type_error( arguments[ 1 ], "a string" );
    if ( ! box_is_object_type( arguments[ 0 ], LOOKUP_OBJECT ) ) raise_type_error( arguments[ 0 ], "a lookup object" );
    selector sel = {};
    string_object* key = string_key( vm, unbox_string( arguments[ 1 ] ) );
    lookup_setkey( vm, (lookup_object*)unbox_object( arguments[ 0 ] ), key, &sel, arguments[ 2 ] );
    return return_void( frame );
}

static result haskey( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    vmachine* vm = (vmachine*)cookie;
    if ( ! box_is_string( arguments[ 1 ] ) ) raise_type_error( arguments[ 1 ], "a string" );
    if ( ! box_is_object_type( arguments[ 0 ], LOOKUP_OBJECT ) ) return return_value( frame, false_value );
    string_object* key = string_key( vm, unbox_string( arguments[ 1 ] ) );
    return return_value( frame, lookup_haskey( vm, (lookup_object*)unbox_object( arguments[ 0 ] ), key ) ? true_value : false_value );
}

static result delkey( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    vmachine* vm = (vmachine*)cookie;
    if ( ! box_is_string( arguments[ 1 ] ) ) raise_type_error( arguments[ 1 ], "a string" );
    if ( ! box_is_object_type( arguments[ 0 ], LOOKUP_OBJECT ) ) return return_void( frame );
    string_object* key = string_key( vm, unbox_string( arguments[ 1 ] ) );
    lookup_delkey( vm, (lookup_object*)unbox_object( arguments[ 0 ] ), key );
    return return_void( frame );
}

static result number_self( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    value v = arguments[ 1 ];
    if ( ! box_is_number( v ) )
    {
        if ( box_is_string( v ) )
        {
            string_object* s = unbox_string( v );
            char* endptr = nullptr;
            double n = strtod( s->text, &endptr );
            if ( *endptr != '\0' ) raise_error( ERROR_TYPE, "string cannot be converted to number" );
            if ( n != n ) n = NAN;
            v = box_number( n );
        }
        else if ( box_is_bool( v ) )
        {
            v = box_number( v.v != false_value.v ? 1.0 : 0.0 );
        }
        else
        {
            raise_type_error( v, "convertible to a number" );
        }
    }
    return return_value( frame, v );
}

static result string_self( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    vmachine* vm = (vmachine*)cookie;
    value v = arguments[ 1 ];
    if ( ! box_is_string( v ) )
    {
        if ( box_is_number( v ) )
        {
            double n = unbox_number( v );
            int size = snprintf( nullptr, 0, "%f", n );
            string_object* s = string_new( vm, nullptr, size );
            snprintf( s->text, s->size, "%f", n );
            v = box_string( s );
        }
        else if ( box_is_bool( v ) )
        {
            std::string_view s = v.v != false_value.v ? "true" : "false";
            v = box_string( string_new( vm, s.data(), s.size() ) );
        }
        else
        {
            raise_type_error( v, "convertible to a string" );
        }
    }
    return return_value( frame, v );
}

static inline size_t array_index( value v )
{
    if ( ! box_is_number( v ) ) raise_type_error( v, "a number" );
    return (size_t)(intptr_t)unbox_number( v );
}

static result array_resize( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    value a = arguments[ 0 ];
    if ( ! box_is_object_type( a, ARRAY_OBJECT ) ) raise_type_error( a, "an array" );
    array_resize( (vmachine*)cookie, (array_object*)unbox_object( a ), array_index( arguments[ 1 ] ) );
    return return_void( frame );
}

static result array_append( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    value a = arguments[ 0 ];
    if ( ! box_is_object_type( a, ARRAY_OBJECT ) ) raise_type_error( a, "an array" );
    value v = array_append( (vmachine*)cookie, (array_object*)unbox_object( a ), arguments[ 1 ] );
    return return_value( frame, v );
}

static result array_extend( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    value a = arguments[ 0 ];
    if ( ! box_is_object_type( a, ARRAY_OBJECT ) ) raise_type_error( a, "an array" );
    array_extend( (vmachine*)cookie, (array_object*)unbox_object( a ), arguments + 1, argcount - 1 );
    return return_void( frame );
}

static result array_insert( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    value a = arguments[ 0 ];
    if ( ! box_is_object_type( a, ARRAY_OBJECT ) ) raise_type_error( a, "an array" );
    value v = array_insert( (vmachine*)cookie, (array_object*)unbox_object( a ), array_index( arguments[ 1 ] ), arguments[ 2 ] );
    return return_value( frame, v );
}

static result array_remove( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    value a = arguments[ 0 ];
    if ( ! box_is_object_type( a, ARRAY_OBJECT ) ) raise_type_error( a, "an array" );
    value v = array_remove( (vmachine*)cookie, (array_object*)unbox_object( a ), array_index( arguments[ 1 ] ) );
    return return_value( frame, v );
}

static result array_pop( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    value a = arguments[ 0 ];
    if ( ! box_is_object_type( a, ARRAY_OBJECT ) ) raise_type_error( a, "an array" );
    array_object* array = (array_object*)unbox_object( a );
    if ( array->length == 0 ) raise_error( ERROR_INDEX, "array is empty" );
    return return_value( frame, read( read( array->aslots )->slots[ --array->length ] ) );
}

static result array_clear( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    value a = arguments[ 0 ];
    if ( ! box_is_object_type( a, ARRAY_OBJECT ) ) raise_type_error( a, "an array" );
    array_object* array = (array_object*)unbox_object( a );
    array_clear( (vmachine*)cookie, array );
    return return_void( frame );
}

static result table_has( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    value t = arguments[ 0 ];
    if ( ! box_is_object_type( t, TABLE_OBJECT ) ) raise_type_error( t, "a table" );
    table_object* table = (table_object*)unbox_object( t );
    return return_value( frame, bool_value( table_tryindex( (vmachine*)cookie, table, arguments[ 1 ], nullptr ) ) );
}

static result table_get( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    value t = arguments[ 0 ];
    if ( argcount > 3 ) raise_error( ERROR_ARGUMENT, "too few arguments, expected 2 or 3, not %zu", argcount );
    if ( ! box_is_object_type( t, TABLE_OBJECT ) ) raise_type_error( t, "a table" );
    value v;
    table_object* table = (table_object*)unbox_object( t );
    if ( ! table_tryindex( (vmachine*)cookie, table, arguments[ 1 ], &v ) )
    {
        v = argcount >= 2 ? arguments[ 2 ] : null_value;
    }
    return return_value( frame, v );
}

static result table_del( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    value t = arguments[ 0 ];
    if ( ! box_is_object_type( t, TABLE_OBJECT ) ) raise_type_error( t, "a table" );
    table_object* table = (table_object*)unbox_object( t );
    table_delindex( (vmachine*)cookie, table, arguments[ 1 ] );
    return return_void( frame );
}

static result table_clear( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    value t = arguments[ 0 ];
    if ( ! box_is_object_type( t, TABLE_OBJECT ) ) raise_type_error( t, "a table" );
    table_object* table = (table_object*)unbox_object( t );
    table_clear( (vmachine*)cookie, table );
    return return_void( frame );
}

static result cothread_done( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    value c = arguments[ 0 ];
    if ( ! box_is_object_type( c, COTHREAD_OBJECT ) ) raise_type_error( c, "a cothread" );
    cothread_object* cothread = (cothread_object*)unbox_object( c );
    return return_value( frame, cothread->stack_frames.empty() ? true_value : false_value );
}

void expose_corobjects( vmachine* vm )
{
    value global = global_object();
    set_key( global, "global", global );

    set_key( global, "superof", create_function( "superof", superof, vm, 1 ) );
    set_key( global, "getkey", create_function( "getkey", getkey, vm, 2 ) );
    set_key( global, "setkey", create_function( "setkey", setkey, vm, 3 ) );
    set_key( global, "haskey", create_function( "haskey", haskey, vm, 2 ) );
    set_key( global, "delkey", create_function( "delkey", delkey, vm, 2 ) );

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
        set_key( box_object( proto_number ), "self", create_function( "number.self", number_self, vm, 2, FUNCTION_DIRECT ) );
        lookup_seal( vm, proto_number );
    }

    lookup_object* proto_string = vm->prototypes[ STRING_OBJECT ];
    set_key( global, "string", box_object( proto_string ) );
    if ( ! lookup_sealed( vm, proto_string ) )
    {
        set_key( box_object( proto_string ), "self", create_function( "string.self", string_self, vm, 2, FUNCTION_DIRECT ) );
        lookup_seal( vm, proto_string );
    }

    lookup_object* proto_array = vm->prototypes[ ARRAY_OBJECT ];
    set_key( global, "array", box_object( proto_array ) );
    if ( ! lookup_sealed( vm, proto_array ) )
    {
        set_key( box_object( proto_array ), "resize", create_function( "array.resize", array_resize, vm, 2 ) );
        set_key( box_object( proto_array ), "append", create_function( "array.append", array_append, vm, 2 ) );
        set_key( box_object( proto_array ), "extend", create_function( "array.extend", array_extend, vm, 1, FUNCTION_VARARG ) );
        set_key( box_object( proto_array ), "insert", create_function( "array.insert", array_insert, vm, 3 ) );
        set_key( box_object( proto_array ), "remove", create_function( "array.remove", array_remove, vm, 2 ) );
        set_key( box_object( proto_array ), "pop", create_function( "array.pop", array_pop, vm, 1 ) );
        set_key( box_object( proto_array ), "clear", create_function( "array.clear", array_clear, vm, 1 ) );
        lookup_seal( vm, proto_array );
    }

    lookup_object* proto_table = vm->prototypes[ TABLE_OBJECT ];
    set_key( global, "table", box_object( proto_table ) );
    if ( ! lookup_sealed( vm, proto_table ) )
    {
        set_key( box_object( proto_table ), "has", create_function( "table.has", table_has, vm, 2 ) );
        set_key( box_object( proto_table ), "get", create_function( "table.get", table_get, vm, 2, FUNCTION_VARARG ) );
        set_key( box_object( proto_table ), "del", create_function( "table.del", table_del, vm, 2 ) );
        set_key( box_object( proto_table ), "clear", create_function( "table.clear", table_clear, vm, 1 ) );
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
        set_key( box_object( proto_cothread ), "done", create_function( "cothread.done", cothread_done, vm, 1 ) );
        lookup_sealed( vm, proto_cothread );
    }

    lookup_object* proto_u64val = vm->prototypes[ U64VAL_OBJECT ];
    set_key( global, "u64val", box_object( proto_u64val ) );
    if ( ! lookup_sealed( vm, proto_u64val ) )
    {
        lookup_seal( vm, proto_u64val );
    }
}

}

