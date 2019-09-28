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
#include "parser.h"

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
    parser.parse();

    for ( const kf::source_diagnostic& diagnostic : source.diagnostics )
    {
        fprintf( stderr, "%s:%u:%u: error: %s\n", source.filename.c_str(), diagnostic.line_info.line, diagnostic.line_info.column, diagnostic.message.c_str() );
    }

    return EXIT_SUCCESS;
}

