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


private:

    source* _source;
    lexer* _lexer;
    void* _yyp;

    std::unique_ptr< syntax_tree > _syntax_tree;
    syntax_function* _syntax_function;

};

}

#endif

