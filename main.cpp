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
#include <string.h>
#include <vector>
#include <kenaf/compiler.h>
#include <kenaf/runtime.h>
#include <kenaf/handles.h>
#include <kenaf/errors.h>

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

    int i = 1;
    for ( ; i < argc; ++i )
    {
        if ( argv[ i ][ 0 ] == '-' )
        {
            const char* option = argv[ i ];
            if ( strcmp( option, "--ast" ) == 0 )
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
            filename = argv[ i ];
            break;
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
    kf::compiler_handle compiler = kf::make_compiler();
    kf::debug_print( compiler.get(), debug_print );
    bool success = kf::compile( compiler.get(), filename, std::string_view( text.data(), text.size() ) );

    size_t diagnostic_count = kf::diagnostic_count( compiler.get() );
    for ( size_t i = 0; i < diagnostic_count; ++i )
    {
        kf::diagnostic d = kf::get_diagnostic( compiler.get(), i );
        fprintf
        (
            stderr,
            "%s:%u:%u: %s: %.*s\n",
            filename,
            d.line,
            d.column,
            d.kind == kf::ERROR ? "error" : "warning",
            (int)d.message.size(), d.message.data()
        );
    }

    if ( ! success )
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

    kf::value main = kf::create_function( kf::compiled_code( compiler.get() ), kf::compiled_size( compiler.get() ) );
    compiler.reset();

    // Executes script, passing remaining command line arguments.
    try
    {
        kf::scoped_frame frame;
        kf::stack_values arguments = kf::push_frame( frame, argc - i );
        size_t argindex = 0;
        while ( i < argc )
        {
            arguments.values[ argindex++ ] = kf::create_string( argv[ i++ ] );
        }

        kf::stack_values results = kf::call_frame( frame, main );
        int result = EXIT_SUCCESS;
        if ( results.count && is_number( results.values[ 0 ] ) )
        {
            result = (int)get_number( results.values[ 0 ] );
        }

        kf::pop_frame( frame );
        return result;
    }
    catch ( const kf::script_error& e )
    {
        fprintf( stderr, "%s\n", e.what() );
        for ( size_t i = 0; i < e.stack_trace_count(); ++i )
        {
            fprintf( stderr, "    %s\n", e.stack_trace( i ) );
        }
        return EXIT_FAILURE;
    }
}

