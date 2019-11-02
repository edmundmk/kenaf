//
//  kenaf.h
//
//  Created by Edmund Kapusniak on 01/11/2019
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KENAF_H
#define KENAF_H

#include <stdint.h>
#include <exception>
#include <string_view>

namespace kf
{

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

struct value { uint64_t v; };

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

value number_value( double n );
double get_number( value v );

value create_lookup();
value create_lookup( value prototype );
value get_key( value lookup, std::string_view k );
void set_key( value lookup, std::string_view k, value v );
bool has_key( value lookup, std::string_view k );
void del_key( value lookup, std::string_view k );

value create_string( std::string_view text );
value create_string_buffer( size_t size );
value update_string_buffer( value string, size_t index, const char* text, size_t size );
std::string_view get_text( value string );

value create_array();
value create_array( size_t capacity );
size_t array_length( value array );
void array_resize( value array, size_t length );
void array_append( value array, value v );
void array_clear( value array );

value get_index( value array, size_t index );
void set_index( value array, size_t index, value v );

value create_table();
value create_table( size_t capacity );
size_t table_length( value table );
void table_clear( value table );

value get_index( value table, value k );
void set_index( value table, value k, value v );
void del_index( value table, value k );

value create_function( const void* code, size_t size );

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
    Call stack.  This is how native code interacts with script code.
*/




/*
    Exception thrown from script code.
*/

class exception : public std::exception
{
};

}

#endif

