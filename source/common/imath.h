//
//  imath.h
//
//  Created by Edmund Kapusniak on 03/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KF_IMATH_H
#define KF_IMATH_H

#include <stdint.h>
#include <cmath>

namespace kf
{

uint32_t ibitint_overflow( double u );

double ifloordiv( double u, double v );
double ifloormod( double u, double v );
double ibitnot( double u );
double ilshift( double u, double v );
double irshift( double u, double v );
double iashift( double u, double v );
double ibitand( double u, double v );
double ibitxor( double u, double v );
double ibitor( double u, double v );

inline double ifloordiv( double u, double v )
{
    return std::floor( u / v );
}

inline double ifloormod( double u, double v )
{
    double x = std::fmod( u, v );
    return ( x < 0.0 ) == ( v < 0.0 ) ? x : x + v;
}

inline uint32_t ibitint( double u )
{
    if ( -0x1p63 <= u && u < 0x1p63 )
    {
        return (uint32_t)(int64_t)u;
    }
    else
    {
        return ibitint_overflow( u );
    }
}

inline double ibitnot( double u )
{
    return (double)(uint32_t)~ibitint( u );
}

inline double ilshift( double u, double v )
{
    uint32_t amount = ibitint( v );
    return amount < 32 ? ibitint( u ) << amount : 0;
}

inline double irshift( double u, double v )
{
    uint32_t amount = ibitint( v );
    return amount < 32 ? ibitint( u ) >> amount : 0;
}

inline double iashift( double u, double v )
{
    uint32_t amount = ibitint( v );
    return amount < 32 ? (uint32_t)( (int32_t) ibitint( u ) >> amount ) : ~(uint32_t)0;
}

inline double ibitand( double u, double v )
{
    return ibitint( u ) & ibitint( v );
}

inline double ibitxor( double u, double v )
{
    return ibitint( u ) ^ ibitint( v );
}

inline double ibitor( double u, double v )
{
    return ibitint( u ) | ibitint( v );
}

}

#endif

