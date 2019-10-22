//
//  imath.cpp
//
//  Created by Edmund Kapusniak on 03/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "imath.h"
#include <string.h>

namespace kf
{

/*
    If a double is greater than INT64_MAX or less than INT64_MIN, then double
    to int64_t conversion is undefined in C/C++.  This routine recovers the bit
    pattern that results when the value of the double is treated as an
    infinite-precision integer.
*/

uint32_t ibitint_overflow( double u )
{
    // For infinities and NaNs, return 0.
    if ( ! std::isfinite( u ) )
    {
        return 0;
    }

    // Get the actual bits of the double, assume IEEE-754.
    uint64_t b = 0;
    memcpy( &b, &u, sizeof( b ) );

    // Extract exponent and mantissa.
    int64_t mantissa = b & ( ( UINT64_C( 1 ) << 52 ) - 1 );
    int64_t exponent = (int64_t)( ( b >> 52 ) & 2047 ) - 1023;

    // Shift mantissa by the exponent to get integer part.
    int64_t shift = exponent - 52;
    if ( shift >= 0 )
    {
        if ( shift < 64 )
            mantissa <<= shift;
        else
            mantissa = 0;
    }
    else
    {
        if ( shift > -64 )
            mantissa >>= -shift;
        else
            mantissa = 0;
    }

    // Do two's complement on resulting integer if double was negative.
    if ( (int64_t)b < 0 )
    {
        mantissa = -mantissa;
    }

    // Return low 32 bits.
    return (uint32_t)mantissa;
}

}

