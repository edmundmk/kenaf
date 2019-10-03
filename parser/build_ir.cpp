//
//  build_ir.cpp
//
//  Created by Edmund Kapusniak on 02/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "build_ir.h"

namespace kf
{

build_ir::build_ir()
    :   _ast_function( nullptr )
    ,   _block( nullptr )
{
}

build_ir::~build_ir()
{
}

std::unique_ptr< ir_function > build_ir::build( ast_function* function )
{
    // Set up for building.
    _ast_function = function;
    _ir_function = std::make_unique< ir_function >();
    _ir_function->ast = function;

    // Create initial block.
    _block = make_block();
    _loop_stack.push_back( { _block, nullptr } );

    // Visit AST.
    node_index node = { &_ast_function->nodes.back(), (unsigned)_ast_function->nodes.size() - 1 };
    visit( node );

    // Done.
    _ast_function = nullptr;
    _v.clear();
    _def_map.clear();
    return std::move( _ir_function );
}

build_ir::node_index build_ir::child_node( node_index node )
{
    unsigned child_index = node.node->child_index;
    return { &_ast_function->nodes[ child_index ], child_index };
}

build_ir::node_index build_ir::next_node( node_index node )
{
    unsigned next_index = node.node->next_index;
    return { &_ast_function->nodes[ next_index ], next_index };
}

ir_operand build_ir::visit( node_index node )
{
    double n[ 2 ];

    switch ( node->kind )
    {
    case AST_STMT_VAR:
        // TODO.
        break;

    case AST_STMT_IF:
    {
        node_index expr = child_node( node );
        node_index body = next_node( expr );
        node_index next = next_node( body );

        ir_block* endif_block = nullptr;
        while ( true )
        {
            // Evaluate test.  TODO: Ensure this is an actual use of operand.
            assert( _block->test.kind == IR_O_NONE );
            _block->test = visit( expr );

            // Construct code for body.
            ir_block* test_block = _block;
            _block = link_next_block( test_block, make_block() );
            visit( body );

            // End of body is endif.
            if ( _block )
            {
                if ( ! endif_block )
                    endif_block = jump_block( _block );
                link_next_block( _block, endif_block );
            }

            // Repeat for elifs.
            if ( next.index < node.index && next->kind == AST_ELIF )
            {
                _block = link_fail_block( test_block, make_block() );
                expr = child_node( next );
                body = next_node( expr );
                next = next_node( next );
                continue;
            }
            else
            {
                _block = test_block;
                break;
            }
        }

        // Check for else.


    }

    default: break;
    }
}

build_ir::operand_pair build_ir::visit_pair( node_index node, node_index last )
{
    // Visit lhs.
    assert( node.index < last.index );
    ir_operand a = visit( node );
    assert( a.kind != IR_O_NONE );
    node = next_node( node );

    // Visit rhs.
    assert( node.index < last.index );
    ir_operand b = visit( node );
    assert( b.kind != IR_O_NONE );
    node = next_node( node );

    // Done.
    assert( node.index == last.index );
    return { a, b };
}

unsigned build_ir::visit_eval( node_index node, node_index last )
{
    // Visit each node, pushing onto eval stack.
    unsigned count = 0;
    for ( ; node.index < last.index; node = next_node( node ) )
    {
        ir_operand operand = visit( node );
        assert( operand.kind != IR_O_NONE );
        _v.push_back( operand );
        count += 1;
    }
    return count;
}

void build_ir::visit_list( node_index node, node_index last )
{
    // Visit each node, throwing away result.
    for ( ; node.index < last.index; node = next_node( node ) )
    {
        visit( node );
    }
}

bool build_ir::check_number( ir_operand operand, double* n )
{
    if ( operand.kind == IR_O_INTEGER )
    {
        *n = ir_unpack_integer_operand( operand );
        return true;
    }
    if ( operand.kind == IR_O_AST_NUMBER )
    {
        *n = _ast_function->nodes.at( operand.index ).leaf_number().n;
        return true;
    }
    return false;
}

bool build_ir::check_number( operand_pair operands, double n[ 2 ] )
{
    return check_number( operands.a, &n[ 0 ] ) && check_number( operands.b, &n[ 1 ] );
}

ir_block* build_ir::make_block( ir_loop_kind loop_kind )
{
    // TODO.
    return nullptr;
}

ir_block* build_ir::jump_block( ir_block* block )
{
    // TODO.
    return nullptr;
}

ir_block* build_ir::link_next_block( ir_block* block, ir_block* next )
{
    // TODO.
    return nullptr;
}

ir_block* build_ir::link_fail_block( ir_block* block, ir_block* fail )
{
    // TODO.
    return nullptr;
}

unsigned build_ir::op( srcloc sloc, ir_opcode opcode, unsigned operand_count, bool head )
{
    assert( _block->next_block == nullptr );

    ir_op op;
    op.opcode = opcode;
    op.operand_count = operand_count;
    op.operands = operand_count ? _block->operands.size() : IR_INVALID_INDEX;
    op.sloc = sloc;

    unsigned op_index;
    if ( ! head )
    {
        op_index = _block->ops.body_size();
        _block->ops.push_body( op );
    }
    else
    {
        op_index = _block->ops.head_size();
        _block->ops.push_head( op );
    }

    // TODO: REMOVE THIS.
    operand_count = std::min< unsigned >( _v.size(), operand_count );

    assert( _v.size() >= operand_count );
    for ( unsigned i = _v.size() - operand_count; i < _v.size(); ++i )
    {
        _block->operands.push_back( _v.at( i ) );
    }
    _v.resize( _v.size() - operand_count );

    return op_index;
}

void build_ir::def( unsigned local_index, ir_block* block, unsigned op_index )
{
    _def_map.insert_or_assign( std::make_pair( local_index, block ), op_index );
}

}

