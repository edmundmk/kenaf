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

int main( int argc, char* argv[] )
{
    kf::source source;
    source.filename = "[stdin]";
    std::unique_ptr< char[] > buffer( new char[ 1024 ] );
    while ( true )
    {
        size_t read = fread( buffer.get(), 1, 1024, stdin );
        source.append( buffer.get(), read );
        if ( read < 1024 )
            break;
    }
    buffer.reset();

    kf::lexer lexer( &source );
    kf::parser parser( &source, &lexer );
    std::unique_ptr< kf::ast_script > ast_script = parser.parse();

    if ( ! source.has_error )
    {
        for ( const auto& function : ast_script->functions )
        {
            function->debug_print();
        }

        kf::resolve_names resolve( &source, ast_script.get() );
        resolve.resolve();

        kf::build_ir build_ir;
        for ( const auto& function : ast_script->functions )
        {
            function->debug_print();
            std::unique_ptr< kf::ir_function > ir = build_ir.build( function.get() );
            ir->debug_print();
        }
    }

    for ( const kf::source_diagnostic& diagnostic : source.diagnostics )
    {
        fprintf( stderr, "%s:%u:%u: error: %s\n", source.filename.c_str(), diagnostic.line_info.line, diagnostic.line_info.column, diagnostic.message.c_str() );
    }

    return EXIT_SUCCESS;
}

