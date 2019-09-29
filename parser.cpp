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
    ,   _fstack{ _syntax_tree->new_function( 0, nullptr ) }
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

    do
    {
        _token = _lexer->lex();

#ifndef NDEBUG
        if ( trace )
        {
            source_location location = _source->location( _token.sloc );
            printf( "%s:%u:%u: %s\n", _source->filename.c_str(), location.line, location.column, spelling( _token ).c_str() );
        }
#endif

        KenafParse( _yyp, _token.kind, _token, this );
    }
    while ( _token.kind != TOKEN_EOF );

    _fstack.clear();
    return std::move( _syntax_tree );
}

void parser::syntax_error( token token )
{
    _source->error( token.sloc, "unexpected %s", spelling( token ).c_str() );
}

void parser::error( srcloc sloc, const char* message, ... )
{
    va_list ap;
    va_start( ap, message );
    _source->error( sloc, message, ap );
    va_end( ap );
}

syntax_function* parser::push_function( srcloc sloc )
{
    _fstack.push_back( _syntax_tree->new_function( sloc, _fstack.back() ) );
    return _fstack.back();
}

void parser::pop_function()
{
    _fstack.pop_back();
}

srcloc parser::current_sloc()
{
    return _token.sloc;
}

srcloc parser::node_sloc( size_t index )
{
    if ( index != AST_INVALID_INDEX )
        return _fstack.back()->nodes.at( index ).sloc;
    else
        return 0;
}

void parser::update_sloc( size_t index, srcloc sloc )
{
    _fstack.back()->nodes.at( index ).sloc = sloc;
}

size_t parser::node( syntax_node_kind kind, srcloc sloc, size_t child )
{
    syntax_node node;
    node.kind = kind;
    node.leaf = AST_NON_LEAF;
    node.sloc = sloc;
    node.child_index = child;
    node.next_index = AST_INVALID_INDEX;
    size_t index = _fstack.back()->nodes.size();
    _fstack.back()->nodes.push_back( node );
    return index;
}

size_t parser::string_node( syntax_node_kind kind, srcloc sloc, const char* text, size_t size )
{
    syntax_node node;
    node.kind = kind;
    node.leaf = AST_LEAF_STRING;
    node.sloc = sloc;
    node.s.text = text;
    node.s.size = size;
    size_t index = _fstack.back()->nodes.size();
    _fstack.back()->nodes.push_back( node );
    return index;
}

size_t parser::number_node( syntax_node_kind kind, srcloc sloc, double n )
{
    syntax_node node;
    node.kind = kind;
    node.leaf = AST_LEAF_NUMBER;
    node.sloc = sloc;
    node.n = n;
    size_t index = _fstack.back()->nodes.size();
    _fstack.back()->nodes.push_back( node );
    return index;
}

size_t parser::function_node( syntax_node_kind kind, srcloc sloc, syntax_function* function )
{
    syntax_node node;
    node.kind = kind;
    node.leaf = AST_LEAF_FUNCTION;
    node.sloc = sloc;
    node.function = function;
    size_t index = _fstack.back()->nodes.size();
    _fstack.back()->nodes.push_back( node );
    return index;
}

}

