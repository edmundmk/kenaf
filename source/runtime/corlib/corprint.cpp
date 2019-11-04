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
#include <stdio.h>
#include <assert.h>
#include <vector>
#include <exception>
#include "kenaf/runtime.h"

namespace kf
{

/*
    print has a printf-like format.
*/

static size_t print( void* cookie, frame* frame, const value* arguments, size_t argcount )
{
    std::string_view text = get_text( arguments[ 0 ] );
    const char* s = text.data();
    const char* e = text.data() + text.size();
    assert( e[ 0 ] == '\0' );

    std::vector< char > format;

    size_t argindex = 1;
    const char* p = s;
    while ( p < e )
    {
        if ( p[ 0 ] != '%' )
        {
            ++p;
            continue;
        }

        // Print format string between end of last format and %.
        printf( "%.*s", (int)( p - s ), s );

        /*
            Parse format specifier.

                %% | % [-+ #0]* [0-9]*|* (.[0-9]*|*) [sdioXxufFeEaAgG]

        */
        format.push_back( *p++ );

        if ( *p == '%' )
        {
            printf( "%%" );
            s = ++p;
            continue;
        }

        while ( *p == '-' || *p == '+' || *p == ' ' || *p == '#' || *p == '0' )
        {
            format.push_back( *p++ );
        }

        unsigned intarg_count = 0;
        int intarg[ 2 ] = { 0, 0 };

        if ( *p == '*' )
        {
            format.push_back( *p++ );
            if ( argindex >= argcount ) throw std::exception();
            if ( ! is_number( arguments[ argindex ] ) ) throw std::exception();
            intarg[ intarg_count++ ] = (int)(int64_t)get_number( arguments[ argindex ] );
            argindex += 1;
        }
        else while ( *p >= '0' && *p <= '9' )
        {
            format.push_back( *p++ );
        }

        if ( *p == '.' )
        {
            format.push_back( *p++ );
            if ( *p == '*' )
            {
                format.push_back( *p++ );
                if ( argindex >= argcount ) throw std::exception();
                if ( ! is_number( arguments[ argindex ] ) ) throw std::exception();
                intarg[ intarg_count++ ] = (int)(int64_t)get_number( arguments[ argindex ] );
                argindex += 1;
            }
            else while ( *p >= '0' && *p <= '9' )
            {
                format.push_back( *p++ );
            }
        }

        if ( argindex >= argcount ) throw std::exception();
        value arg = arguments[ argindex++ ];

        char c = *p++;
        switch ( c )
        {
        case 'c':
        {
            if ( ! is_number( arg ) ) throw std::exception();
            int c = (int)(int64_t)get_number( arg );
            format.push_back( c );
            format.push_back( '\0' );
            if ( intarg_count == 0 )
                printf( format.data(), c );
            else if ( intarg_count == 1 )
                printf( format.data(), intarg[ 0 ], c );
            else
                printf( format.data(), intarg[ 0 ], intarg[ 1 ], c );
            break;
        }

        case 's':
        {
            if ( ! is_string( arg ) ) throw std::exception();
            std::string_view text = get_text( arg );
            format.push_back( c );
            format.push_back( '\0' );
            if ( intarg_count == 0 )
                printf( format.data(), text.data() );
            else if ( intarg_count == 1 )
                printf( format.data(), intarg[ 0 ], text.data() );
            else
                printf( format.data(), intarg[ 0 ], intarg[ 1 ], text.data() );
            break;
        }

        case 'd':
        case 'i':
        {
            if ( ! is_number( arg ) ) throw std::exception();
            intmax_t i = (intmax_t)get_number( arg );
            format.push_back( 'j' );
            format.push_back( c );
            format.push_back( '\0' );
            if ( intarg_count == 0 )
                printf( format.data(), i );
            else if ( intarg_count == 1 )
                printf( format.data(), intarg[ 0 ], i );
            else
                printf( format.data(), intarg[ 0 ], intarg[ 1 ], i );
            break;
        }

        case 'o':
        case 'x':
        case 'X':
        case 'u':
        {
            if ( ! is_number( arg ) ) throw std::exception();
            uintmax_t u = (uintmax_t)get_number( arg );
            format.push_back( 'j' );
            format.push_back( c );
            format.push_back( '\0' );
            if ( intarg_count == 0 )
                printf( format.data(), u );
            else if ( intarg_count == 1 )
                printf( format.data(), intarg[ 0 ], u );
            else
                printf( format.data(), intarg[ 0 ], intarg[ 1 ], u );
            break;
        }

        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'a':
        case 'A':
        case 'g':
        case 'G':
        {
            if ( ! is_number( arg ) ) throw std::exception();
            double n = get_number( arg );
            format.push_back( c );
            format.push_back( '\0' );
            if ( intarg_count == 0 )
                printf( format.data(), n );
            else if ( intarg_count == 1 )
                printf( format.data(), intarg[ 0 ], n );
            else
                printf( format.data(), intarg[ 0 ], intarg[ 1 ], n );
            break;
        }

        default:
            throw std::exception();
        }

        format.clear();

        s = p;
    }

    printf( "%.*s", (int)( p - s ), s );

    return rvoid( frame );
}

void expose_corprint()
{
    value global = global_object();
    set_key( global, "print", create_function( print, nullptr, 1, PARAM_VARARG ) );
}

}
