//
//  resolve_names.cpp
//
//  Created by Edmund Kapusniak on 30/09/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "resolve_names.h"

namespace kf
{

resolve_names::resolve_names( source* source, syntax_tree* syntax_tree )
    :   _source( source )
    ,   _syntax_tree( syntax_tree )
{
}

resolve_names::~resolve_names()
{
}

void resolve_names::resolve()
{

}

void resolve_names::visit( syntax_function* f, unsigned index )
{
    // Get node.
    const syntax_node& n = f->nodes[ index ];

    //


    // Visit children.
    for ( unsigned c = n.child_index; c < index; c = f->nodes[ c ].next_index )
    {
        visit( f, c );
    }
}

}

