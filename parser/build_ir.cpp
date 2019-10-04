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
#include "../common/k_math.h"

namespace kf
{

build_ir::build_ir( source* source )
    :   _source( source )
{
}

build_ir::~build_ir()
{
}

std::unique_ptr< ir_function > build_ir::build( ast_function* function )
{
    // Set up for building.
    _f = std::make_unique< ir_function >();
    _f->ast = function;

    // Visit AST.
    node_index node = { &_f->ast->nodes.back(), (unsigned)_f->ast->nodes.size() - 1 };
    visit( node );

    // Done.
    return std::move( _f );
}

build_ir::node_index build_ir::child_node( node_index node )
{
    unsigned child_index = node.node->child_index;
    return { &_f->ast->nodes[ child_index ], child_index };
}

build_ir::node_index build_ir::next_node( node_index node )
{
    unsigned next_index = node.node->next_index;
    return { &_f->ast->nodes[ next_index ], next_index };
}


ir_operand build_ir::visit( node_index node )
{
    switch ( node->kind )
    {
/*

    case AST_FUNCTION:
    {
        block_head( node->sloc );
        visit_children( node );
        ir_operand o[ 0 ];
        o[ 0 ] = { IR_O_NULL };
        emit( node->sloc, IR_BLOCK_RETURN, o, 1 );
        break;
    }

    case AST_BLOCK:
    {
        visit_children( node );
        close_upstack( node );
        break;
    }

//  case AST_STMT_VAR: TODO

    case AST_STMT_IF:
    {
        node_index expr = child_node( node );
        node_index body = next_node( expr );
        node_index next = next_node( body );

        test_fixup link;
        size_t endif_index = _jump_endif.size();

        while ( true )
        {
            // Test ends current block.
            link = block_test( node->sloc, visit( expr ) );

            // Body of if.
            fixup( link.if_true, block_head( body->sloc ) );
            visit( body );
            _jump_endif.push_back( block_jump( body->sloc ) );

            if ( next.index < node.index && next->kind == AST_STMT_ELIF )
            {
                fixup( link.if_false, block_head( next->sloc ) );
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

        if ( next.index < node.index )
        {
            // Else clause.
            assert( next->kind == AST_BLOCK );
            fixup( link.if_false, block_head( next->sloc ) );
            visit( next );
            _jump_endif.push_back( block_jump( next->sloc ) );
        }
        else
        {
            _jump_endif.push_back( link.if_false );
        }

        // Link all bodies to endif.
        unsigned endif = block_head( node->sloc );
        for ( size_t i = endif_index; i < _jump_endif.size(); ++i )
        {
            fixup( _jump_endif[ i ], endif );
        }
        _jump_endif.resize( endif_index );

        break;
    }

    case AST_STMT_FOR_STEP:
    {
        node_index decl = child_node( node );
        node_index start = next_node( decl );
        node_index limit = next_node( start );
        node_index step = next_node( limit );
        node_index body = next_node( step );

        // Evaluate start : limit : step
        ir_operand o[ 3 ];
        o[ 0 ] = visit( start );
        o[ 1 ] = visit( limit );
        o[ 2 ] = visit( step );
        // TODO: assign i from start

        // Close current block.
        jump_fixup link_previous = block_jump( node->sloc );

        // Mark break/continue stacks.
        size_t continue_index = _jump_continue.size();
        size_t break_index = _jump_break.size();

        // Loop header including condition.
        unsigned loop_continue = block_head( node->sloc );
        fixup( link_previous, loop_continue );
        // TODO: Make phi/some kind of loop var for i/limit/step?
        test_fixup link_loop = block_test( node->sloc, emit( node->sloc, IR_FOR_STEP, o, 3 ) );
        _jump_break.push_back( link_loop.if_false );

        // Loop body.
        fixup( link_loop.if_true, block_head( body->sloc ) );
        // TODO: assign decl from i
        visit( body );
        _jump_continue.push_back( block_jump( body->sloc ) );

        // Fixup break/continue.
        unsigned loop_break = block_head( node->sloc );
        break_continue( break_index, loop_break, continue_index, loop_continue );
        break;
    }

    case AST_STMT_FOR_EACH:
    {
        node_index decl = child_node( node );
        node_index expr = next_node( decl );
        node_index body = next_node( expr );

        // Evaluate generator expression and construct generator from it.
        ir_operand o[ 1 ];
        o[ 0 ] = visit( expr );
        o[ 0 ] = emit( node->sloc, IR_GENERATE, o, 1 );
        // TODO: assign g/i from result of generate?

        // Close current block.
        jump_fixup link_previous = block_jump( node->sloc );

        // Mark break/continue stacks.
        size_t continue_index = _jump_continue.size();
        size_t break_index = _jump_break.size();

        // Loop header including condition.
        unsigned loop_continue = block_head( node->sloc );
        fixup( link_previous, loop_continue );
        // TODO: make phi/some kind of loop var for g/i
        test_fixup link_loop = block_test( node->sloc, emit( node->sloc, IR_FOR_EACH, o, 1 ) );
        _jump_break.push_back( link_loop.if_false );

        // Loop body.
        fixup( link_loop.if_true, block_head( body->sloc ) );
        // TODO: assign decls from unpacking g/i
        visit( body );
        _jump_continue.push_back( block_jump( body->sloc ) );

        // Fixup break/continue.
        unsigned loop_break = block_head( node->sloc );
        break_continue( break_index, loop_break, continue_index, loop_continue );
        break;
    }

    case AST_STMT_WHILE:
    {
        node_index expr = child_node( node );
        node_index body = next_node( expr );

        // Close previous block.
        jump_fixup link_previous = block_jump( node->sloc );

        // Mark break/continue stacks.
        size_t continue_index = _jump_continue.size();
        size_t break_index = _jump_break.size();

        // Loop header including condition.
        unsigned loop_continue = block_head( node->sloc );
        fixup( link_previous, loop_continue );
        test_fixup link_loop = block_test( node->sloc, visit( expr ) );
        _jump_break.push_back( link_loop.if_false );

        // Loop body.
        fixup( link_loop.if_true, block_head( body->sloc ) );
        visit( body );
        _jump_continue.push_back( block_jump( body->sloc ) );

        // Fixup break/continue.
        unsigned loop_break = block_head( node->sloc );
        break_continue( break_index, loop_break, continue_index, loop_continue );
        break;
    }

    case AST_STMT_REPEAT:
    {
        node_index body = child_node( node );
        node_index expr = next_node( body );

        // Close previous block.
        jump_fixup link_previous = block_jump( node->sloc );

        // Mark break/continue stacks.
        size_t continue_index = _jump_continue.size();
        size_t break_index = _jump_break.size();

        // Loop header/body.
        unsigned loop_header = block_head( node->sloc );
        fixup( link_previous, loop_header );
        visit( body );

        // Make continue block if there are any continues.
        unsigned loop_continue = IR_INVALID_INDEX;
        if ( continue_index < _jump_continue.size() )
        {
            jump_fixup link_previous = block_jump( node->sloc );
            loop_continue = block_head( node->sloc );
            fixup( link_previous, loop_continue );
        }

        // Loop condition.
        test_fixup link_loop = block_test( node->sloc, visit( expr ) );
        _jump_break.push_back( link_loop.if_true );
        fixup( link_loop.if_false, loop_header );

        // Fixup break/continue.
        unsigned loop_break = block_head( node->sloc );
        break_continue( break_index, loop_break, continue_index, loop_continue );
        break;
    }

    case AST_STMT_BREAK:
    {
        close_upstack( node );
        _jump_break.push_back( block_jump( node->sloc ) );
        break;
    }

    case AST_STMT_CONTINUE:
    {
        close_upstack( node );
        _jump_continue.push_back( block_jump( node->sloc ) );
        break;
    }

    case AST_STMT_RETURN:
    {
        if ( child_node( node ).index < node.index )
        {
            block_last( list_op( IR_BLOCK_RETURN, node ) );
        }
        else
        {
            ir_operand o[ 1 ];
            o[ 0 ] = { IR_O_NULL };
            block_last( emit( node->sloc, IR_BLOCK_RETURN, o, 1 ) );
        }
        break;
    }

    case AST_STMT_THROW:
    {
        ir_operand o[ 1 ];
        o[ 0 ] = visit( child_node( node ) );
        block_last( emit( node->sloc, IR_BLOCK_THROW, o, 1 ) );
        break;
    }

//  case AST_ASSIGN: TODO

    case AST_OP_ASSIGN:
    {
        node_index u = child_node( node );
        node_index op = next_node( u );
        node_index v = next_node( op );

        // TODO!

        visit( u );
        visit( v );

        return { IR_O_NONE };
    }

    case AST_EXPR_NULL:
        return emit( node->sloc,
    case AST_EXPR_FALSE:
        return { IR_O_FALSE };
    case AST_EXPR_TRUE:
        return { IR_O_TRUE };

    case AST_EXPR_NUMBER:
        return k_number( node->leaf_number().n );

    case AST_EXPR_NEG:
        return fold_unary( IR_NEG, node, []( double u ) { return -u; } );
    case AST_EXPR_POS:
        return fold_unary( IR_POS, node, []( double u ) { return +u; } );
    case AST_EXPR_BITNOT:
        return fold_unary( IR_BITNOT, node, []( double u ) { return k_bitnot( u ); } );

    case AST_EXPR_MUL:
        return fold_binary( IR_MUL, node, []( double u, double v ) { return u * v; } );
    case AST_EXPR_DIV:
        return fold_binary( IR_MUL, node, []( double u, double v ) { return u / v; } );
    case AST_EXPR_INTDIV:
        return fold_binary( IR_MUL, node, []( double u, double v ) { return k_floordiv( u, v ); } );
    case AST_EXPR_MOD:
        return fold_binary( IR_MUL, node, []( double u, double v ) { return k_floormod( u, v ); } );
    case AST_EXPR_ADD:
        return fold_binary( IR_MUL, node, []( double u, double v ) { return u + v; } );
    case AST_EXPR_SUB:
        return fold_binary( IR_MUL, node, []( double u, double v ) { return u - v; } );
    case AST_EXPR_LSHIFT:
        return fold_binary( IR_MUL, node, []( double u, double v ) { return k_lshift( u, v ); } );
    case AST_EXPR_RSHIFT:
        return fold_binary( IR_MUL, node, []( double u, double v ) { return k_rshift( u, v ); } );
    case AST_EXPR_ASHIFT:
        return fold_binary( IR_MUL, node, []( double u, double v ) { return k_ashift( u, v ); } );
    case AST_EXPR_BITAND:
        return fold_binary( IR_MUL, node, []( double u, double v ) { return k_bitand( u, v ); } );
    case AST_EXPR_BITXOR:
        return fold_binary( IR_MUL, node, []( double u, double v ) { return k_bitxor( u, v ); } );
    case AST_EXPR_BITOR:
        return fold_binary( IR_MUL, node, []( double u, double v ) { return k_bitor( u, v ); } );

    case AST_EXPR_STRING:
    {
        unsigned k = _f->k_strings.size();
        _f->k_strings.push_back( { node->leaf_string().text, node->leaf_string().size } );
        return { IR_O_K_STRING, k };
    }

    case AST_EXPR_CONCAT:
    {
        return list_op( IR_CONCAT, node );
    }

    case AST_NAME:
    {
        assert( ! "names should have been resolved" );
        return { IR_O_NONE };
    }

    case AST_EXPR_KEY:
    {
        ir_operand o[ 2 ];
        o[ 0 ] = visit( child_node( node ) );
        o[ 1 ] = { IR_O_AST_KEY, node.index };
        return emit( node->sloc, IR_GET_KEY, o, 2 );
    }

    case AST_EXPR_INDEX:
        return list_op( IR_GET_INDEX, node );
    case AST_EXPR_CALL:
        return list_op( IR_CALL, node );
    case AST_EXPR_LENGTH:
        return list_op( IR_LENGTH, node );
*/

    default:
        // TODO: remove this once all cases handled.
        visit_children( node );
        break;
    }

    return { IR_O_NONE };
}

void build_ir::visit_children( node_index node )
{
    for ( node_index child = child_node( node ); child.index < node.index; child = next_node( child ) )
    {
        visit( child );
    }
}

ir_operand build_ir::visit_children_op( ir_opcode opcode, node_index node )
{
    size_t oindex = _o.size();
    for ( node_index child = child_node( node ); child.index < node.index; child = next_node( child ) )
    {
        _o.push_back( visit( child ) );
    }
    ir_operand result_op = emit( node->sloc, opcode, _o.data() + oindex, _o.size() - oindex );
    _o.resize( oindex );
    return result_op;
}

ir_operand build_ir::emit( srcloc sloc, ir_opcode opcode, ir_operand* o, unsigned ocount )
{
    ir_op op;
    op.opcode = opcode;
    op.ocount = ocount;
    op.oindex = ocount ? _f->operands.size() : IR_INVALID_INDEX;
    op.sloc = sloc;

    unsigned op_index = _f->ops.size();
    _f->ops.push_back( op );

    for ( unsigned i = 0; i < ocount; ++i )
    {
        _f->operands.push_back( o[ i ] );
    }

    return { IR_O_OP, op_index };
}

void build_ir::close_upstack( node_index node )
{
    unsigned close_index = node->leaf_index().index;
    if ( close_index != AST_INVALID_INDEX )
    {
        ir_operand o[ 1 ];
        o[ 0 ] = { IR_O_UPSTACK_INDEX, close_index };
        emit( node->sloc, IR_CLOSE_UPSTACK, o, 1 );
    }
}

unsigned build_ir::block_head( srcloc sloc )
{
    return emit( sloc, IR_BLOCK_HEAD, nullptr, 0 ).index;
    // TODO: setup block.
    // TODO: reachable again?
}

build_ir::test_fixup build_ir::block_test( srcloc sloc, ir_operand test )
{
    unsigned oindex = _f->operands.size();
    ir_operand o[ 3 ];
    o[ 0 ] = test;
    o[ 1 ] = { IR_O_JUMP, IR_INVALID_INDEX };
    o[ 2 ] = { IR_O_JUMP, IR_INVALID_INDEX };
    block_last( emit( sloc, IR_BLOCK_TEST, o, 3 ) );
    return { { oindex + 1 }, { oindex + 2 } };
}

build_ir::jump_fixup build_ir::block_jump( srcloc sloc )
{
    unsigned oindex = _f->operands.size();
    ir_operand o[ 1 ];
    o[ 0 ] = { IR_O_JUMP, IR_INVALID_INDEX };
    block_last( emit( sloc, IR_BLOCK_JUMP, o, 1 ) );
    return { oindex };
}

void build_ir::block_last( ir_operand last_op )
{
    // TODO: unreachable now.
}

void build_ir::fixup( jump_fixup fixup, unsigned target )
{
    ir_operand* operand = &_f->operands.at( fixup.oindex );
    assert( operand->kind == IR_O_JUMP );
    operand->index = target;
}

void build_ir::break_continue( size_t break_index, unsigned loop_break, size_t continue_index, unsigned loop_continue )
{
    for ( size_t i = continue_index; i < _jump_continue.size(); ++i )
    {
        fixup( _jump_continue[ i ], loop_continue );
    }
    _jump_continue.resize( continue_index );

    for ( size_t i = break_index; i < _jump_break.size(); ++i )
    {
        fixup( _jump_break[ i ], loop_break );
    }
    _jump_break.resize( break_index );
}

}

