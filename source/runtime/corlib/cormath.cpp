//
//  cormath.cpp
//
//  Created by Edmund Kapusniak on 02/11/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "cormath.h"
#include <cmath>
#include <algorithm>
#include "kenaf/runtime.h"
#include "kenaf/errors.h"
#include "../../common/imath.h"

namespace kf
{

const double PI = 0x3.243F6A8885A30p0;

static size_t abs( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    return result( frame, number_value( std::abs( get_number( arguments[ 0 ] ) ) ) );
}

static size_t min( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    double n = get_number( arguments[ 0 ] );
    for ( size_t i = 1; i < argcount; ++i )
    {
        n = std::min( n, get_number( arguments[ i ] ) );
    }
    return result( frame, number_value( n ) );
}

static size_t max( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    double n = get_number( arguments[ 0 ] );
    for ( size_t i = 1; i < argcount; ++i )
    {
        n = std::max( n, get_number( arguments[ i ] ) );
    }
    return result( frame, number_value( n ) );
}

static size_t pow( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    double a = get_number( arguments[ 0 ] );
    double b = get_number( arguments[ 1 ] );
    return result( frame, number_value( std::pow( a, b ) ) );
}

static size_t sqrt( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    return result( frame, number_value( std::sqrt( get_number( arguments[ 0 ] ) ) ) );
}

static size_t sin( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    return result( frame, number_value( std::sin( get_number( arguments[ 0 ] ) ) ) );
}

static size_t tan( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    return result( frame, number_value( std::tan( get_number( arguments[ 0 ] ) ) ) );
}

static size_t cos( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    return result( frame, number_value( std::cos( get_number( arguments[ 0 ] ) ) ) );
}

static size_t asin( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    return result( frame, number_value( std::asin( get_number( arguments[ 0 ] ) ) ) );
}

static size_t acos( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    return result( frame, number_value( std::acos( get_number( arguments[ 0 ] ) ) ) );
}

static size_t atan( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    if ( argcount == 1 )
    {
        return result( frame, number_value( std::atan( get_number( arguments[ 0 ] ) ) ) );
    }
    else if ( argcount == 2 )
    {
        return result( frame, number_value( std::atan2( get_number( arguments[ 0 ] ), get_number( arguments[ 1 ] ) ) ) );
    }
    else
    {
        throw argument_error( "too few arguments, expected 1 or 2, not %zu", argcount );
    }
}

static size_t ceil( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    return result( frame, number_value( std::ceil( get_number( arguments[ 0 ] ) ) ) );
}

static size_t floor( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    return result( frame, number_value( std::floor( get_number( arguments[ 0 ] ) ) ) );
}

static size_t round( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    return result( frame, number_value( std::round( get_number( arguments[ 0 ] ) ) ) );
}

static size_t trunc( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    return result( frame, number_value( std::trunc( get_number( arguments[ 0 ] ) ) ) );
}

static size_t isnan( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    return result( frame, bool_value( std::isnan( get_number( arguments[ 0 ] ) ) ) );
}

static size_t isinf( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    return result( frame, bool_value( std::isinf( get_number( arguments[ 0 ] ) ) ) );
}

static size_t isfinite( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    return result( frame, bool_value( std::isfinite( get_number( arguments[ 0 ] ) ) ) );
}

static size_t fmod( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    double a = get_number( arguments[ 0 ] );
    double b = get_number( arguments[ 1 ] );
    return result( frame, number_value( std::fmod( a, b ) ) );
}

static size_t log2( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    return result( frame, bool_value( std::log2( get_number( arguments[ 0 ] ) ) ) );
}

static size_t exp2( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    return result( frame, bool_value( std::exp2( get_number( arguments[ 0 ] ) ) ) );
}

static size_t log( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    return result( frame, bool_value( std::log( get_number( arguments[ 0 ] ) ) ) );
}

static size_t exp( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    return result( frame, bool_value( std::exp( get_number( arguments[ 0 ] ) ) ) );
}

static size_t clz( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    uint32_t i = ibitint( get_number( arguments[ 0 ] ) );
    i = i ? __builtin_clz( i ) : 0;
    return result( frame, number_value( i ) );
}

static size_t ctz( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    uint32_t i = ibitint( get_number( arguments[ 0 ] ) );
    i = i ? __builtin_ctz( i ) : 0;
    return result( frame, number_value( i ) );
}

void expose_cormath()
{
    value global = global_object();

    set_key( global, "abs", create_function( abs, nullptr, 1 ) );
    set_key( global, "min", create_function( min, nullptr, 1, PARAM_VARARG ) );
    set_key( global, "max", create_function( max, nullptr, 1, PARAM_VARARG ) );
    set_key( global, "pow", create_function( pow, nullptr, 2 ) );
    set_key( global, "sqrt", create_function( sqrt, nullptr, 1 ) );
    set_key( global, "sin", create_function( sin, nullptr, 1 ) );
    set_key( global, "tan", create_function( tan, nullptr, 1 ) );
    set_key( global, "cos", create_function( cos, nullptr, 1 ) );
    set_key( global, "asin", create_function( asin, nullptr, 1 ) );
    set_key( global, "acos", create_function( acos, nullptr, 1 ) );
    set_key( global, "atan", create_function( atan, nullptr, 1, PARAM_VARARG ) );
    set_key( global, "ceil", create_function( ceil, nullptr, 1 ) );
    set_key( global, "floor", create_function( floor, nullptr, 1 ) );
    set_key( global, "round", create_function( round, nullptr, 1 ) );
    set_key( global, "trunc", create_function( trunc, nullptr, 1 ) );
    set_key( global, "isnan", create_function( isnan, nullptr, 1 ) );
    set_key( global, "isinf", create_function( isinf, nullptr, 1 ) );
    set_key( global, "isfinite", create_function( isfinite, nullptr, 1 ) );
    set_key( global, "fmod", create_function( fmod, nullptr, 2 ) );
    set_key( global, "log2", create_function( log2, nullptr, 1 ) );
    set_key( global, "exp2", create_function( exp2, nullptr, 1 ) );
    set_key( global, "log", create_function( log, nullptr, 1 ) );
    set_key( global, "exp", create_function( exp, nullptr, 1 ) );
    set_key( global, "clz", create_function( clz, nullptr, 1 ) );
    set_key( global, "ctz", create_function( ctz, nullptr, 1 ) );

    set_key( global, "pi", number_value( PI ) );
    set_key( global, "tau", number_value( PI * 2.0 ) );
    set_key( global, "nan", number_value( NAN ) );
    set_key( global, "infinity", number_value( INFINITY ) );
}

}

