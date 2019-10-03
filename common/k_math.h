//
//  k_math.h
//
//  Created by Edmund Kapusniak on 03/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef K_MATH_H
#define K_MATH_H

#include <stdint.h>
#include <cmath>

inline double k_floordiv( double u, double v )
{
    return std::floor( u / v );
}

inline double k_floormod( double u, double v )
{
    double x = std::fmod( u, v );
    return ( x < 0.0 ) == ( v < 0.0 ) ? x : x + v;
}

inline uint32_t k_bitint( double u )
{
    return (uint32_t)(int64_t)u;
}

inline double k_bitnot( double u )
{
    return ~ k_bitint( u );
}

inline double k_lshift( double u, double v )
{
    uint32_t amount = k_bitint( v );
    return amount < 32 ? k_bitint( u ) << amount : 0;
}

inline double k_rshift( double u, double v )
{
    uint32_t amount = k_bitint( v );
    return amount < 32 ? k_bitint( v ) >> amount : 0;
}

inline double k_ashift( double u, double v )
{
    uint32_t amount = k_bitint( v );
    return amount < 32 ? (uint32_t)( (int32_t) k_bitint( v ) >> amount ) : ~(uint32_t)0;
}

inline double k_bitand( double u, double v )
{
    return k_bitint( u ) & k_bitint( v );
}

inline double k_bitxor( double u, double v )
{
    return k_bitint( u ) ^ k_bitint( v );
}

inline double k_bitor( double u, double v )
{
    return k_bitint( u ) | k_bitint( v );
}

#endif

