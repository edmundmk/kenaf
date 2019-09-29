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

    _syntax_tree = std::make_unique< syntax_tree >();

    _fstack.push_back( _syntax_tree->new_function( 0, nullptr ) );
    _fstack.back()->is_top_level = true;

    size_t z = string_node( AST_EXPR_NAME, 0, "args", 4 );
    z = node( AST_VARARG_PARAM, 0, z );
    z = node( AST_PARAMETERS, 0, z );

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

    pop_function();
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
    _fstack.back()->fixup_nodes();
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
    size_t index = _fstack.back()->nodes.size();
    child = child != AST_INVALID_INDEX ? child : index;
    _fstack.back()->nodes.push_back( { (uint16_t)kind, AST_NON_LEAF, sloc, (unsigned)child, 0 } );
    return index;
}

size_t parser::string_node( syntax_node_kind kind, srcloc sloc, const char* text, size_t size )
{
    size_t index = _fstack.back()->nodes.size();
    _fstack.back()->nodes.push_back( { (uint16_t)kind, AST_LEAF_STRING, sloc, (unsigned)index, 0 } );
    _fstack.back()->nodes.push_back( {} );
    *( (syntax_leaf_string*)&_fstack.back()->nodes.back() ) = { text, size };
    return index;
}

size_t parser::number_node( syntax_node_kind kind, srcloc sloc, double n )
{
    size_t index = _fstack.back()->nodes.size();
    _fstack.back()->nodes.push_back( { (uint16_t)kind, AST_LEAF_NUMBER, sloc, (unsigned)index, 0 } );
    _fstack.back()->nodes.push_back( {} );
    ( (syntax_leaf_number*)&_fstack.back()->nodes.back() )->n = n;
    return index;
}

size_t parser::function_node( syntax_node_kind kind, srcloc sloc, syntax_function* function )
{
    size_t index = _fstack.back()->nodes.size();
    _fstack.back()->nodes.push_back( { (uint16_t)kind, AST_LEAF_FUNCTION, sloc, (unsigned)index, 0 } );
    _fstack.back()->nodes.push_back( {} );
    ( (syntax_leaf_function*)&_fstack.back()->nodes.back() )->function = function;
    return index;
}

std::string parser::qual_name_string( size_t index )
{
    const syntax_node& n = _fstack.back()->nodes.at( index );
    if ( n.kind == AST_EXPR_NAME )
    {
        syntax_leaf_string* s = (syntax_leaf_string*)( &n + 1 );
        return std::string( s->text, s->size );
    }
    else if ( n.kind == AST_EXPR_KEY )
    {
        const syntax_node& o = _fstack.back()->nodes.at( index + 1 );
        assert( o.kind == AST_EXPR_NAME );
        syntax_leaf_string* s = (syntax_leaf_string*)( &o + 1 );

        std::string qual_name = qual_name_string( n.child_index );
        qual_name.append( "." );
        qual_name.append( s->text, s->size );

        return qual_name;
    }
    else
    {
        assert( ! "malformed qual_name AST" );
        return "";
    }
}

}

