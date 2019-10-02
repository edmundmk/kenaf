//
//  build_icode.cpp
//
//  Created by Edmund Kapusniak on 02/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "build_icode.h"
#include "syntax.h"

namespace kf
{

build_icode::build_icode()
    :   _ast_function( nullptr )
{
}

build_icode::~build_icode()
{
}

std::unique_ptr< icode_function > build_icode::build( syntax_function* function )
{
    // Set up for building.
    _ast_function = function;
    _ir_function = std::make_unique< icode_function >();
    _ir_function->ast = function;

    // Create initial block.
    _ir_function->blocks.push_back( std::make_unique< icode_block >() );
    _block = _ir_function->blocks.back().get();
    _block->function = _ir_function.get();

    // Visit AST.
    visit( _ast_function->nodes.size() - 1 );

    // Done.
    _ast_function = nullptr;
    _def_map.clear();
    return std::move( _ir_function );
}

void build_icode::visit( unsigned ast_index )
{
    syntax_node* n = &_ast_function->nodes[ ast_index ];

    switch ( n->kind )
    {


    default: break;
    }

    for ( unsigned c = n->child_index; c < ast_index; c = _ast_function->nodes[ c ].next_index )
    {
        visit( c );
    }

    switch ( n->kind )
    {

    default: break;
    }
}

void build_icode::def( unsigned local_index, icode_block* block, unsigned op_index )
{
    _def_map.insert_or_assign( std::make_pair( local_index, block ), op_index );
}

}

