//
//  string_object.h
//
//  Created by Edmund Kapusniak on 25/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KF_STRING_OBJECT_H
#define KF_STRING_OBJECT_H

/*
    This is a UTF-8 string.  Strings store their length explicitly, and are
    also null-terminated.  This makes our lives easier.
*/

#include <stddef.h>
#include <functional>
#include <string_view>
#include "object_model.h"

namespace kf
{

/*
    Key used in the hash table for the key map.
*/

struct string_hashkey
{
    size_t hash;
    size_t size;
    const char* text;
};

inline bool operator == ( const string_hashkey& a, const string_hashkey& b )
{
    return a.hash == b.hash && a.size == b.size && memcmp( a.text, b.text, a.size ) == 0;
}

inline bool operator != ( const string_hashkey& a, const string_hashkey& b )
{
    return ! operator == ( a, b );
}

/*
    String object structure.
*/

struct string_object : public object
{
    size_t size;
    char text[];
};

/*
    String functions.
*/

string_object* string_new( vm_context* vm, size_t size );
size_t string_hash( vm_context* vm, string_object* string );
string_object* string_key( vm_context* vm, string_object* string );
string_object* string_key( vm_context* vm, const char* text, size_t size );

/*
    Inline functions.
*/

inline size_t string_hash( vm_context* vm, string_object* string )
{
    return std::hash< std::string_view >()( std::string_view( string->text, string->size ) );
}

inline string_object* string_key( vm_context* vm, string_object* string )
{
    extern string_object* string_key_internal( vm_context* vm, string_object* string );
    if ( header( string )->flags & FLAG_KEY )
        return string;
    else
        return string_key_internal( vm, string );
}

}

template <> struct std::hash< kf::string_hashkey >
{
    size_t operator () ( const kf::string_hashkey& k ) { return k.hash; }
};

#endif
