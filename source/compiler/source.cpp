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
#include <string.h>
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
}

void source::append( const void* data, size_t size )
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
    strings.push_back( source_string_ptr( s ) );
    return s;
}

const source_string* source::new_string( const char* atext, size_t asize, const char* btext, size_t bsize )
{
    source_string* s = (source_string*)malloc( sizeof( source_string ) + asize + bsize );
    s->size = asize + bsize;
    memcpy( (char*)s->text, atext, asize );
    memcpy( (char*)s->text + asize, btext, bsize );
    strings.push_back( source_string_ptr( s ) );
    return s;
}

source_location source::location( srcloc sloc ) const
{
    auto i = --std::upper_bound( newlines.begin(), newlines.end(), sloc );
    return { (unsigned)( i - newlines.begin() + 1 ), (unsigned)( sloc - *i + 1 ) };
}

errors::errors()
{
}

errors::~errors()
{
}

void errors::reset()
{
    diagnostics.clear();
    has_error = false;
}

report::report( struct source* source, struct errors* errors )
    :   source( source )
    ,   errors( errors )
{
}

report::~report()
{
}

void report::error( srcloc sloc, const char* message, ... )
{
    va_list ap;
    va_start( ap, message );
    diagnostic( ERROR, sloc, message, ap );
    va_end( ap );
}

void report::error( srcloc sloc, const char* message, va_list ap )
{
    diagnostic( ERROR, sloc, message, ap );
}

void report::warning( srcloc sloc, const char* message, ... )
{
    va_list ap;
    va_start( ap, message );
    diagnostic( WARNING, sloc, message, ap );
    va_end( ap );
}

void report::warning( srcloc sloc, const char* message, va_list ap )
{
    diagnostic( WARNING, sloc, message, ap );
}

void report::diagnostic( diagnostic_kind kind, srcloc sloc, const char* message, va_list ap )
{
    va_list aq;

    va_copy( aq, ap );
    int size = vsnprintf( nullptr, 0, message, aq );
    va_end( aq );

    std::string text( size + 1, '\0' );

    va_copy( aq, ap );
    vsnprintf( text.data(), text.size(), message, aq );
    va_end( aq );

    text.pop_back();
    source_location l = source->location( sloc );
    errors->diagnostics.push_back( { kind, { l.line, l.column }, std::move( text ) } );
    errors->has_error |= kind == ERROR;
}

}

