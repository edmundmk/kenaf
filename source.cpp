//
//  source.cpp
//
//  Created by Edmund Kapusniak on 28/09/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "source.h"
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <algorithm>

namespace kf
{

source::source()
    :   text( SOURCE_LOOKAHEAD, '\0' )
    ,   newlines{ 0 }
{
}

source::~source()
{
    for ( source_string* s : strings )
    {
        free( s );
    }
}

void source::append( void* data, size_t size )
{
    text.insert( text.end() - SOURCE_LOOKAHEAD, (char*)data, (char*)data + size );
}

size_t source::size() const
{
    return text.size() - SOURCE_LOOKAHEAD;
}

void source::newline( srcloc sloc )
{
    assert( sloc >= newlines.back() );
    newlines.push_back( sloc );
}

const source_string* source::new_string( const char* text, size_t size )
{
    source_string* s = (source_string*)malloc( sizeof( source_string ) + size );
    s->size = size;
    memcpy( (char*)s->text, text, size );
    strings.push_back( s );
    return s;
}

source_location source::location( srcloc sloc ) const
{
    auto i = --std::upper_bound( newlines.begin(), newlines.end(), sloc );
    return { (unsigned)( i - newlines.begin() + 1 ), (unsigned)( sloc - *i + 1 ) };
}

void source::error( srcloc sloc, const char* message, ... )
{
    va_list vap, ap;
    va_start( vap, message );
    va_copy( ap, vap );
    int size = vsnprintf( nullptr, 0, message, ap );
    va_end( ap );
    std::string text( size + 1, '\0' );
    vsnprintf( text.data(), text.size(), message, vap );
    va_end( vap );
    text.pop_back();
    diagnostics.push_back( { ERROR, location( sloc ), std::move( text ) } );
}

}

