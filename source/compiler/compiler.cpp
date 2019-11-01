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

struct compilation
{
    intptr_t refcount;
    code_script_ptr code;
    std::vector< source_diagnostic > diagnostics;
};

compilation* compilation_retain( compilation* cn )
{
    assert( cn->refcount >= 1 );
    cn->refcount += 1;
    return cn;
}

void compilation_release( compilation* cn )
{
    assert( cn->refcount >= 1 );
    if ( --cn->refcount == 0 )
    {
        delete cn;
    }
}

bool compilation_success( compilation* cn )
{
    return cn->code != nullptr;
}

code_view compilation_code( compilation* cn )
{
    if ( cn->code )
        return { cn->code.get(), cn->code->code_size };
    else
        return { nullptr, 0 };
}

size_t compilation_diagnostic_count( compilation* cn )
{
    return cn->diagnostics.size();
}

diagnostic compilation_diagnostic( compilation* cn, size_t index )
{
    const source_diagnostic& d = cn->diagnostics.at( index );
    return { d.kind, d.location.line, d.location.column, d.message };
}

compilation* compile( std::string_view filename, std::string_view text, unsigned debug_print )
{
    std::unique_ptr< compilation > cn = std::make_unique< compilation >();
    cn->refcount = 1;

    source source;
    try
    {

        // Load source text.
        source.filename = filename;
        source.append( text.data(), text.size() );

        // Parse AST.
        lexer lexer( &source );
        parser parser( &source, &lexer );
        std::unique_ptr< ast_script > script = parser.parse();
        if ( debug_print & PRINT_AST_PARSED )
            script->debug_print();
        if ( source.has_error )
            goto return_error;

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
            goto return_error;

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
                goto return_error;

            fold.fold( ir.get() );
            if ( debug_print & PRINT_IR_FOLD )
                ir->debug_print();
            if ( source.has_error )
                goto return_error;

            live.live( ir.get() );
            if ( debug_print & PRINT_IR_LIVE )
                ir->debug_print();
            if ( source.has_error )
                goto return_error;

            foldk.foldk( ir.get() );
            if ( debug_print & PRINT_IR_FOLDK )
                ir->debug_print();
            if ( source.has_error )
                goto return_error;

            live.live( ir.get() );
            if ( debug_print & PRINT_IR_FOLD_LIVE )
                ir->debug_print();
            if ( source.has_error )
                goto return_error;

            alloc.alloc( ir.get() );
            if ( debug_print & PRINT_IR_ALLOC )
                ir->debug_print();
            if ( source.has_error )
                goto return_error;

            emit.emit( ir.get() );
            if ( source.has_error )
                goto return_error;
        }

        code_script_ptr code = unit.pack();
        if ( debug_print & PRINT_CODE )
            code->debug_print();

        cn->code = std::move( code );

    }
    catch ( std::exception& e )
    {
        source.error( 0, "internal: %s", e.what() );
    }

return_error:
    std::swap( cn->diagnostics, source.diagnostics );
    return cn.release();
}

void debug_print_code( const void* code, size_t size )
{
    const code_script* c = (const code_script*)code;
    assert( c->magic == CODE_MAGIC );
    assert( c->code_size == size );
    c->debug_print();
}

}

