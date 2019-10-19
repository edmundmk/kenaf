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

std::unique_ptr< ast_script > parser::parse()
{
#ifndef NDEBUG
    bool trace = false;
    if ( getenv( "KF_PARSE_TRACE" ) )
    {
        KenafParseTrace( stdout, (char*)"" );
        trace = true;
    }
#endif

    _ast_script = std::make_unique< ast_script >();

    _fstack.push_back( _ast_script->new_function( 0, nullptr ) );
    _fstack.back()->name = _source->filename;
    _fstack.back()->is_top_level = true;

    unsigned z = string_node( AST_NAME, 0, "args", 4 );
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

    node( AST_FUNCTION, 0, z );
    pop_function();
    _fstack.clear();

    return std::move( _ast_script );
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

ast_function* parser::push_function( srcloc sloc )
{
    _fstack.push_back( _ast_script->new_function( sloc, _fstack.back() ) );
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

srcloc parser::node_sloc( unsigned index )
{
    if ( index != AST_INVALID_INDEX )
        return _fstack.back()->nodes.at( index ).sloc;
    else
        return 0;
}

ast_node_kind parser::node_kind( unsigned index )
{
    if ( index != AST_INVALID_INDEX )
        return _fstack.back()->nodes.at( index ).kind;
    else
        return AST_NONE;
}

void parser::update_sloc( unsigned index, srcloc sloc )
{
    _fstack.back()->nodes.at( index ).sloc = sloc;
}

unsigned parser::node( ast_node_kind kind, srcloc sloc, unsigned child )
{
    std::vector< ast_node >& nodes = _fstack.back()->nodes;
    unsigned index = nodes.size();
    child = child != AST_INVALID_INDEX ? child : index;
    nodes.push_back( { kind, AST_NO_LEAF, false, sloc, child, AST_INVALID_INDEX } );
    return index;
}

unsigned parser::string_node( ast_node_kind kind, srcloc sloc, const char* text, unsigned size )
{
    return string_node( kind, sloc, AST_INVALID_INDEX, text, size );
}

unsigned parser::string_node( ast_node_kind kind, srcloc sloc, unsigned child, const char* text, unsigned size )
{
    std::vector< ast_node >& nodes = _fstack.back()->nodes;
    unsigned index = nodes.size();
    child = child != AST_INVALID_INDEX ? child : index;
    ast_node& node = *nodes.insert( nodes.end(), { { kind, AST_LEAF_STRING, false, sloc, child, AST_INVALID_INDEX }, {} } );
    node.leaf_string() = { text, size };
    return index;
}

unsigned parser::number_node( ast_node_kind kind, srcloc sloc, double n )
{
    std::vector< ast_node >& nodes = _fstack.back()->nodes;
    unsigned index = nodes.size();
    ast_node& node = *nodes.insert( nodes.end(), { { kind, AST_LEAF_NUMBER, false, sloc, index, AST_INVALID_INDEX }, {} } );
    node.leaf_number().n = n;
    return index;
}

unsigned parser::function_node( ast_node_kind kind, srcloc sloc, ast_function* function )
{
    std::vector< ast_node >& nodes = _fstack.back()->nodes;
    unsigned index = nodes.size();
    ast_node& node = *nodes.insert( nodes.end(), { { kind, AST_LEAF_FUNCTION, false, sloc, index, AST_INVALID_INDEX }, {} } );
    node.leaf_function().function = function;
    return index;
}

unsigned parser::index_node( ast_node_kind kind, srcloc sloc, unsigned child )
{
    std::vector< ast_node >& nodes = _fstack.back()->nodes;
    unsigned index = nodes.size();
    child = child != AST_INVALID_INDEX ? child : index;
    ast_node& node = *nodes.insert( nodes.end(), { { kind, AST_LEAF_INDEX, false, sloc, child, AST_INVALID_INDEX }, {} } );
    node.leaf_index().index = AST_INVALID_INDEX;
    return index;

}

std::string parser::qual_name_string( unsigned index )
{
    std::vector< ast_node >& nodes = _fstack.back()->nodes;
    const ast_node& n = nodes.at( index );

    if ( n.kind == AST_NAME )
    {
        const ast_leaf_string& s = n.leaf_string();
        return std::string( s.text, s.size );
    }
    else if ( n.kind == AST_EXPR_KEY )
    {
        const ast_leaf_string& s = n.leaf_string();
        std::string qual_name = qual_name_string( n.child_index );
        qual_name.append( "." );
        qual_name.append( std::string_view( s.text, s.size ) );
        return qual_name;
    }
    else
    {
        assert( ! "malformed qual_name AST" );
        return "";
    }
}

}

