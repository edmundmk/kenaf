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

void fold_ir::fold_constants( ir_block* block )
{
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


