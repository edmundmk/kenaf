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
#include <string_view>

namespace kf
{

// ==== runtime ====

class runtime
{
};


// ==== context ====

class context
{
};


// ==== value ====

struct value { uint64_t v; };

enum value_kind
{
    LOOKUP,
    STRING,
    ARRAY,
    TABLE,
    FUNCTION,
    COTHREAD,
    NUMBER,
    BOOL,
    NULL,
};

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

value superof( value v );
bool test( value v );

value null_value();
value true_value();
value false_value();

value number_value( double n );
double get_number( value v );

value new_lookup();
value get_key( value lookup, std::string_view k );
void set_key( value lookup, std::string_view k, value v );
bool has_key( value lookup, std::string_view k );
bool del_key( value lookup, std::string_view k );

value new_string( std::string_view text );
std::string_view get_text( value string );

value new_array();
size_t array_length( value array );
void array_reserve( value array, size_t capacity );
void array_resize( value array, size_t length );
void array_append( value array, value v );
void array_clear( value array );

value get_index( value array, size_t index );
void set_index( value array, size_t index, value v );

value new_table();
size_t table_length( value table );
void table_reserve( value table, size_t capacity );

value get_index( value table, value k );
void set_index( value table, value k, value v );
void del_index( value table, value k );

value new_function( const void* code, size_t size );


// ==== call stack ===




// ==== exception ====

class exception : public std::exception
{
};


}

#endif

