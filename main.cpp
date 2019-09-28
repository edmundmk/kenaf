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
#include "source.h"
#include "lexer.h"

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
    while ( true )
    {
        kf::token token = lexer.lex();
        kf::source_location location = source.location( token.sloc );
        if ( token.kind != TOKEN_NUMBER )
        {
            printf( "%s:%u:%u: '%.*s'\n", source.filename.c_str(), location.line, location.column, (int)token.size, token.text );
        }
        else
        {
            printf( "%s:%u:%u: %f\n", source.filename.c_str(), location.line, location.column, token.n );
        }
        if ( token.kind == TOKEN_EOF )
        {
            break;
        }
    }

    for ( const kf::source_diagnostic& diagnostic : source.diagnostics )
    {
        fprintf( stderr, "%s:%u:%u: error: %s\n", source.filename.c_str(), diagnostic.line_info.line, diagnostic.line_info.column, diagnostic.message.c_str() );
    }

    return EXIT_SUCCESS;
}

