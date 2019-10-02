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
    _block->block_index = _ir_function->blocks.size() - 1;
    _block->function = _ir_function.get();

    // Declare parameters.

    // Visit AST.
    visit( _ast_function->nodes.size() - 1 );

    // Done.
    _ast_function = nullptr;
    _eval.clear();
    _def_map.clear();
    return std::move( _ir_function );
}

void build_icode::visit( unsigned ast_index )
{
    syntax_node* n = &_ast_function->nodes[ ast_index ];

    switch ( n->kind )
    {
    case AST_EXPR_NULL:
    {
        _eval.push_back( { IR_O_NULL } );
        return;
    }

    case AST_EXPR_FALSE:
    {
        _eval.push_back( { IR_O_FALSE } );
        return;
    }

    case AST_EXPR_TRUE:
    {
        _eval.push_back( { IR_O_TRUE } );
        return;
    }

    case AST_EXPR_NUMBER:
    {
        double f = n->leaf_number().n;
        int8_t i = (int8_t)f;
        if ( i == f )
            _eval.push_back( icode_pack_integer_operand( i ) );
        else
            _eval.push_back( { IR_O_AST_NUMBER, ast_index } );
        return;
    }

    case AST_EXPR_STRING:
    {
        _eval.push_back( { IR_O_AST_STRING, ast_index } );
        return;
    }

    case AST_UPVAL_NAME:
    case AST_UPVAL_NAME_SUPER:
    {
        _eval.push_back( { IR_O_UPVAL_INDEX, n->leaf_index().index } );
        _eval.push_back( { IR_O_VALUE, op( n->sloc, IR_GET_UPVAL, 1 ) } );
        if ( n->kind == AST_UPVAL_NAME_SUPER )
        {
            _eval.push_back( { IR_O_VALUE, op( n->sloc, IR_SUPEROF, 1 ) } );
        }
        return;
    }

    case AST_LOCAL_NAME:
    case AST_LOCAL_NAME_SUPER:
    {
        // TODO.  This should perform SSA construction to find defs of local.
        _eval.push_back( { IR_O_PARAM_INDEX, n->leaf_index().index } );
        if ( n->kind == AST_LOCAL_NAME_SUPER )
        {
            _eval.push_back( { IR_O_VALUE, op( n->sloc, IR_SUPEROF, 1 ) } );
        }
        return;
    }

    case AST_PARAMETERS:
    {
        // Declare parameters.
        for ( unsigned c = n->child_index; c < ast_index; c = _ast_function->nodes[ c ].next_index )
        {
            syntax_node* pn = &_ast_function->nodes[ c ];
            if ( pn->kind == AST_VARARG_PARAM )
            {
                continue;
            }

            assert( pn->kind == AST_LOCAL_DECL );
            unsigned local_index = pn->leaf_index().index;
            _eval.push_back( { IR_O_PARAM_INDEX, local_index } );
            def( local_index, _block, op( pn->sloc, IR_PARAM, 1, true ) );
        }
        return;
    }



    default: break;
    }

    unsigned child_count = 0;
    for ( unsigned c = n->child_index; c < ast_index; c = _ast_function->nodes[ c ].next_index )
    {
        child_count += 1;
        visit( c );
    }

    switch ( n->kind )
    {
    case AST_EXPR_LENGTH:
    {
//        assert( child_count == 1 );
        _eval.push_back( { IR_O_VALUE, op( n->sloc, IR_LENGTH, 1 ) } );
        return;
    }

    case AST_EXPR_NEG:
    {
//        assert( child_count == 1 );
        _eval.push_back( { IR_O_VALUE, op( n->sloc, IR_NEG, 1 ) } );
        return;
    }

    case AST_EXPR_POS:
    {
//        assert( child_count == 1 );
        _eval.push_back( { IR_O_VALUE, op( n->sloc, IR_POS, 1 ) } );
        return;
    }

    case AST_EXPR_BITNOT:
    {
//        assert( child_count == 1 );
        _eval.push_back( { IR_O_VALUE, op( n->sloc, IR_BITNOT, 1 ) } );
        return;
    }

    case AST_EXPR_MUL:
    {
//        assert( child_count == 2 );
        _eval.push_back( { IR_O_VALUE, op( n->sloc, IR_MUL, 2 ) } );
        return;
    }

    case AST_EXPR_DIV:
    {
//        assert( child_count == 2 );
        _eval.push_back( { IR_O_VALUE, op( n->sloc, IR_DIV, 2 ) } );
        return;
    }

    case AST_EXPR_INTDIV:
    {
//        assert( child_count == 2 );
        _eval.push_back( { IR_O_VALUE, op( n->sloc, IR_INTDIV, 2 ) } );
        return;
    }

    case AST_EXPR_MOD:
    {
//        assert( child_count == 2 );
        _eval.push_back( { IR_O_VALUE, op( n->sloc, IR_MOD, 2 ) } );
        return;
    }

    case AST_EXPR_ADD:
    {
 //       assert( child_count == 2 );
        _eval.push_back( { IR_O_VALUE, op( n->sloc, IR_ADD, 2 ) } );
        return;
    }

    case AST_EXPR_SUB:
    {
//        assert( child_count == 2 );
        _eval.push_back( { IR_O_VALUE, op( n->sloc, IR_SUB, 2 ) } );
        return;
    }

    case IR_CONCAT:
    {
//        assert( child_count == 2 );
        _eval.push_back( { IR_O_VALUE, op( n->sloc, IR_SUB, 2 ) } );
        return;
    }

    case IR_LSHIFT:
    {
//        assert( child_count == 2 );
        _eval.push_back( { IR_O_VALUE, op( n->sloc, IR_LSHIFT, 2 ) } );
        return;
    }

    case IR_RSHIFT:
    {
//        assert( child_count == 2 );
        _eval.push_back( { IR_O_VALUE, op( n->sloc, IR_RSHIFT, 2 ) } );
        return;
    }

    case IR_ASHIFT:
    {
//        assert( child_count == 2 );
        _eval.push_back( { IR_O_VALUE, op( n->sloc, IR_ASHIFT, 2 ) } );
        return;
    }

    case IR_BITAND:
    {
//        assert( child_count == 2 );
        _eval.push_back( { IR_O_VALUE, op( n->sloc, IR_BITAND, 2 ) } );
        return;
    }

    case IR_BITXOR:
    {
//        assert( child_count == 2 );
        _eval.push_back( { IR_O_VALUE, op( n->sloc, IR_BITXOR, 2 ) } );
        return;
    }

    case IR_BITOR:
    {
//        assert( child_count == 2 );
        _eval.push_back( { IR_O_VALUE, op( n->sloc, IR_SUB, 2 ) } );
        return;
    }

    case AST_EXPR_KEY:
    {
//        assert( child_count == 1 );
        _eval.push_back( { IR_O_AST_KEY, ast_index } );
        _eval.push_back( { IR_O_VALUE, op( n->sloc, IR_GET_KEY, 2 ) } );
        return;
    }

    case AST_EXPR_INDEX:
    {
//        assert( child_count == 2 );
        _eval.push_back( { IR_O_VALUE, op( n->sloc, IR_GET_INDEX, 2 ) } );
        return;
    }

    case AST_EXPR_CALL:
    {
        // TODO: unpack at end?
//        assert( child_count >= 1 );
        _eval.push_back( { IR_O_VALUE, op( n->sloc, IR_CALL, child_count ) } );
        return;
    }

    default: break;
    }
}

unsigned build_icode::op( srcloc sloc, icode_opcode opcode, unsigned operand_count, bool head )
{
    icode_op op;
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
    operand_count = std::min< unsigned >( _eval.size(), operand_count );

    assert( _eval.size() >= operand_count );
    for ( unsigned i = _eval.size() - operand_count; i < _eval.size(); ++i )
    {
        _block->operands.push_back( _eval.at( i ) );
    }
    _eval.resize( _eval.size() - operand_count );

    return op_index;
}

void build_icode::def( unsigned local_index, icode_block* block, unsigned op_index )
{
    _def_map.insert_or_assign( std::make_pair( local_index, block ), op_index );
}

}

