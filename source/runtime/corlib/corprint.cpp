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

    size_t argindex = 1;
    for ( const char* p = s; p < e; ++p )
    {
        if ( p[ 0 ] == '%' )
        {
            printf( "%.*s", (int)( p - s ), s );
        }

    }

    return rvoid( frame );
}

void expose_corprint()
{
    value global = global_object();
    set_key( global, "print", create_function( print, nullptr, 1, PARAM_VARARG ) );
}

}

