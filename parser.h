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

namespace kf
{

class parser
{
public:

    void syntax_error( token token );

};

}

#endif

