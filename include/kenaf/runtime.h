//
//  runtime.h
//
//  Created by Edmund Kapusniak on 01/11/2019
//  Copyright © 2019 Edmund Kapusniak.
//
//  Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files (the "Software"),
//  to deal in the Software without restriction, including without limitation
//  the rights to use, copy, modify, merge, publish, distribute, sublicense,
//  and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//  DEALINGS IN THE SOFTWARE.
//

#ifndef KENAF_RUNTIME_H
#define KENAF_RUNTIME_H

#include <stdint.h>
#include <exception>
#include <string_view>
#include "defines.h"

namespace kf
{

/*
    Value structure.
*/

struct value { uint64_t v; };

/*
    A runtime manages the global state of a virtual machine, including the
    garbage collected heap.  Only one thread can be using a runtime at a time.
*/

struct runtime;

KF_API runtime* create_runtime();
KF_API runtime* retain_runtime( runtime* r );
KF_API void release_runtime( runtime* r );

/*
    A context consists of a runtime and a global object.  Contexts on the same
    runtime can share values.
*/

struct context;

KF_API context* create_context( runtime* r );
KF_API context* retain_context( context* c );
KF_API void release_context( context* c );

KF_API context* make_current( context* c );
KF_API value global_object();

/*
    Values.
*/

enum value_kind
{
    LOOKUP,
    STRING,
    ARRAY,
    TABLE,
    FUNCTION,
    COTHREAD,
    U64VAL,
    NUMBER,
    BOOL_VALUE,
    NULL_VALUE,
};

KF_API value retain( value v );
KF_API void release( value v );

KF_API value_kind kindof( value v );
KF_API bool is_lookup( value v );
KF_API bool is_string( value v );
KF_API bool is_array( value v );
KF_API bool is_table( value v );
KF_API bool is_function( value v );
KF_API bool is_cothread( value v );
KF_API bool is_u64val( value v );
KF_API bool is_number( value v );
KF_API bool is_bool( value v );
KF_API bool is_null( value v );

KF_API value null_value();
KF_API value true_value();
KF_API value false_value();

KF_API value superof( value v );
KF_API bool test( value v );

KF_API value bool_value( bool b );
KF_API bool get_bool( value v );

KF_API value number_value( double n );
KF_API double get_number( value v );

KF_API value u64val_value( uint64_t u );
KF_API uint64_t get_u64val( value v );

KF_API value create_lookup();
KF_API value create_lookup( value prototype );
KF_API value get_key( value lookup, std::string_view k );
KF_API void set_key( value lookup, std::string_view k, value v );
KF_API bool has_key( value lookup, std::string_view k );
KF_API void del_key( value lookup, std::string_view k );

KF_API value create_string( std::string_view text );
KF_API value create_string_buffer( size_t size, char** out_text );
KF_API std::string_view get_text( value string );

KF_API value create_array();
KF_API value create_array( size_t capacity );
KF_API size_t array_length( value array );
KF_API void array_resize( value array, size_t length );
KF_API void array_append( value array, value v );
KF_API void array_clear( value array );

KF_API value create_table();
KF_API value create_table( size_t capacity );
KF_API size_t table_length( value table );
KF_API void table_clear( value table );

KF_API value get_index( value table, value k );
KF_API value get_index( value array, size_t index );
KF_API void set_index( value table, value k, value v );
KF_API void set_index( value array, size_t index, value v );
KF_API void del_index( value table, value k );

KF_API value create_function( const void* code, size_t size );

/*
    Native function interface.
*/

struct frame { void* sp; size_t fp; };
typedef size_t (*native_function)( void* cookie, frame* frame, const value* arguments, size_t argcount );

enum { PARAM_VARARG = 1 << 0 };
KF_API value create_function( std::string_view name, native_function native, void* cookie, unsigned param_count, unsigned code_flags = 0 );

KF_API value* arguments( frame* frame );
KF_API value* results( frame* frame, size_t count );
KF_API size_t result( frame* frame, value v );
KF_API size_t rvoid( frame* frame );

/*
    Calling functions from native code.
*/

struct KF_API stack_values { value* values; size_t count; };
KF_API stack_values push_frame( frame* frame, size_t argcount );
KF_API stack_values call_frame( frame* frame, value function );
KF_API void pop_frame( frame* frame );

KF_API value call( value function, const value* arguments, size_t argcount );
KF_API value call( value function, std::initializer_list< value > arguments );

template < typename ... A > value call( value function, A ... argvalues )
{
    return call( function, { std::forward< A >( argvalues ) ... } );
}

}

#endif

