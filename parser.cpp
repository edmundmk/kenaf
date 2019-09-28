//
//  parser.cpp
//
//  Created by Edmund Kapusniak on 28/09/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "parser.h"

void* KenafParseAlloc( void* (*mallocProc)( size_t ) );
void KenafParseFree( void* p, void (*freeProc)( void* ) );
void KenafParse( void* yyp, int yymajor, kf::token yyminor, kf::parser* p );

#ifndef NDEBUG
void KenafParseTrace( FILE* TraceFILE, char* zTracePrompt );
#endif

namespace kf
{

parser::parser( source* source, lexer* lexer )
    :   _source( source )
    ,   _lexer( lexer )
    ,   _yyp( KenafParseAlloc( malloc ) )
    ,   _syntax_tree( std::make_unique< syntax_tree >() )
    ,   _syntax_function( _syntax_tree->new_function() )
{
}

parser::~parser()
{
    KenafParseFree( _yyp, free );
}

std::unique_ptr< syntax_tree > parser::parse()
{
#ifndef NDEBUG
    bool trace = false;
    if ( getenv( "KF_PARSE_TRACE" ) )
    {
        KenafParseTrace( stdout, (char*)"" );
        trace = true;
    }
#endif

    while ( true )
    {
        token token = _lexer->lex();

#ifndef NDEBUG
        if ( trace )
        {
            source_location location = _source->location( token.sloc );
            printf( "%s:%u:%u: %s\n", _source->filename.c_str(), location.line, location.column, spelling( token ).c_str() );
        }
#endif

        KenafParse( _yyp, token.kind, token, this );
        if ( token.kind == TOKEN_EOF )
        {
            break;
        }
    }

    return std::move( _syntax_tree );
}

void parser::syntax_error( token token )
{
    _source->error( token.sloc, "unexpected %s", spelling( token ).c_str() );
}

}

