//
//  kenaf.h
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


#ifndef KENAF_H
#define KENAF_H

#include <stdint.h>
#include <exception>
#include <string_view>

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

runtime* create_runtime();
runtime* retain_runtime( runtime* r );
void release_runtime( runtime* r );

/*
    A context consists of a runtime and a global object.  Contexts on the same
    runtime can share values.
*/

struct context;

context* create_context( runtime* r );
context* retain_context( context* c );
void release_context( context* c );

context* make_current( context* c );
value global_object();

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
    NUMBER,
    BOOL_VALUE,
    NULL_VALUE,
};

value retain( value v );
void release( value v );

value_kind kindof( value v );
bool is_lookup( value v );
bool is_string( value v );
bool is_array( value v );
bool is_table( value v );
bool is_function( value v );
bool is_cothread( value v );
bool is_number( value v );
bool is_bool( value v );
bool is_null( value v );

value null_value();
value true_value();
value false_value();

value superof( value v );
bool test( value v );

value bool_value( bool b );
bool get_bool( value v );

value number_value( double n );
double get_number( value v );

value create_lookup();
value create_lookup( value prototype );
value get_key( value lookup, std::string_view k );
void set_key( value lookup, std::string_view k, value v );
bool has_key( value lookup, std::string_view k );
void del_key( value lookup, std::string_view k );

value create_string( std::string_view text );
value create_string_buffer( size_t size, char** out_text );
std::string_view get_text( value string );

value create_array();
value create_array( size_t capacity );
size_t array_length( value array );
void array_resize( value array, size_t length );
void array_append( value array, value v );
void array_clear( value array );

value create_table();
value create_table( size_t capacity );
size_t table_length( value table );
void table_clear( value table );

value get_index( value table, value k );
value get_index( value array, size_t index );
void set_index( value table, value k, value v );
void set_index( value array, size_t index, value v );
void del_index( value table, value k );

value create_function( const void* code, size_t size );

/*
    Native function interface.
*/

struct frame;

typedef size_t (*native_function)( void* cookie, frame* frame, const value* arguments, size_t argcount );

enum
{
    PARAM_VARARG = 1 << 0,
};

value create_function( native_function native, void* cookie, unsigned param_count, unsigned code_flags = 0 );

value* arguments( frame* frame );
value* results( frame* frame, size_t count );
size_t result( frame* frame, value v );
size_t rvoid( frame* frame );

/*
    Calling functions from native code.
*/

struct stack_values { value* values; size_t count; };

stack_values stack_push( size_t count );
stack_values stack_call( value function );
void stack_pop();

value call( value function, const value* arguments, size_t argcount );
value call( value function, std::initializer_list< value > arguments );

template < typename ... A > value call( value function, A ... argvalues )
{
    return call( function, { std::forward< A >( argvalues ) ... } );
}

/*
    Exception thrown from script code.
*/

class exception : public std::exception
{
};

}

#endif

