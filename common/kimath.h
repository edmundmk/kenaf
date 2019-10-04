//
//  kimath.h
//
//  Created by Edmund Kapusniak on 03/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KIMATH_H
#define KIMATH_H

#include <stdint.h>
#include <cmath>

inline double kifloordiv( double u, double v )
{
    return std::floor( u / v );
}

inline double kifloormod( double u, double v )
{
    double x = std::fmod( u, v );
    return ( x < 0.0 ) == ( v < 0.0 ) ? x : x + v;
}

inline uint32_t kibitint( double u )
{
    return (uint32_t)(int64_t)u;
}

inline double kibitnot( double u )
{
    return ~ kibitint( u );
}

inline double kilshift( double u, double v )
{
    uint32_t amount = kibitint( v );
    return amount < 32 ? kibitint( u ) << amount : 0;
}

inline double kirshift( double u, double v )
{
    uint32_t amount = kibitint( v );
    return amount < 32 ? kibitint( v ) >> amount : 0;
}

inline double kiashift( double u, double v )
{
    uint32_t amount = kibitint( v );
    return amount < 32 ? (uint32_t)( (int32_t) kibitint( v ) >> amount ) : ~(uint32_t)0;
}

inline double kibitand( double u, double v )
{
    return kibitint( u ) & kibitint( v );
}

inline double kibitxor( double u, double v )
{
    return kibitint( u ) ^ kibitint( v );
}

inline double kibitor( double u, double v )
{
    return kibitint( u ) | kibitint( v );
}

#endif

