//
//  ir_fold.cpp
//
//  Created by Edmund Kapusniak on 12/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "ir_fold.h"
#include "ast.h"
#include "../common/imath.h"

namespace kf
{

ir_fold::ir_fold( report* report, source* source )
    :   _report( report )
    ,   _source( source )
    ,   _f( nullptr )
{
}

ir_fold::~ir_fold()
{
}

void ir_fold::fold( ir_function* function )
{
    _f = function;
    fold_phi();
    fold_constants();
    remove_unreachable_blocks();
}

void ir_fold::fold_phi()
{
    /*
        Fold the function's phigraph.  Each phi should reference either a
        non-phi op, or a phi op that merges multiple distinct definitions.

        First we replace links which loop back to the header with a self-def.
        Then we simplify by skipping phi definitions with a single operand.
    */
    fold_phi_loop();
    fold_phi_step();
}

void ir_fold::fold_phi_loop()
{
    /*
       Replace links which always loop back to the header.
    */
    for ( unsigned block_index = 0; block_index < _f->blocks.size(); ++block_index )
    {
        ir_block* block = &_f->blocks[ block_index ];
        if ( block->kind != IR_BLOCK_LOOP )
        {
            continue;
        }

        for ( unsigned phi_index = block->phi_head; phi_index != IR_INVALID_INDEX; phi_index = _f->ops[ phi_index ].phi_next )
        {
            ir_op* phi = &_f->ops[ phi_index ];
            if ( phi->opcode == IR_REF )
            {
                continue;
            }

            for ( unsigned j = 0; j < phi->ocount; ++j )
            {
                ir_operand* operand = &_f->operands[ phi->oindex + j ];
                assert( operand->kind == IR_O_OP );

                if ( phi_loop_search( { IR_O_OP, phi_index }, *operand ) )
                {
                    *operand = { IR_O_OP, phi_index };
                }
            }
        }
    }
}

bool ir_fold::phi_loop_search( ir_operand loop_phi, ir_operand operand )
{
    /*
        Return true if all reachable ops from operand terminate at loop_phi.
    */
    assert( operand.kind == IR_O_OP );
    ir_op* op = &_f->ops[ operand.index ];
    if ( op->opcode != IR_PHI && op->opcode != IR_REF )
    {
        return false;
    }

    if ( op->mark )
    {
        return true;
    }
    op->mark = true;

    for ( unsigned j = 0; j < op->ocount; ++j )
    {
        ir_operand operand = _f->operands[ op->oindex + j ];
        assert( operand.kind == IR_O_OP );

        if ( operand.index == loop_phi.index )
        {
            continue;
        }

        if ( ! phi_loop_search( loop_phi, operand ) )
        {
            op->mark = false;
            return false;
        }
    }

    op->mark = false;
    return true;
}

void ir_fold::fold_phi_step()
{
    /*
        Simplify by folding all phi operands that reference a phi that
        references a single other op.  This is the same simplification which
        was performed when closing the phi in the build step.
    */
    for ( unsigned block_index = 0; block_index < _f->blocks.size(); ++block_index )
    {
        ir_block* block = &_f->blocks[ block_index ];

        for ( unsigned phi_index = block->phi_head; phi_index != IR_INVALID_INDEX; phi_index = _f->ops[ phi_index ].phi_next )
        {
            ir_op* phi = &_f->ops[ phi_index ];
            assert( phi->opcode == IR_PHI || phi->opcode == IR_REF );

            size_t ref_count = 0;
            ir_operand ref = { IR_O_NONE };
            for ( unsigned j = 0; j < phi->ocount; ++j )
            {
                ir_operand def = _f->operands[ phi->oindex + j ];
                assert( def.kind == IR_O_OP );

                // Look through refs.
                ir_op* op = &_f->ops[ def.index ];
                if ( op->opcode == IR_REF )
                {
                    assert( op->ocount == 1 );
                    def = _f->operands[ op->oindex ];
                    assert( def.kind == IR_O_OP );
                }

                // Detect case of single non-self ref.
                if ( def.index != phi_index && def.index != ref.index )
                {
                    ref = def;
                    ref_count += 1;
                }
            }

            // Collapse phi to ref.
            if ( ref_count == 1 )
            {
                assert( phi->ocount >= 1 );
                phi->opcode = IR_REF;
                phi->ocount = 1;
                _f->operands[ phi->oindex ] = ref;
            }
        }
    }
}

void ir_fold::fold_constants()
{
    if ( _f->blocks.size() )
    {
        _stack.push_back( { IR_O_BLOCK, 0 } );
    }

    while ( ! _stack.empty() )
    {
        ir_operand block_operand = _stack.back();
        _stack.pop_back();

        assert( block_operand.kind == IR_O_BLOCK );
        ir_block* block = &_f->blocks[ block_operand.index ];

        // If we've already visited, continue.
        if ( block->reachable )
            continue;
        block->reachable = true;

        // Fold constants in block.
        fold_constants( block );

        // Find blocks reachable from this block.
        ir_op& jump = _f->ops[ block->upper - 1 ];
        if ( jump.opcode == IR_JUMP )
        {
            assert( jump.ocount == 1 );
            _stack.push_back( jump_block_operand( jump.oindex ) );
        }
        else if ( jump.opcode == IR_JUMP_TEST || jump.opcode == IR_JUMP_FOR_EACH || jump.opcode == IR_JUMP_FOR_STEP )
        {
            assert( jump.ocount == 3 );
            _stack.push_back( jump_block_operand( jump.oindex + 1 ) );
            _stack.push_back( jump_block_operand( jump.oindex + 2 ) );
        }
        else if ( jump.opcode == IR_JUMP_FOR_EGEN || jump.opcode == IR_JUMP_FOR_SGEN )
        {
            _stack.push_back( jump_block_operand( jump.oindex + jump.ocount - 1 ) );
        }
        else
        {
            assert( jump.opcode == IR_JUMP_THROW || jump.opcode == IR_JUMP_RETURN );
        }
    }
}

void ir_fold::fold_constants( ir_block* block )
{
    for ( unsigned op_index = block->lower; op_index < block->upper; ++op_index )
    {
        ir_op* op = &_f->ops[ op_index ];
        if ( op->opcode == IR_PHI || op->opcode == IR_REF )
        {
            continue;
        }

        switch ( op->opcode )
        {
        case IR_NEG:
        case IR_POS:
        case IR_BITNOT:
            fold_unarithmetic( op );
            break;

        case IR_MUL:
        case IR_DIV:
        case IR_INTDIV:
        case IR_MOD:
        case IR_ADD:
        case IR_SUB:
        case IR_LSHIFT:
        case IR_RSHIFT:
        case IR_ASHIFT:
        case IR_BITAND:
        case IR_BITXOR:
        case IR_BITOR:
            fold_biarithmetic( op );
            break;

        case IR_CONCAT:
            fold_concat( op );
            break;

        case IR_MOV:
            fold_mov( op );
            break;

        case IR_EQ:
        case IR_NE:
            fold_equal( op );
            break;

        case IR_LT:
        case IR_LE:
            fold_compare( op );
            break;

        case IR_NOT:
            fold_not( op );
            break;

        case IR_JUMP_TEST:
            fold_test( op );
            break;

        default: break;
        }
    }
}

ir_operand ir_fold::jump_block_operand( unsigned operand_index )
{
    ir_operand o = _f->operands[ operand_index ];
    assert( o.kind == IR_O_JUMP );
    ir_op& block = _f->ops[ o.index ];
    assert( block.opcode == IR_BLOCK );
    assert( block.ocount == 1 );
    o = _f->operands[ block.oindex ];
    assert( o.kind == IR_O_BLOCK );
    return o;
}

ir_operand ir_fold::fold_operand( unsigned operand_index )
{
    return ir_fold_operand( _f, _f->operands[ operand_index ] );
}

bool ir_fold::is_constant( ir_operand operand )
{
    return operand.kind == IR_O_NULL
        || operand.kind == IR_O_TRUE || operand.kind == IR_O_FALSE
        || operand.kind == IR_O_NUMBER || operand.kind == IR_O_STRING;
}

double ir_fold::to_number( ir_operand operand )
{
    assert( operand.kind == IR_O_NUMBER );
    return _f->constants[ operand.index ].n;
}

std::string_view ir_fold::to_string( ir_operand operand )
{
    assert( operand.kind == IR_O_STRING );
    const ir_constant& s = _f->constants[ operand.index ];
    return std::string_view( s.text, s.size );
}

bool ir_fold::test_constant( ir_operand operand )
{
    if ( operand.kind == IR_O_NULL || operand.kind == IR_O_FALSE )
        return false;
    else if ( operand.kind == IR_O_NUMBER )
        return to_number( operand ) != 0.0;
    else
        return true;
}

std::pair< ir_operand, size_t > ir_fold::count_nots( ir_operand operand )
{
    const ir_op* not_op;
    size_t not_count = 0;
    while ( operand.kind == IR_O_OP && ( not_op = &_f->ops[ operand.index ] )->opcode == IR_NOT )
    {
        operand = _f->operands[ not_op->oindex ];
        not_count += 1;
    }
    return std::make_pair( operand, not_count );
}

bool ir_fold::fold_unarithmetic( ir_op* op )
{
    assert( op->ocount == 1 );
    ir_operand u = fold_operand( op->oindex );

    if ( ! is_constant( u ) )
    {
        return false;
    }

    if ( u.kind == IR_O_NUMBER )
    {
        // Perform calculation.
        double result = 0.0;
        double a = to_number( u );

        switch ( op->opcode )
        {
        case IR_NEG:        result = -a;            break;
        case IR_POS:        result = +a;            break;
        case IR_BITNOT:     result = ibitnot( a );  break;
        default: break;
        }

        // Update operand.
        ir_operand* operand = &_f->operands[ op->oindex ];
        operand->kind = IR_O_NUMBER;
        operand->index = _f->constants.append( ir_constant( result ) );

        // Change op to constant.
        op->opcode = IR_CONST;
        return true;
    }
    else
    {
        _report->warning( op->sloc, "arithmetic on constant will throw at runtime" );
        return false;
    }
}

bool ir_fold::fold_biarithmetic( ir_op* op )
{
    assert( op->ocount == 2 );
    ir_operand u = fold_operand( op->oindex + 0 );
    ir_operand v = fold_operand( op->oindex + 1 );

    if ( ! is_constant( u ) || ! is_constant( v ) )
    {
        return false;
    }

    if ( u.kind == IR_O_NUMBER && v.kind == IR_O_NUMBER )
    {
        // Perform calculation.
        double result = 0.0;
        double a = to_number( u );
        double b = to_number( v );

        switch ( op->opcode )
        {
        case IR_MUL:        result = a * b;                 break;
        case IR_DIV:        result = a / b;                 break;
        case IR_INTDIV:     result = ifloordiv( a, b );     break;
        case IR_MOD:        result = ifloormod( a, b );     break;
        case IR_ADD:        result = a + b;                 break;
        case IR_SUB:        result = a - b;                 break;
        case IR_LSHIFT:     result = ilshift( a, b );       break;
        case IR_RSHIFT:     result = irshift( a, b );       break;
        case IR_ASHIFT:     result = iashift( a, b );       break;
        case IR_BITAND:     result = ibitand( a, b );       break;
        case IR_BITXOR:     result = ibitxor( a, b );       break;
        case IR_BITOR:      result = ibitor( a, b );        break;
        default: break;
        }

        // Update operand.
        ir_operand* operand = &_f->operands[ op->oindex ];
        operand->kind = IR_O_NUMBER;
        operand->index = _f->constants.append( ir_constant( result ) );

        // Change op to constant.
        op->opcode = IR_CONST;
        op->ocount = 1;
        return true;
    }
    else
    {
        _report->warning( op->sloc, "arithmetic on constant will throw at runtime" );
        return false;
    }
}

bool ir_fold::fold_concat( ir_op* op )
{
    assert( op->ocount == 2 );
    ir_operand u = fold_operand( op->oindex + 0 );
    ir_operand v = fold_operand( op->oindex + 1 );

    if ( ! is_constant( u ) || ! is_constant( v ) )
    {
        return false;
    }

    if ( u.kind == IR_O_STRING && v.kind == IR_O_STRING )
    {
        std::string_view ustring = to_string( u );
        std::string_view vstring = to_string( v );

        // Concatenate string.
        const source_string* result = _source->new_string( ustring.data(), ustring.size(), vstring.data(), vstring.size() );
        ir_operand* operand = &_f->operands[ op->oindex ];
        operand->kind = IR_O_STRING;
        operand->index = _f->constants.append( ir_constant( result->text, result->size ) );

        // Change op to constant.
        op->opcode = IR_CONST;
        op->ocount = 1;
        return true;
    }
    else
    {
        _report->warning( op->sloc, "arithmetic on constant will throw at runtime" );
        return false;
    }
}

bool ir_fold::fold_mov( ir_op* op )
{
    assert( op->ocount == 1 );
    ir_operand u = fold_operand( op->oindex );

    if ( ! is_constant( u ) )
    {
        return false;
    }

    ir_operand* operand = &_f->operands[ op->oindex ];
    *operand = u;

    op->opcode = IR_CONST;
    op->ocount = 1;
    return true;
}

bool ir_fold::fold_equal( ir_op* op )
{
    assert( op->ocount == 2 );
    ir_operand u = fold_operand( op->oindex + 0 );
    ir_operand v = fold_operand( op->oindex + 1 );

    if ( ! is_constant( u ) || ! is_constant( v ) )
    {
        return false;
    }

    bool result;
    if ( u.kind == IR_O_NUMBER && v.kind == IR_O_NUMBER )
    {
        double a = to_number( u );
        double b = to_number( v );
        result = op->opcode == IR_EQ ? a == b : a != b;
    }
    else if ( u.kind == IR_O_STRING && v.kind == IR_O_STRING )
    {
        std::string_view a = to_string( u );
        std::string_view b = to_string( v );
        result = op->opcode == IR_EQ ? a == b : a != b;
    }
    else
    {
        result = op->opcode == IR_EQ ? u.kind == v.kind : u.kind != v.kind;
    }

    // Update operand.
    ir_operand* operand = &_f->operands[ op->oindex ];
    operand->kind = result ? IR_O_TRUE : IR_O_FALSE;

    // Change op to constant.
    op->opcode = IR_CONST;
    op->ocount = 1;
    return true;
}

bool ir_fold::fold_compare( ir_op* op )
{
    assert( op->ocount == 2 );
    ir_operand u = fold_operand( op->oindex + 0 );
    ir_operand v = fold_operand( op->oindex + 1 );

    if ( ! is_constant( u ) || ! is_constant( v ) )
    {
        return false;
    }

    bool result;
    if ( u.kind == IR_O_NUMBER && v.kind == IR_O_NUMBER )
    {
        double a = to_number( u );
        double b = to_number( v );
        result = op->opcode == IR_LT ? a < b : a <= b;
    }
    else if ( u.kind == IR_O_STRING && v.kind == IR_O_STRING )
    {
        std::string_view a = to_string( u );
        std::string_view b = to_string( v );
        result = op->opcode == IR_LT ? a < b : a <= b;
    }
    else
    {
        _report->warning( op->sloc, "arithmetic on constant will throw at runtime" );
        return false;
    }

    // Update operand.
    ir_operand* operand = &_f->operands[ op->oindex ];
    operand->kind = result ? IR_O_TRUE : IR_O_FALSE;

    // Change op to constant.
    op->opcode = IR_CONST;
    op->ocount = 1;
    return true;
}

bool ir_fold::fold_not( ir_op* op )
{
    assert( op->opcode == IR_NOT );
    assert( op->ocount == 1 );
    ir_operand u = fold_operand( op->oindex );

    if ( ! is_constant( u ) )
    {
        return false;
    }

    bool test = test_constant( u );

    // Update operand.
    ir_operand* operand = &_f->operands[ op->oindex ];
    operand->kind = test ? IR_O_FALSE : IR_O_TRUE;

    // Change op to constant.
    op->opcode = IR_CONST;
    op->ocount = 1;
    return true;
}

bool ir_fold::fold_test( ir_op* op )
{
    assert( op->opcode == IR_JUMP_TEST );
    assert( op->ocount == 3 );
    ir_operand u = fold_operand( op->oindex );

    if ( is_constant( u ) )
    {
        // Change test to unconditional jump.
        bool test = test_constant( u );
        ir_operand* operand = &_f->operands[ op->oindex ];
        ir_operand* jump = &_f->operands[ op->oindex + ( test ? 1 : 2 ) ];
        *operand = *jump;
        op->opcode = IR_JUMP;
        op->ocount = 1;
        return true;
    }

    // Count nots in test expression.
    std::pair< ir_operand, size_t > not_count = count_nots( u );
    if ( not_count.second )
    {
        // Skip past nots.
        ir_operand* operand = &_f->operands[ op->oindex ];
        *operand = not_count.first;

        // Swap true/false if not_count is odd.
        if ( not_count.second % 2 )
        {
            ir_operand* jt = &_f->operands[ op->oindex + 1 ];
            ir_operand* jf = &_f->operands[ op->oindex + 2 ];
            std::swap( *jt, *jf );
        }
    }

    return false;
}

void ir_fold::remove_unreachable_blocks()
{
    for ( unsigned block_index = 0; block_index < _f->blocks.size(); ++block_index )
    {
        ir_block* block = &_f->blocks[ block_index ];
        if ( block->reachable )
        {
            continue;
        }

        // Remove block.
        block->kind = IR_BLOCK_NONE;
        block->preceding_lower = block->preceding_upper = IR_INVALID_INDEX;

        // Remove phi ops.
        for ( unsigned phi_index = block->phi_head; phi_index != IR_INVALID_INDEX; phi_index = _f->ops[ phi_index ].phi_next )
        {
            ir_op* phi = &_f->ops[ phi_index ];
            phi->opcode = IR_NOP;
            phi->ocount = 0;
            phi->oindex = IR_INVALID_INDEX;
            phi->set_local( IR_INVALID_LOCAL );
        }
        block->phi_head = block->phi_tail = IR_INVALID_INDEX;

        // Remove instructions.
        for ( unsigned op_index = block->lower; op_index < block->upper; ++op_index )
        {
            ir_op* op = &_f->ops[ op_index ];
            if ( op->opcode == IR_PHI || op->opcode == IR_REF )
                continue;
            op->opcode = IR_NOP;
            op->ocount = 0;
            op->oindex = IR_INVALID_INDEX;
            op->set_local( IR_INVALID_LOCAL );
        }
    }
}

ir_operand ir_fold_operand( ir_function* f, ir_operand operand )
{
    if ( operand.kind != IR_O_OP )
    {
        return operand;
    }

    const ir_op* op = &f->ops[ operand.index ];
    while ( true )
    {
        // Look past MOV/REF.
        if ( op->opcode == IR_MOV || op->opcode == IR_REF )
        {
            assert( op->ocount == 1 );
            ir_operand oval = f->operands[ op->oindex ];
            assert( oval.kind == IR_O_OP );
            op = &f->ops[ oval.index ];
        }
        else
        {
            break;
        }
    }

    if ( op->opcode == IR_CONST )
    {
        assert( op->ocount == 1 );
        return f->operands[ op->oindex ];
    }

    return operand;
}

}

