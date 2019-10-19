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
#include <kenaf/compiler.h>

int main( int argc, char* argv[] )
{
    if ( argc < 2 )
    {
        fprintf( stderr, "usage: kenaf script\n" );
        return EXIT_FAILURE;
    }

    FILE* file = fopen( argv[ 1 ], "rb" );
    fseek( file, 0, SEEK_END );
    std::vector< char > text( (size_t)ftell( file ) );
    fseek( file, 0, SEEK_SET );
    fread( text.data(), 1, text.size(), file );
    fclose( file );

    unsigned debug_print = kf::PRINT_AST_RESOLVED | kf::PRINT_IR_ALLOC;
    kf::compile_result result = kf::compile( argv[ 1 ], std::string_view( text.data(), text.size() ), debug_print );

    if ( result )
    {
        result.code()->debug_print();
    }

    for ( size_t i = 0; i < result.diagnostic_count(); ++i )
    {
        const kf::diagnostic& d = result.diagnostic( i );
        fprintf
        (
            stderr,
            "%s:%u:%u: %s: %s\n",
            argv[ 1 ],
            d.line,
            d.column,
            d.kind == kf::ERROR ? "error" : "warning",
            d.message.c_str()
        );
    }

    return result ? EXIT_SUCCESS : EXIT_FAILURE;
}

