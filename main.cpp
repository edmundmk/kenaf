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
#include "parser/ast_resolve.h"
#include "parser/ir_build.h"
#include "parser/ir_fold.h"
#include "parser/ir_live.h"
#include "parser/ir_foldk.h"
#include "parser/ir_alloc.h"

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
        kf::ast_resolve resolve( &source, ast_script.get() );
        resolve.resolve();
        ast_script->debug_print();
    }

    if ( ! source.has_error )
    {
        kf::ir_build ir_build( &source );
        kf::ir_fold ir_fold( &source );
        kf::ir_live ir_live( &source );
        kf::ir_foldk ir_foldk( &source );
        kf::ir_alloc ir_alloc( &source );

        for ( const auto& function : ast_script->functions )
        {
            std::unique_ptr< kf::ir_function > ir = ir_build.build( function.get() );
            if ( ! ir )
            {
                continue;
            }

            ir_fold.fold( ir.get() );
            ir_live.live( ir.get() );
            ir_foldk.foldk( ir.get() );
            ir_live.live( ir.get() );
//            ir_alloc.alloc( ir.get() );

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

