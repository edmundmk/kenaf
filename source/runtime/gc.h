//
//  gc.h
//
//  Created by Edmund Kapusniak on 12/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef GC_H
#define GC_H

/*
    The garbage collector is a concurrent mark-sweep algorithm.
*/

#include "gcatomic.h"

namespace kf
{

struct value;

/*
    Each object in the GC heap is preceded by the following 32-bit gc header.
*/

enum
{
    GC_COLOR_NONE,
    GC_COLOR_ORANGE,
    GC_COLOR_PURPLE,
};

enum
{
    GC_FLAGS_NONE,
    GC_FLAGS_LEAF       = 1 << 0,
    GC_FLAGS_HASH       = 1 << 1,
    GC_FLAGS_KEY        = 1 << 2,
    GC_FLAGS_PROTOTYPE  = 1 << 3,
};

struct gc_header
{
    atomic_u8 color;
    uint8_t flags;
    uint8_t type_index;
    uint8_t ref_count;
};

/*
    A gcref pointer.
*/

template < typename T > using gc_ref = atomic_p< T >;
template < typename T > inline T* gc_read( const gc_ref< T >& r );
template < typename T > inline void gc_write( gc_ref< T >& r, T* v );

/*
    A gcvalue, which is either a boxed number or a gcref.
*/

using gc_value = atomic_u64;
inline value gc_read( const gc_value& r );
inline void gc_write( gc_value& r, value v );

}

#endif

