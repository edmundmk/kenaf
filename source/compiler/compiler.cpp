//
//  compiler.cpp
//
//  Created by Edmund Kapusniak on 19/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "kenaf/compiler.h"
#include "source.h"
#include "lexer.h"
#include "parser.h"
#include "ast_resolve.h"
#include "ir_build.h"
#include "ir_live.h"
#include "ir_fold.h"
#include "ir_foldk.h"
#include "ir_alloc.h"
#include "ir_emit.h"
#include "code_unit.h"

namespace kf
{

compile_result::compile_result()
    :   _code( nullptr )
{
}

compile_result::compile_result( const code_script* code, std::vector< struct diagnostic >&& diagnostics )
    :   _code( code )
    ,   _diagnostics( std::move( diagnostics ) )
{
}

compile_result::compile_result( std::vector< struct diagnostic >&& diagnostics )
    :   _code( nullptr )
    ,   _diagnostics( std::move( diagnostics ) )
{
}

compile_result::compile_result( compile_result&& r )
    :   _code( nullptr )
{
    std::swap( _code, r._code );
    std::swap( _diagnostics, r._diagnostics );
}

compile_result& compile_result::operator = ( compile_result&& r )
{
    free( (void*)_code );
    _code = nullptr;
    _diagnostics.clear();
    std::swap( _code, r._code );
    std::swap( _diagnostics, r._diagnostics );
    return *this;
}

compile_result::~compile_result()
{
    free( (void*)_code );
}

compile_result::operator bool () const
{
    return _code != nullptr;
}

const code_script* compile_result::code() const
{
    return _code;
}

size_t compile_result::diagnostic_count() const
{
    return _diagnostics.size();
}

const diagnostic& compile_result::diagnostic( size_t index ) const
{
    return _diagnostics.at( index );
}

compile_result compile( std::string_view filename, std::string_view text, unsigned debug_print )
{
    // Load source text.
    source source;
    source.filename = filename;
    source.append( text.data(), text.size() );

    // Parse AST.
    lexer lexer( &source );
    parser parser( &source, &lexer );
    std::unique_ptr< ast_script > script = parser.parse();
    if ( debug_print & PRINT_AST_PARSED )
        script->debug_print();
    if ( source.has_error )
        return compile_result( std::move( source.diagnostics ) );

    // Construct code unit.
    code_unit unit;
    unit.script.debug_script_name = unit.debug_heap.size();
    unit.debug_newlines = source.newlines;
    unit.debug_heap.insert( unit.debug_heap.end(), source.filename.begin(), source.filename.end() );
    unit.debug_heap.push_back( '\0' );

    // Resolve names.
    ast_resolve resolve( &source, script.get() );
    resolve.resolve();
    if ( debug_print & PRINT_AST_RESOLVED )
        script->debug_print();
    if ( source.has_error )
        return compile_result( std::move( source.diagnostics ) );

    // Perform IR passes.
    ir_build build( &source );
    ir_fold fold( &source );
    ir_live live( &source );
    ir_foldk foldk( &source );
    ir_alloc alloc( &source );
    ir_emit emit( &source, &unit );

    for ( const auto& function : script->functions )
    {
        std::unique_ptr< ir_function > ir = build.build( function.get() );
        if ( ir && ( debug_print & PRINT_IR_BUILD ) )
            ir->debug_print();
        if ( ! ir || source.has_error )
            return compile_result( std::move( source.diagnostics ) );

        fold.fold( ir.get() );
        if ( debug_print & PRINT_IR_FOLD )
            ir->debug_print();
        if ( source.has_error )
            return compile_result( std::move( source.diagnostics ) );

        live.live( ir.get() );
        if ( debug_print & PRINT_IR_LIVE )
            ir->debug_print();
        if ( source.has_error )
            return compile_result( std::move( source.diagnostics ) );

        foldk.foldk( ir.get() );
        if ( debug_print & PRINT_IR_FOLDK )
            ir->debug_print();
        if ( source.has_error )
            return compile_result( std::move( source.diagnostics ) );

        live.live( ir.get() );
        if ( debug_print & PRINT_IR_FOLD_LIVE )
            ir->debug_print();
        if ( source.has_error )
            return compile_result( std::move( source.diagnostics ) );

        alloc.alloc( ir.get() );
        if ( debug_print & PRINT_IR_ALLOC )
            ir->debug_print();
        if ( source.has_error )
            return compile_result( std::move( source.diagnostics ) );

        emit.emit( ir.get() );
        if ( source.has_error )
            return compile_result( std::move( source.diagnostics ) );
    }

    return compile_result( unit.pack(), std::move( source.diagnostics ) );
}

}
