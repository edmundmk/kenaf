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
    fold_constants();
    remove_unreachable_blocks();
}

void fold_ir::fold_constants()
{
    _block_stack.push_back( 0 );
    while ( ! _block_stack.empty() )
    {
        ir_block_index block_index = _block_stack.back();
        _block_stack.pop_back();

        ir_block* block = &_f->blocks.at( block_index );

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
            _block_stack.push_back( jump_block_index( jump.oindex ) );
        }
        else if ( jump.opcode == IR_JUMP_TEST )
        {
            assert( jump.ocount == 3 );
            _block_stack.push_back( jump_block_index( jump.oindex + 1 ) );
            _block_stack.push_back( jump_block_index( jump.oindex + 2 ) );
        }
        else if ( jump.opcode == IR_JUMP_FOR_EACH || jump.opcode == IR_JUMP_FOR_STEP )
        {
            assert( jump.ocount == 2 );
            _block_stack.push_back( jump_block_index( jump.oindex + 0 ) );
            _block_stack.push_back( jump_block_index( jump.oindex + 1 ) );
        }
        else
        {
            assert( jump.opcode == IR_JUMP_THROW || jump.opcode == IR_JUMP_RETURN );
        }
    }
}

ir_block_index fold_ir::jump_block_index( unsigned operand_index )
{
    ir_operand o = _f->operands.at( operand_index );
    assert( o.kind == IR_O_JUMP );
    ir_op& block = _f->ops.at( o.index );
    assert( block.opcode == IR_BLOCK );
    assert( block.ocount == 1 );
    o = _f->operands.at( block.oindex );
    assert( o.kind == IR_O_BLOCK );
    return o.index;
}

ir_operand fold_ir::fold_operand( unsigned operand_index )
{
    ir_operand operand = _f->operands.at( operand_index );

    if ( operand.kind == IR_O_OP )
    {
        ir_op* op = &_f->ops.at( operand.index );

        while ( op->opcode == IR_VAL )
        {
            assert( op->ocount == 1 );
            ir_operand oval = _f->operands.at( op->oindex );
            assert( oval.kind == IR_O_OP );
            op = &_f->ops.at( oval.index );
        }

        if ( op->opcode == IR_PHI )
        {
//            operand = fold_phi( operand );
        }

        if ( op->opcode == IR_CONST )
        {
            assert( op->ocount == 1 );
            operand = _f->operands.at( op->oindex );
        }
    }

    return operand;
}

ir_operand fold_ir::fold_phi( ir_operand phi_operand )
{
    ir_op* phi = &_f->ops.at( phi_operand.index );
    assert( phi->opcode == IR_PHI );

    if ( phi->ocount == 0 )
    {
        return phi_operand;
    }

    ir_operand operand = fold_operand( phi->oindex );
    if ( ! is_constant( operand ) )
    {
        return phi_operand;
    }

    for ( unsigned i = 1; i < phi->ocount; ++i )
    {
//        ir_operand
    }

    return phi_operand;
}

bool fold_ir::is_constant( ir_operand operand )
{
    return operand.kind == IR_O_NULL
        || operand.kind == IR_O_TRUE || operand.kind == IR_O_FALSE
        || operand.kind == IR_O_NUMBER || operand.kind == IR_O_STRING;
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


