//
//  string_object.h
//
//  Created by Edmund Kapusniak on 25/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "string_object.h"
#include "../vm_context.h"

namespace kf
{

string_object* string_new( vm_context* vm, const char* text, size_t size )
{
    string_object* string = new ( object_new( vm, STRING_OBJECT, sizeof( string_object ) + size ) ) string_object();
    string->size = size;
    if ( text )
    {
        memcpy( string->text, text, size );
    }
    return string;
}

string_object* string_key_internal( vm_context* vm, string_object* string )
{
    assert( ( header( string )->flags & FLAG_KEY ) == 0 );

    string_hashkey hashkey = { string_hash( vm, string ), string->size, string->text };
    auto i = vm->keys.find( hashkey );
    if ( i != vm->keys.end() )
    {
        return i->second;
    }

    header( string )->flags |= FLAG_KEY;
    vm->keys.insert_or_assign( hashkey, string );
    return string;
}

string_object* string_key( vm_context* vm, const char* text, size_t size )
{
    size_t hash = std::hash< std::string_view >()( std::string_view( text, size ) );
    string_hashkey hashkey = { hash, size, text };
    auto i = vm->keys.find( hashkey );
    if ( i != vm->keys.end() )
    {
        return i->second;
    }

    string_object* string = string_new( vm, text, size );
    hashkey = { hash, string->size, string->text };

    header( string )->flags |= FLAG_KEY;
    vm->keys.insert_or_assign( hashkey, string );
    return string;
}

}

