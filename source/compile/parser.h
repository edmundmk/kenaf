//
//  parser.h
//
//  Created by Edmund Kapusniak on 28/09/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KF_PARSER_H
#define KF_PARSER_H

#include <vector>
#include "lexer.h"
#include "ast.h"

namespace kf
{

class parser
{
public:

    parser( source* source, lexer* lexer );
    ~parser();

    std::unique_ptr< ast_script > parse();

    void syntax_error( token token );
    void error( srcloc sloc, const char* message, ... ) PRINTF_FORMAT( 3, 4 );

    ast_function* push_function( srcloc sloc );
    void pop_function();

    srcloc current_sloc();
    srcloc node_sloc( unsigned index );
    ast_node_kind node_kind( unsigned index );
    void update_sloc( unsigned index, srcloc sloc );

    unsigned node( ast_node_kind kind, srcloc sloc, unsigned child );
    unsigned string_node( ast_node_kind kind, srcloc sloc, const char* text, unsigned size );
    unsigned string_node( ast_node_kind kind, srcloc sloc, unsigned child, const char* text, unsigned size );
    unsigned number_node( ast_node_kind kind, srcloc sloc, double n );
    unsigned function_node( ast_node_kind kind, srcloc sloc, ast_function* function );
    unsigned index_node( ast_node_kind kind, srcloc sloc, unsigned child );

    std::string qual_name_string( unsigned index );

    void check_generator( srcloc sloc );

private:

    source* _source;
    lexer* _lexer;
    void* _yyp;
    token _token;

    std::unique_ptr< ast_script > _ast_script;
    std::vector< ast_function* > _fstack;

};

}

#endif

