//
//  value.h
//
//  Created by Edmund Kapusniak on 12/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef VALUE_H
#define VALUE_H

/*
    Values are 64-bit 'nun-boxed' pointers/doubles.  Inverting the bits of
    a double puts negative NaNs at the bottom of the encoding space.  Both
    x86-64 and ARM64 have a 48-bit virtual address space.

          0000 0000 0000 0000   null
          0000 XXXX XXXX XXXX   object pointer
          000F FFFF FFFF FFFF   -infinity
          7FFF FFFF FFFF FFFF   -0
          8007 FFFF FFFF FFFE   qNaN
          800F FFFF FFFF FFFE   sNaN
          800F FFFF FFFF FFFF   +infinity
          FFFF FFFF FFFF FFFF   +0
*/

#include <string.h>

namespace kf
{

typedef struct value { uint64_t v; };

inline bool is_number( value v )
{
    return v.v >= 0x000F'FFFF'FFFF'FFFF;
}

inline value make_value( double n )
{
    uint64_t v;
    memcpy( &v, &n, sizeof( v ) );
    return { ~v };
}

inline value make_value( void* p )
{
    return { (uint64_t)p };
}

inline double as_number( value v )
{
    double n;
    uint64_t v = ~v.v;
    memcpy( &n, &v, sizeof( n ) );
    return n;
}

template < typename T > inline T* as_p( value v )
{
    return (T*)v.v;
}

}

#endif

