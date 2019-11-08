//
//  hashkeys.h
//
//  Created by Edmund Kapusniak on 07/11/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KF_HASHKEYS_H
#define KF_HASHKEYS_H

/*
    Keys used in execution environment hash tables.
*/

#include <string.h>
#include <functional>

namespace kf
{

struct layout_object;
struct string_object;

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

struct layout_hashkey
{
    layout_object* layout;
    string_object* key;
};

inline bool operator == ( const layout_hashkey& a, const layout_hashkey& b )
{
    return a.layout == b.layout && a.key == b.key;
}

inline bool operator != ( const layout_hashkey& a, const layout_hashkey& b )
{
    return ! operator == ( a, b );
}

}

template <> struct std::hash< kf::string_hashkey >
{
    size_t operator () ( const kf::string_hashkey& k ) { return k.hash; }
};

template <> struct std::hash< kf::layout_hashkey > : private std::hash< std::string_view >
{
    size_t operator () ( const kf::layout_hashkey& k ) { return std::hash< std::string_view >::operator () ( std::string_view( (char*)&k, sizeof( k ) ) ); }
};

#endif

