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
    ,   is_generator( false )
    ,   is_top_level( false )
{
}

syntax_function::~syntax_function()
{
}

void syntax_function::fixup_nodes()
{
    // Calculate next sibling pointers.
    for ( unsigned index = 0; index < nodes.size(); ++index )
    {
        // Link last child node to its parent.
        if ( index )
        {
            unsigned last_index = index - 1;
            nodes[ last_index ].next_index = index;
        }

        // Find oldest descendant.
        unsigned node_index = index;
        unsigned child_index = nodes[ node_index ].child_index;
        while ( child_index != node_index )
        {
            node_index = child_index;
            child_index = nodes[ node_index ].child_index;
        }

        // Previous node is the node before the oldest descendant.
        if ( child_index )
        {
            unsigned prev_index = child_index - 1;
            nodes[ prev_index ].next_index = index;
        }

        // Skip leaf data.
        if ( nodes[ index ].leaf )
        {
            ++index;
        }
    }
}

}

