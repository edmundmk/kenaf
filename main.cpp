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
#include <stdio.h>
#include <vector>
#include <kenaf/compile.h>
#include <kenaf/runtime.h>
#include <kenaf/handles.h>

int print_usage()
{
    fprintf( stderr, "usage: kenaf script\n" );
    return EXIT_FAILURE;
}

int main( int argc, char* argv[] )
{
    // Parse arguments.
    const char* filename = nullptr;
    unsigned debug_print = kf::PRINT_NONE;

    for ( int i = 1; i < argc; ++i )
    {
        if ( argv[ i ][ 0 ] == '-' )
        {
            const char* option = argv[ i ];
            if ( strcmp( option, "--ast-parsed" ) == 0 )
            {
                debug_print |= kf::PRINT_AST_PARSED;
            }
            if ( strcmp( option, "--ast-resolved" ) == 0 )
            {
                debug_print |= kf::PRINT_AST_RESOLVED;
            }
            if ( strcmp( option, "--ir-build" ) == 0 )
            {
                debug_print |= kf::PRINT_IR_BUILD;
            }
            if ( strcmp( option, "--ir-fold" ) == 0 )
            {
                debug_print |= kf::PRINT_IR_FOLD;
            }
            if ( strcmp( option, "--ir-live" ) == 0 )
            {
                debug_print |= kf::PRINT_IR_LIVE;
            }
            if ( strcmp( option, "--ir-foldk" ) == 0 )
            {
                debug_print |= kf::PRINT_IR_FOLDK;
            }
            if ( strcmp( option, "--ir-fold-live" ) == 0 )
            {
                debug_print |= kf::PRINT_IR_FOLD_LIVE;
            }
            if ( strcmp( option, "--ir-alloc" ) == 0 )
            {
                debug_print |= kf::PRINT_IR_ALLOC;
            }
            if ( strcmp( option, "--code" ) == 0 )
            {
                debug_print |= kf::PRINT_CODE;
            }
        }
        else
        {
            if ( filename ) return print_usage();
            filename = argv[ i ];
        }
    }

    // Load script source.
    if ( ! filename ) return print_usage();

    FILE* file = fopen( filename, "rb" );
    if ( ! file )
    {
        fprintf( stderr, "cannot open script file '%s'\n", filename );
        return EXIT_FAILURE;
    }

    fseek( file, 0, SEEK_END );
    std::vector< char > text( (size_t)ftell( file ) );
    fseek( file, 0, SEEK_SET );
    fread( text.data(), 1, text.size(), file );
    fclose( file );

    // Compile script.
    kf::compilation_handle compilation( kf::compile( filename, std::string_view( text.data(), text.size() ), debug_print ) );

    size_t diagnostic_count = kf::diagnostic_count( compilation.get() );
    for ( size_t i = 0; i < diagnostic_count; ++i )
    {
        kf::diagnostic d = kf::get_diagnostic( compilation.get(), i );
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

    if ( ! kf::success( compilation.get() ) )
    {
        return EXIT_FAILURE;
    }

    // Skip execution if we printed something.
    if ( debug_print != kf::PRINT_NONE )
    {
        return EXIT_SUCCESS;
    }

    // Execute script.
    kf::runtime_handle runtime = kf::make_runtime();
    kf::context_handle context = kf::make_context( runtime.get() );
    kf::make_current( context.get() );

    kf::code_view code = kf::get_code( compilation.get() );
    kf::value main = kf::create_function( code.code, code.size );
    compilation.reset();

    return EXIT_SUCCESS;
}

