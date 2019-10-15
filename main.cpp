//
//  kenaf
//
//  Created by Edmund Kapusniak on 28/09/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include <stdlib.h>
#include "parser/source.h"
#include "parser/lexer.h"
#include "parser/parser.h"
#include "parser/resolve_names.h"
#include "parser/build_ir.h"
#include "parser/fold_ir.h"
#include "parser/live_ir.h"
#include "parser/foldk_ir.h"

int main( int argc, char* argv[] )
{
    kf::source source;
    source.filename = "[stdin]";
    FILE* f = stdin;

    if ( argc > 1 )
    {
        source.filename = argv[ 1 ];
        f = fopen( argv[ 1 ], "rb" );
        if ( ! f )
        {
            fprintf( stderr, "unable to open script: %s\n", argv[ 1 ] );
            return EXIT_FAILURE;
        }
    }

    std::unique_ptr< char[] > buffer( new char[ 1024 ] );
    while ( true )
    {
        size_t read = fread( buffer.get(), 1, 1024, f );
        source.append( buffer.get(), read );
        if ( read < 1024 )
            break;
    }
    buffer.reset();

    if ( f != stdin )
    {
        fclose( f );
    }

    kf::lexer lexer( &source );
    kf::parser parser( &source, &lexer );
    std::unique_ptr< kf::ast_script > ast_script = parser.parse();

    if ( ! source.has_error )
    {
        kf::resolve_names resolve( &source, ast_script.get() );
        resolve.resolve();
    }

    if ( ! source.has_error )
    {
        kf::build_ir build_ir( &source );
        kf::fold_ir fold_ir( &source );
        kf::live_ir live_ir( &source );
        kf::foldk_ir foldk_ir( &source );

        for ( const auto& function : ast_script->functions )
        {
            std::unique_ptr< kf::ir_function > ir = build_ir.build( function.get() );
            if ( ! ir )
            {
                continue;
            }

            fold_ir.fold( ir.get() );
            live_ir.live( ir.get() );
            if ( foldk_ir.foldk( ir.get() ) )
            {
                live_ir.reset_live( ir.get() );
                live_ir.live( ir.get() );
            }

            ir->debug_print();
        }
    }

    for ( const kf::source_diagnostic& diagnostic : source.diagnostics )
    {
        fprintf
        (
            stderr,
            "%s:%u:%u: %s: %s\n",
            source.filename.c_str(),
            diagnostic.line_info.line,
            diagnostic.line_info.column,
            diagnostic.kind == kf::ERROR ? "error" : "warning",
            diagnostic.message.c_str()
        );
    }

    return EXIT_SUCCESS;
}

