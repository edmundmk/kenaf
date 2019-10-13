//
//  fold_ir.cpp
//
//  Created by Edmund Kapusniak on 12/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "fold_ir.h"
#include "ast.h"
#include "../common/imath.h"

namespace kf
{

fold_ir::fold_ir( source* source )
    :   _source( source )
    ,   _f( nullptr )
{
}

fold_ir::~fold_ir()
{
}

void fold_ir::fold( ir_function* function )
{
    _f = function;
    fold_phi();
    fold_constants();
    remove_unreachable_blocks();
}

void fold_ir::fold_phi()
{
    /*
        Fold the function's phigraph.  Each phi should reference either a
        non-phi op, or a phi op that merges multiple distinct definitions.

        First we break links which always loop back to a loop header.  Then
        we simplify by skipping phi definitions with a single operand.
    */
    fold_phi_loop();
    fold_phi_step();

    _f->debug_print_phi_graph();
}

void fold_ir::fold_phi_loop()
{
    /*
        Break links from loop header phi ops which loop back to the header.
    */
    for ( unsigned block_index = 0; block_index < _f->blocks.size(); ++block_index )
    {
        ir_block* block = &_f->blocks[ block_index ];
        if ( block->kind != IR_BLOCK_LOOP )
        {
            continue;
        }

        for ( unsigned phi_index = block->phi_head; phi_index != IR_INVALID_INDEX; phi_index = _f->ops.at( phi_index ).phi_next )
        {
            ir_op* phi = &_f->ops.at( phi_index );
            assert( phi->opcode == IR_PHI );

            for ( unsigned j = 0; j < phi->ocount; ++j )
            {
                ir_operand operand = _f->operands.at( phi->oindex + j );
                assert( operand.kind == IR_O_OP );

                if ( phi_loop_search( { IR_O_OP, phi_index }, operand ) )
                {
                    continue;
                }

                _stack.push_back( operand );
            }

            assert( _stack.size() <= phi->ocount );
            std::copy( _stack.begin(), _stack.end(), _f->operands.begin() + phi->oindex );
            phi->ocount = _stack.size();
            _stack.clear();
        }
    }
}

bool fold_ir::phi_loop_search( ir_operand loop_phi, ir_operand operand )
{
    /*
        Return true if all reachable ops from operand terminate at loop_phi.
    */
    assert( operand.kind == IR_O_OP );
    const ir_op* op = &_f->ops.at( operand.index );
    if ( op->opcode != IR_PHI )
    {
        return false;
    }

    for ( unsigned j = 0; j < op->ocount; ++j )
    {
        ir_operand operand = _f->operands.at( op->oindex + j );
        assert( operand.kind == IR_O_OP );

        if ( operand.index == loop_phi.index )
        {
            continue;
        }

        if ( ! phi_loop_search( loop_phi, operand ) )
        {
            return false;
        }
    }

    return true;
}

void fold_ir::fold_phi_step()
{
    /*
        Simplify by folding all phi operands that reference a phi that
        references a single other op.  This is the same simplification which
        was performed when closing the phi in the build step.
    */
    for ( unsigned block_index = 0; block_index < _f->blocks.size(); ++block_index )
    {
        ir_block* block = &_f->blocks[ block_index ];

        for ( unsigned phi_index = block->phi_head; phi_index != IR_INVALID_INDEX; phi_index = _f->ops.at( phi_index ).phi_next )
        {
            ir_op* phi = &_f->ops.at( phi_index );
            assert( phi->opcode == IR_PHI );

            for ( unsigned j = 0; j < phi->ocount; ++j )
            {
                ir_operand def = _f->operands.at( phi->oindex + j );
                assert( def.kind == IR_O_OP );

                // Look through phi ops with a single operand.
                ir_op* op = &_f->ops.at( def.index );
                if ( op->opcode == IR_PHI && op->ocount == 1 )
                {
                    def = _f->operands.at( op->oindex );
                    assert( def.kind == IR_O_OP );
                }

                // Ignore loop back to this phi.
                if ( def.index == phi_index )
                {
                    continue;
                }

                // Merge defs that are identical.
                bool exists = false;
                for ( size_t i = 0; i < _stack.size(); ++i )
                {
                    if ( def.index == _stack[ i ].index )
                    {
                        exists = true;
                        break;
                    }
                }

                if ( exists )
                {
                    continue;
                }

                _stack.push_back( def );
            }

            assert( _stack.size() <= phi->ocount );
            std::copy( _stack.begin(), _stack.end(), _f->operands.begin() + phi->oindex );
            phi->ocount = _stack.size();
            _stack.clear();
        }
    }
}

void fold_ir::fold_constants()
{
    _stack.push_back( { IR_O_BLOCK, 0 } );
    while ( ! _stack.empty() )
    {
        ir_operand block_operand = _stack.back();
        _stack.pop_back();

        assert( block_operand.kind == IR_O_BLOCK );
        ir_block* block = &_f->blocks.at( block_operand.index );

        // If we've already visited, continue.
        if ( block->reachable )
            continue;
        block->reachable = true;

        // Fold constants in block.
        fold_constants( block );

        // Find blocks reachable from this block.
        ir_op& jump = _f->ops.at( block->upper - 1 );
        if ( jump.opcode == IR_JUMP )
        {
            assert( jump.ocount == 1 );
            _stack.push_back( jump_block_operand( jump.oindex ) );
        }
        else if ( jump.opcode == IR_JUMP_TEST )
        {
            assert( jump.ocount == 3 );
            _stack.push_back( jump_block_operand( jump.oindex + 1 ) );
            _stack.push_back( jump_block_operand( jump.oindex + 2 ) );
        }
        else if ( jump.opcode == IR_JUMP_FOR_EACH || jump.opcode == IR_JUMP_FOR_STEP )
        {
            assert( jump.ocount == 2 );
            _stack.push_back( jump_block_operand( jump.oindex + 0 ) );
            _stack.push_back( jump_block_operand( jump.oindex + 1 ) );
        }
        else
        {
            assert( jump.opcode == IR_JUMP_THROW || jump.opcode == IR_JUMP_RETURN );
        }
    }
}

ir_operand fold_ir::jump_block_operand( unsigned operand_index )
{
    ir_operand o = _f->operands.at( operand_index );
    assert( o.kind == IR_O_JUMP );
    ir_op& block = _f->ops.at( o.index );
    assert( block.opcode == IR_BLOCK );
    assert( block.ocount == 1 );
    o = _f->operands.at( block.oindex );
    assert( o.kind == IR_O_BLOCK );
    return o;
}

ir_operand fold_ir::fold_operand( unsigned operand_index )
{
    ir_operand operand = _f->operands.at( operand_index );

    if ( operand.kind == IR_O_OP )
    {
        const ir_op* op = &_f->ops.at( operand.index );
        if ( is_upval( op ) )
        {
            return operand;
        }

        while ( op->opcode == IR_VAL || ( op->opcode == IR_PHI && op->ocount == 1 ) )
        {
            assert( op->ocount == 1 );
            ir_operand oval = _f->operands.at( op->oindex );
            assert( oval.kind == IR_O_OP );
            op = &_f->ops.at( oval.index );
            if ( is_upval( op ) )
            {
                return operand;
            }
        }

        if ( op->opcode == IR_CONST )
        {
            assert( op->ocount == 1 );
            operand = _f->operands.at( op->oindex );
            assert( is_constant( operand ) );
            return operand;
        }
    }

    return operand;
}

bool fold_ir::is_constant( ir_operand operand )
{
    return operand.kind == IR_O_NULL
        || operand.kind == IR_O_TRUE || operand.kind == IR_O_FALSE
        || operand.kind == IR_O_NUMBER || operand.kind == IR_O_STRING;
}

bool fold_ir::is_upval( const ir_op* op )
{
    return op->local != IR_INVALID_LOCAL && _f->ast->locals.at( op->local ).upstack_index != AST_INVALID_INDEX;
}

double fold_ir::to_number( ir_operand operand )
{
    assert( operand.kind == IR_O_NUMBER );
    return _f->numbers.at( operand.index ).n;
}

template < typename F > bool fold_ir::fold_unarithmetic( ir_op* op, F fold )
{
    assert( op->ocount == 1 );
    ir_operand u = fold_operand( op->oindex );

    if ( u.kind == IR_O_NUMBER )
    {
        // Perform calculation.
        double result = fold( to_number( u ) );

        // Update operand.
        ir_operand* operand = &_f->operands.at( op->oindex );
        operand->kind = IR_O_NUMBER;
        operand->index = _f->numbers.size();
        _f->numbers.push_back( { result } );

        // Change op to constant.
        op->opcode = IR_CONST;
        return true;
    }

    if ( is_constant( u ) )
    {
        _source->warning( op->sloc, "arithmetic on constant will throw at runtime" );
    }

    return false;
}

template < typename F > bool fold_ir::fold_biarithmetic( ir_op* op, F fold )
{
    assert( op->ocount == 2 );
    ir_operand u = fold_operand( op->oindex + 0 );
    ir_operand v = fold_operand( op->oindex + 1 );

    if ( u.kind == IR_O_NUMBER && v.kind == IR_O_NUMBER )
    {
        // Perform calculation.
        double result = fold( to_number( u ), to_number( v ) );

        // Update operand.
        ir_operand* operand = &_f->operands.at( op->oindex );
        operand->kind = IR_O_NUMBER;
        operand->index = _f->numbers.size();
        _f->numbers.push_back( { result } );

        // Change op to constant.
        op->opcode = IR_CONST;
        op->ocount = 1;
        return true;
    }

    if ( is_constant( u ) && is_constant( v ) )
    {
        _source->warning( op->sloc, "arithmetic on constant will throw at runtime" );
    }

    return false;
}

void fold_ir::fold_constants( ir_block* block )
{
    for ( unsigned op_index = block->lower; op_index < block->upper; ++op_index )
    {
        ir_op* op = &_f->ops.at( op_index );
        if ( op->opcode == IR_PHI )
        {
            continue;
        }

        switch ( op->opcode )
        {
        case IR_NEG:    fold_unarithmetic( op, []( double u ) { return -u; } );                             break;
        case IR_POS:    fold_unarithmetic( op, []( double u ) { return +u; } );                             break;
        case IR_BITNOT: fold_unarithmetic( op, []( double u ) { return ibitnot( u ); } );                   break;

        case IR_MUL:    fold_biarithmetic( op, []( double u, double v ) { return u * v; } );                break;
        case IR_DIV:    fold_biarithmetic( op, []( double u, double v ) { return u / v; } );                break;
        case IR_INTDIV: fold_biarithmetic( op, []( double u, double v ) { return ifloordiv( u, v ); } );    break;
        case IR_MOD:    fold_biarithmetic( op, []( double u, double v ) { return ifloormod( u, v ); } );    break;
        case IR_ADD:    fold_biarithmetic( op, []( double u, double v ) { return u + v; } );                break;
        case IR_SUB:    fold_biarithmetic( op, []( double u, double v ) { return u - v; } );                break;
        case IR_LSHIFT: fold_biarithmetic( op, []( double u, double v ) { return ilshift( u, v ); } );      break;
        case IR_RSHIFT: fold_biarithmetic( op, []( double u, double v ) { return irshift( u, v ); } );      break;
        case IR_ASHIFT: fold_biarithmetic( op, []( double u, double v ) { return iashift( u, v ); } );      break;
        case IR_BITAND: fold_biarithmetic( op, []( double u, double v ) { return ibitand( u, v ); } );      break;
        case IR_BITXOR: fold_biarithmetic( op, []( double u, double v ) { return ibitxor( u, v ); } );      break;
        case IR_BITOR:  fold_biarithmetic( op, []( double u, double v ) { return ibitor( u, v ); } );       break;

        default: break;
        }
    }
}

void fold_ir::remove_unreachable_blocks()
{
    for ( unsigned block_index = 0; block_index < _f->blocks.size(); ++block_index )
    {
        ir_block* block = &_f->blocks[ block_index ];
        if ( block->reachable )
            continue;

        // Remove block.
        block->kind = IR_BLOCK_NONE;
        block->preceding_lower = block->preceding_upper = IR_INVALID_INDEX;

        // Remove phi ops.
        for ( unsigned phi_index = block->phi_head; phi_index != IR_INVALID_INDEX; phi_index = _f->ops.at( phi_index ).phi_next )
        {
            ir_op* phi = &_f->ops.at( phi_index );
            phi->opcode = IR_NOP;
            phi->ocount = 0;
            phi->oindex = IR_INVALID_INDEX;
        }
        block->phi_head = block->phi_tail = IR_INVALID_INDEX;

        // Remove instructions.
        for ( unsigned op_index = block->lower; op_index < block->upper; ++op_index )
        {
            ir_op* op = &_f->ops.at( op_index );
            if ( op->opcode == IR_PHI )
                continue;
            op->opcode = IR_NOP;
            op->ocount = 0;
            op->oindex = IR_INVALID_INDEX;
        }
    }
}

}

