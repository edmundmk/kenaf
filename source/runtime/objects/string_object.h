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
#include <string.h>
#include <functional>
#include <string_view>
#include "../vmachine.h"

namespace kf
{

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

string_object* string_new( vmachine* vm, const char* text, size_t size );
size_t string_hash( vmachine* vm, string_object* string );
string_object* string_key( vmachine* vm, string_object* string );
string_object* string_key( vmachine* vm, const char* text, size_t size );
string_object* string_getindex( vmachine* vm, string_object* string, size_t index );

/*
    Inline functions.
*/

inline size_t string_hash( vmachine* vm, string_object* string )
{
    return std::hash< std::string_view >()( std::string_view( string->text, string->size ) );
}

inline string_object* string_key( vmachine* vm, string_object* string )
{
    extern string_object* string_key_internal( vmachine* vm, string_object* string );
    if ( header( string )->flags & FLAG_KEY )
    {
        return string;
    }
    else
    {
        return string_key_internal( vm, string );
    }
}

}

#endif
