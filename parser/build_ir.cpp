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
    _loop_stack.push_back( { _block } );

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
    if ( ! _block )
    {
        // Unreachable.
        return { IR_O_NONE };
    }

    switch ( node->kind )
    {
    case AST_BLOCK:
    {
        visit_list( child_node( node ), node );
        if ( node->leaf_index().index != AST_INVALID_INDEX )
        {
            ir_operand o[ 1 ];
            o[ 0 ] = { IR_O_VM_INDEX, node->leaf_index().index };
            op( node->sloc, IR_CLOSE_UPSTACK, o, 1 );
        }
        break;
    }

    case AST_STMT_VAR:
    case AST_ASSIGN:
    {
        // TODO.
        break;
    }

    case AST_OP_ASSIGN:
    {
        // TODO.
        break;
    }

    case AST_STMT_IF:
    {
        node_index expr = child_node( node );
        node_index body = next_node( expr );
        node_index next = next_node( body );

        ir_block* endif_block = nullptr;

        while ( true )
        {
            // Perform test.
            ir_block* test_block = test( visit( expr ) );

            // Construct code for body.
            _block = link_next_block( test_block, make_block() );
            visit( body );
            if ( _block )
            {
                endif_block = link_next_block( _block, jump_block( endif_block ) );
            }

            // Link fail from original test block.
            _block = test_block;

            // Repeat for elifs.
            if ( next.index < node.index && next->kind == AST_ELIF )
            {
                _block = link_fail_block( _block, make_block() );
                expr = child_node( next );
                body = next_node( expr );
                next = next_node( next );
                continue;
            }
            else
            {
                break;
            }
        }

        // Check for else.
        if ( next.index < node.index )
        {
            assert( next->kind == AST_BLOCK );
            _block = link_fail_block( _block, make_block() );
            visit( next );
            if ( _block )
            {
                endif_block = link_next_block( _block, jump_block( endif_block ) );
            }
        }
        else
        {
            endif_block = link_fail_block( _block, jump_block( endif_block ) );
        }

        _block = endif_block;
        break;
    }

    case AST_STMT_FOR_STEP:
    {
        break;
    }

    case AST_STMT_FOR_EACH:
    {
        break;
    }

    case AST_STMT_WHILE:
    {
        node_index expr = child_node( node );
        node_index body = next_node( expr );

        // Open loop with test.
        _block = link_next_block( _block, jump_block() );
        _block->loop_kind = IR_LOOP_WHILE;
        size_t loop_index = _loop_stack.size();
        _loop_stack.push_back( { _block, _block, nullptr } );

        // Perform test.
        ir_block* test_block = test( visit( expr ) );
        _loop_stack[ loop_index ].break_block = link_fail_block( test_block, jump_block( _loop_stack[ loop_index ].break_block ) );

        // Loop body.
        visit( body );
        assert( loop_index == _loop_stack.size() - 1 );

        // Repeat loop.
        if ( _block )
        {
            link_next_block( _block, _loop_stack[ loop_index ].loop );
        }

        // Break to here.
        _block = _loop_stack[ loop_index ].break_block;
        _block->loop = nullptr;
        _loop_stack.pop_back();
        break;
    }

    case AST_STMT_REPEAT:
    {
        node_index body = child_node( node );
        node_index expr = next_node( body );

        // Open loop.
        _block = link_next_block( _block, jump_block() );
        _block->loop_kind = IR_LOOP_REPEAT;
        size_t loop_index = _loop_stack.size();
        _loop_stack.push_back( { _block, nullptr, nullptr } );

        // Loop body.
        visit( body );
        assert( loop_index == _loop_stack.size() - 1 );

        // Continue to here.
        if ( _loop_stack[ loop_index ].continue_block )
        {
            _block = link_next_block( _block, jump_block( _loop_stack[ loop_index ].continue_block ) );
        }

        if ( _block )
        {
            ir_block* test_block = test( visit( expr ) );
            link_fail_block( test_block, _loop_stack[ loop_index ].loop );
            _loop_stack[ loop_index ].break_block = link_next_block( test_block, jump_block( _loop_stack[ loop_index ].break_block ) );
        }

        // Break to here.
        _block = _loop_stack[ loop_index ].break_block;
        _block->loop = nullptr;
        _loop_stack.pop_back();
        break;
    }

    case AST_STMT_BREAK:
    {
        // TODO.
        break;
    }

    case AST_STMT_CONTINUE:
    {
        // TODO.
        break;
    }

    case AST_STMT_RETURN:
    {
        // TODO.
        break;
    }

    case AST_STMT_THROW:
    {
        // TODO.
        break;
    }

    case AST_EXPR_YIELD:
    {
        // TODO.
        break;
    }

    case AST_EXPR_YIELD_FOR:
    {
        // TODO.
        break;
    }

    case AST_EXPR_NULL:
    {
        return { IR_O_NULL };
    }

    case AST_EXPR_FALSE:
    {
        return { IR_O_FALSE };
    }

    case AST_EXPR_TRUE:
    {
        return { IR_O_TRUE };
    }

    case AST_EXPR_NUMBER:
    {
        double n = node->leaf_number().n;
        int8_t i = (int8_t)n;
        if ( i == n )
        {
            return ir_pack_integer_operand( i );
        }
        else
        {
            return { IR_O_AST_NUMBER, node.index };
        }
    }

    case AST_EXPR_STRING:
    {
        return { IR_O_AST_STRING, node.index };
    }

    case AST_EXPR_KEY:
    {
        ir_operand o[ 2 ];
        o[ 0 ] = visit( child_node( node ) );
        o[ 1 ] = { IR_O_AST_KEY, node.index };
        return { IR_O_OP, op( node->sloc, IR_GET_KEY, o, 2 ) };
    }

    case AST_EXPR_INDEX:
    {
        operand_pair o = visit_pair( child_node( node ), node );
        return { IR_O_OP, op( node->sloc, IR_GET_INDEX, o.o, 2 ) };
    }

    case AST_EXPR_CALL:
    {
        // TODO.
        break;
    }

    case AST_EXPR_LENGTH:
    {
        ir_operand o = visit( child_node( node ) );
        return { IR_O_OP, op( node->sloc, IR_LENGTH, &o, 1 ) };
    }

    case AST_EXPR_NEG:
    {
        ir_operand o = visit( child_node( node ) );
        return { IR_O_OP, op( node->sloc, IR_NEG, &o, 1 ) };
    }

    case AST_EXPR_POS:
    {
        ir_operand o = visit( child_node( node ) );
        return { IR_O_OP, op( node->sloc, IR_POS, &o, 1 ) };
    }

    case AST_EXPR_BITNOT:
    {
        ir_operand o = visit( child_node( node ) );
        return { IR_O_OP, op( node->sloc, IR_BITNOT, &o, 1 ) };
    }

    default: break;
    }

    return { IR_O_NONE };
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
    return { { a, b } };
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
    return check_number( operands.o[ 0 ], &n[ 0 ] ) && check_number( operands.o[ 1 ], &n[ 1 ] );
}

ir_block* build_ir::make_block()
{
    std::unique_ptr< ir_block > block = std::make_unique< ir_block >();
    block->block_index = _ir_function->blocks.size();
    _ir_function->blocks.push_back( std::move( block ) );
    return _ir_function->blocks.back().get();
}

ir_block* build_ir::jump_block( ir_block* block )
{
    if ( block )
    {
        return block;
    }

    if ( _block && _block->ops.empty() && block->test.kind == IR_O_NONE )
    {
        return _block;
    }

    return make_block();
}

ir_block* build_ir::link_next_block( ir_block* block, ir_block* next )
{
    if ( block == next )
    {
        assert( block->ops.empty() );
        return next;
    }

    assert( block->next_block == nullptr );
    block->next_block = next;
    next->predecessors.push_back( block );
    return next;
}

ir_block* build_ir::link_fail_block( ir_block* block, ir_block* fail )
{
    if ( block == fail )
    {
        assert( block->ops.empty() );
        return fail;
    }

    assert( block->fail_block == nullptr );
    block->fail_block = fail;
    fail->predecessors.push_back( block );
    return fail;
}

ir_block* build_ir::test( ir_operand operand )
{
    // TODO: this is a use of the operand.
    assert( _block->test.kind == IR_O_NONE );
    _block->test = operand;
    return _block;
}

unsigned build_ir::op( srcloc sloc, ir_opcode opcode, ir_operand* o, unsigned ocount, bool head )
{
    assert( _block->next_block == nullptr );

    ir_op op;
    op.opcode = opcode;
    op.operand_count = ocount;
    op.operands = ocount ? _block->operands.size() : IR_INVALID_INDEX;
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

    for ( unsigned i = 0; i < ocount; ++i )
    {
        _block->operands.push_back( o[ i ] );
    }

    return op_index;
}

void build_ir::def( unsigned local_index, ir_block* block, unsigned op_index )
{
    _def_map.insert_or_assign( std::make_pair( local_index, block ), op_index );
}

}

