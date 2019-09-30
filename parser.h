//
//  parser.h
//
//  Created by Edmund Kapusniak on 28/09/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef PARSER_H
#define PARSER_H

#include <vector>
#include "lexer.h"
#include "syntax.h"

namespace kf
{

class parser
{
public:

    parser( source* source, lexer* lexer );
    ~parser();

    std::unique_ptr< syntax_tree > parse();

    void syntax_error( token token );
    void error( srcloc sloc, const char* message, ... ) PRINTF_FORMAT( 3, 4 );

    syntax_function* push_function( srcloc sloc );
    void pop_function();

    srcloc current_sloc();
    srcloc node_sloc( unsigned index );
    void update_sloc( unsigned index, srcloc sloc );

    unsigned node( syntax_node_kind kind, srcloc sloc, unsigned child );
    unsigned string_node( syntax_node_kind kind, srcloc sloc, const char* text, unsigned size );
    unsigned number_node( syntax_node_kind kind, srcloc sloc, double n );
    unsigned function_node( syntax_node_kind kind, srcloc sloc, syntax_function* function );
    unsigned index_node( syntax_node_kind kind, srcloc sloc, unsigned child );

    std::string qual_name_string( unsigned index );

private:

    source* _source;
    lexer* _lexer;
    void* _yyp;
    token _token;

    std::unique_ptr< syntax_tree > _syntax_tree;
    std::vector< syntax_function* > _fstack;

};

}

#endif

