//
//  syntax.cpp
//
//  Created by Edmund Kapusniak on 28/09/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "syntax.h"

namespace kf
{

syntax_tree::syntax_tree()
{
}

syntax_tree::~syntax_tree()
{
}

syntax_function* syntax_tree::new_function( srcloc sloc, syntax_function* outer )
{
    functions.push_back( std::make_unique< syntax_function >( sloc, outer ) );
    return functions.back().get();
}

syntax_function::syntax_function( srcloc sloc, syntax_function* outer )
    :   sloc( sloc )
    ,   outer( outer )
    ,   implicit_self( false )
    ,   is_generator( true )
{
}

syntax_function::~syntax_function()
{
}

}

