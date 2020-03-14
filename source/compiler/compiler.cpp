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

struct compiler
{
    intptr_t refcount;
    unsigned debug_print;
    std::unique_ptr< struct errors > errors;
    std::unique_ptr< struct source > source;
    code_script_ptr code;
};

compiler* create_compiler()
{
    compiler* c = new compiler();
    c->refcount = 1;
    c->debug_print = 0;
    c->errors = std::make_unique< errors >();
    return c;
}

compiler* retain_compiler( compiler* c )
{
    assert( c->refcount >= 1 );
    c->refcount += 1;
    return c;
}

void release_compiler( compiler* c )
{
    assert( c->refcount >= 1 );
    if ( --c->refcount == 0 )
    {
        delete c;
    }
}

void source_path( compiler* c, std::string_view path )
{
    if ( ! c->source ) c->source = std::make_unique< source >();
    c->source->path = path;
}

void source_text( compiler* c, std::string_view text )
{
    if ( ! c->source ) c->source = std::make_unique< source >();
    c->source->append( text.data(), text.size() );
}

void* source_buffer( compiler* c, size_t size )
{
    if ( ! c->source ) c->source = std::make_unique< source >();
    return c->source->buffer( size );
}

bool compile( compiler* c )
{
    if ( ! c->source ) return false;
    c->errors->reset();
    c->code.reset();

    report report( c->source.get(), c->errors.get() );

    try
    {
        // Parse AST.
        lexer lexer( &report, c->source.get() );
        parser parser( &report, &lexer );
        std::unique_ptr< ast_script > script = parser.parse();
        if ( c->debug_print & PRINT_AST_PARSED )
            script->debug_print();
        if ( c->errors->has_error )
            goto return_error;

        // Construct code unit.
        code_unit unit;
        unit.script.debug_script_name = unit.debug_heap.size();
        unit.debug_newlines = c->source->newlines;
        unit.debug_heap.insert( unit.debug_heap.end(), c->source->path.begin(), c->source->path.end() );
        unit.debug_heap.push_back( '\0' );

        // Resolve names.
        ast_resolve resolve( &report, script.get() );
        resolve.resolve();
        if ( c->debug_print & PRINT_AST_RESOLVED )
            script->debug_print();
        if ( c->errors->has_error )
            goto return_error;

        // Perform IR passes.
        ir_build build( &report );
        ir_fold fold( &report, c->source.get() );
        ir_live live( &report );
        ir_foldk foldk( &report );
        ir_alloc alloc( &report );
        ir_emit emit( &report, &unit );

        for ( const auto& function : script->functions )
        {
            std::unique_ptr< ir_function > ir = build.build( function.get() );
            if ( ir && ( c->debug_print & PRINT_IR_BUILD ) )
                ir->debug_print();
            if ( ! ir || c->errors->has_error )
                goto return_error;

            fold.fold( ir.get() );
            if ( c->debug_print & PRINT_IR_FOLD )
                ir->debug_print();
            if ( c->errors->has_error )
                goto return_error;

            live.live( ir.get() );
            if ( c->debug_print & PRINT_IR_LIVE )
                ir->debug_print();
            if ( c->errors->has_error )
                goto return_error;

            foldk.foldk( ir.get() );
            if ( c->debug_print & PRINT_IR_FOLDK )
                ir->debug_print();
            if ( c->errors->has_error )
                goto return_error;

            live.live( ir.get() );
            if ( c->debug_print & PRINT_IR_FOLD_LIVE )
                ir->debug_print();
            if ( c->errors->has_error )
                goto return_error;

            alloc.alloc( ir.get() );
            if ( c->debug_print & PRINT_IR_ALLOC )
                ir->debug_print();
            if ( c->errors->has_error )
                goto return_error;

            emit.emit( ir.get() );
            if ( c->errors->has_error )
                goto return_error;
        }

        code_script_ptr code = unit.pack();
        if ( c->debug_print & PRINT_CODE )
            code->debug_print();

        c->code = std::move( code );

        c->source.reset();
        return ! c->errors->has_error;
    }
    catch ( std::exception& e )
    {
        report.error( 0, "internal: %s", e.what() );
    }

return_error:
    c->source.reset();
    return ! c->errors->has_error;
}

const void* compiled_code( compiler* c )
{
    return c->code.get();
}

size_t compiled_size( compiler* c )
{
    return c->code ? c->code->code_size : 0;
}

size_t diagnostic_count( compiler* c )
{
    return c->errors->diagnostics.size();
}

diagnostic get_diagnostic( compiler* c, size_t index )
{
    const source_diagnostic& d = c->errors->diagnostics.at( index );
    return { d.kind, d.location.line, d.location.column, d.message };
}

void debug_print( compiler* c, unsigned debug_print )
{
    c->debug_print = debug_print;
}

void debug_print( const void* code, size_t size )
{
    const code_script* c = (const code_script*)code;
    assert( c->magic == CODE_MAGIC );
    assert( c->code_size == size );
    c->debug_print();
}

}

