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
#include <vector>
#include <kenaf/compiler.h>

int main( int argc, char* argv[] )
{
    if ( argc < 2 )
    {
        fprintf( stderr, "usage: kenaf script\n" );
        return EXIT_FAILURE;
    }

    FILE* file = fopen( argv[ 1 ], "rb" );
    if ( ! file )
    {
        fprintf( stderr, "cannot open script file '%s'\n", argv[ 1 ] );
        return EXIT_FAILURE;
    }

    fseek( file, 0, SEEK_END );
    std::vector< char > text( (size_t)ftell( file ) );
    fseek( file, 0, SEEK_SET );
    fread( text.data(), 1, text.size(), file );
    fclose( file );

    unsigned debug_print = kf::PRINT_AST_RESOLVED | kf::PRINT_IR_ALLOC;
    kf::compilation* compilation = kf::compile( argv[ 1 ], std::string_view( text.data(), text.size() ), debug_print );

    size_t diagnostic_count = kf::compilation_diagnostic_count( compilation );
    for ( size_t i = 0; i < diagnostic_count; ++i )
    {
        kf::diagnostic d = compilation_diagnostic( compilation, i );
        fprintf
        (
            stderr,
            "%s:%u:%u: %s: %.*s\n",
            argv[ 1 ],
            d.line,
            d.column,
            d.kind == kf::ERROR ? "error" : "warning",
            (int)d.message.size(), d.message.data()
        );
    }

    if ( ! kf::compilation_success( compilation ) )
    {
        return EXIT_FAILURE;
    }

    kf::code_view code = compilation_code( compilation );
    kf::debug_print_code( code.code, code.size );

    return EXIT_SUCCESS;
}

