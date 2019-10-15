//
//  foldk_ir.cpp
//
//  Created by Edmund Kapusniak on 15/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "foldk_ir.h"
#include <string.h>
#include "fold_ir.h"
#include "ast.h"

namespace kf
{

foldk_ir::foldk_ir( source* source )
    :   _source( source )
{
}

foldk_ir::~foldk_ir()
{
}

bool foldk_ir::foldk( ir_function* function )
{
    _f = function;

    // Build lists of constants/selectors.
    try
    {
        inline_operands();
        alloc_constants();
    }
    catch ( std::exception& e )
    {
        _source->error(  _f->ast->sloc, "internal: %s", e.what() );
        _constants.clear();
        _selectors.clear();
        _number_map.clear();
        _string_map.clear();
        _selector_map.clear();
        return false;
    }

    // Update lists in function IR.
    _constants.shrink_to_fit();
    _selectors.shrink_to_fit();
    std::swap( _f->constants, _constants );
    std::swap( _f->selectors, _selectors );
    _constants.clear();
    _selectors.clear();
    _number_map.clear();
    _string_map.clear();
    _selector_map.clear();
    return true;
}

void foldk_ir::inline_operands()
{
    /*
        The following constant operands can be inlined:

            ADD v, c            ->  ADDK/ADDI v, c
            ADD c, v            ->  ADDK/ADDI v, c
            SUB v, c            ->  ADDK/ADDI v, -c
            SUB c, v            ->  SUBK/SUBI v, c
            MUL v, c            ->  MULK/MULI v, c
            MUL c, v            ->  MULK/MULI v, c
            CONCAT v, c         ->  CONCATK v, c
            CONCAT c, v         ->  RCONCATK v, c
            EQ v, c; JUMP       ->  JEQK v, c
            NE v, c; JUMP       ->  JNEK v, c
            LT v, c; JUMP       ->  JLTK v, c
            LT c, v; JUMP       ->  JGTK v, c
            LE v, c; JUMP       ->  JLEK v, c
            LE c, v; JUMP       ->  JGEK v, c
            GET_INDEX v, c      ->  GET_INDEXK/GET_INDEXI v, c
            SET_INDEX v, c, u   ->  SET_INDEXK/GET_INDEXK v, c, u

        Except of course that we can only inline the first 255 constants.
    */

    for ( unsigned op_index = 0; op_index < _f->ops.size(); ++op_index )
    {
        ir_op* op = &_f->ops[ op_index ];

        switch ( op->opcode )
        {
        case IR_ADD:
        case IR_MUL:
        {
            ir_operand* u = &_f->operands.at( op->oindex + 0 );
            ir_operand* v = &_f->operands.at( op->oindex + 1 );
            ir_operand fold_u = ir_fold_operand( _f, *u );
            ir_operand fold_v = ir_fold_operand( _f, *v );

            if ( fold_v.kind == IR_O_NUMBER )
            {
                // Second operand is constant.
                *v = inline_number( fold_v );
            }
            else if ( fold_u.kind == IR_O_NUMBER )
            {
                // Operation is commutative, switch operands.
                *u = *v;
                *v = inline_number( fold_u );
            }

            break;
        }

        case IR_SUB:
        {
            ir_operand* u = &_f->operands.at( op->oindex + 0 );
            ir_operand* v = &_f->operands.at( op->oindex + 1 );
            ir_operand fold_u = ir_fold_operand( _f, *u );
            ir_operand fold_v = ir_fold_operand( _f, *v );

            if ( fold_v.kind == IR_O_NUMBER )
            {
                // Convert to ADD with negated operand.
                double constant = _f->constants.at( fold_v.index ).n;
                fold_v.index = _f->constants.size();
                _f->constants.push_back( ir_constant( -constant ) );
                op->opcode = IR_ADD;
                *v = inline_number( fold_v );
            }
            else if ( fold_u.kind == IR_O_NUMBER )
            {
                // First operand is constant.
                *u = inline_number( fold_u );
            }

            break;
        }

        case IR_CONCAT:
        {
            ir_operand* u = &_f->operands.at( op->oindex + 0 );
            ir_operand* v = &_f->operands.at( op->oindex + 1 );
            ir_operand fold_u = ir_fold_operand( _f, *u );
            ir_operand fold_v = ir_fold_operand( _f, *v );

            if ( fold_v.kind == IR_O_STRING )
            {
                *v = insert_string( fold_v );
            }
            else if ( fold_u.kind == IR_O_STRING )
            {
                *u = insert_string( fold_u );
            }

            break;
        }

        case IR_EQ:
        case IR_NE:
        case IR_LT:
        case IR_LE:
        {
            // Can only inline when directly followed by a jump that uses op.
            ir_op* jump = &_f->ops.at( op_index + 1 );
            if ( jump->opcode != IR_JUMP_TEST )
            {
                break;
            }
            ir_operand test = _f->operands.at( jump->oindex );
            if ( test.kind != IR_O_OP || test.index != op_index )
            {
                break;
            }

            // Can inline.
            ir_operand* u = &_f->operands.at( op->oindex + 0 );
            ir_operand* v = &_f->operands.at( op->oindex + 1 );
            ir_operand fold_u = ir_fold_operand( _f, *u );
            ir_operand fold_v = ir_fold_operand( _f, *v );

            if ( fold_v.kind == IR_O_NUMBER )
            {
                *v = insert_number( fold_v );
            }
            else if ( fold_v.kind == IR_O_STRING )
            {
                *v = insert_string( fold_v );
            }
            else if ( fold_u.kind == IR_O_NUMBER )
            {
                *u = insert_number( fold_u );
                if ( op->opcode == IR_EQ || op->opcode == IR_NE )
                {
                    std::swap( *u, *v );
                }
            }
            else if ( fold_u.kind == IR_O_STRING )
            {
                *u = insert_string( fold_u );
                if ( op->opcode == IR_EQ || op->opcode == IR_NE )
                {
                    std::swap( *u, *v );
                }
            }

            break;
        }

        case IR_GET_KEY:
        case IR_SET_KEY:
        {
            // Do key selectors in first pass.
            ir_operand* s = &_f->operands.at( op->oindex + 1 );
            *s = insert_selector( *s );
            if ( s->index > 0xFF )
            {
                throw std::out_of_range( "too many selectors" );
            }
            break;
        }

        case IR_GET_INDEX:
        case IR_SET_INDEX:
        {
            ir_operand* i = &_f->operands.at( op->oindex + 1 );
            ir_operand fold_i = ir_fold_operand( _f, *i );

            if ( fold_i.kind == IR_O_NUMBER )
            {
                *i = inline_number( fold_i );
            }
            else if ( fold_i.kind == IR_O_STRING )
            {
                *i = insert_string( fold_i );
            }

            break;
        }

        default : break;
        }

        // Maximum inline constant index is 0xFF.
        if ( _constants.size() > 0xFF )
        {
            return;
        }
    }
}

void foldk_ir::alloc_constants()
{
    /*
        Update all constants and selectors, merging identical constants.
    */

    for ( unsigned op_index = 0; op_index < _f->ops.size(); ++op_index )
    {
        ir_op* op = &_f->ops[ op_index ];

        switch ( op->opcode )
        {
        case IR_CONST:
        {
            ir_operand* u = &_f->operands.at( op->oindex );
            if ( u->kind == IR_O_NUMBER )
            {
                *u = insert_number( *u );
            }
            else if ( u->kind == IR_O_STRING )
            {
                *u = insert_string( *u );
            }
            break;
        }

        case IR_GET_GLOBAL:
        {
            ir_operand* s = &_f->operands.at( op->oindex );
            *s = insert_selector( *s );
            break;
        }

        default: break;
        }
    }
}


ir_operand foldk_ir::inline_number( ir_operand operand )
{
    assert( operand.kind == IR_O_NUMBER );
    double number = _f->constants.at( operand.index ).n;
    int8_t imm8 = (int8_t)number;
    if ( (double)imm8 == number )
    {
        return { IR_O_IMMEDIATE, (unsigned)imm8 };
    }
    else
    {
        return insert_number( operand );
    }
}

ir_operand foldk_ir::insert_number( ir_operand operand )
{
    assert( operand.kind == IR_O_NUMBER );
    double number = _f->constants.at( operand.index ).n;

    // Use bit pattern so exact double is preserved.
    uint64_t double_key = 0;
    memcpy( &double_key, &number, sizeof( double_key ) );

    // Check if it exists already.
    auto i = _number_map.find( double_key );
    if ( i != _number_map.end() )
    {
        return { IR_O_NUMBER, i->second };
    }

    // Maximum constant index is 0xFFFF.
    if ( _constants.size() > 0xFFFF )
    {
        throw std::out_of_range( "too many constants" );
    }

    // Add new number to map.
    unsigned index = _constants.size();
    _constants.push_back( ir_constant( number ) );
    _number_map.emplace( double_key, index );
    return { IR_O_NUMBER, index };
}

ir_operand foldk_ir::insert_string( ir_operand operand )
{
    assert( operand.kind == IR_O_STRING );
    ir_constant sc = _f->constants.at( operand.index );
    std::string_view sv( sc.text, sc.size );

    auto i = _string_map.find( sv );
    if ( i != _string_map.end() )
    {
        return { IR_O_STRING, i->second };
    }

    // Maximum constant index is 0xFFFF.
    if ( _constants.size() > 0xFFFF )
    {
        throw std::out_of_range( "too many constants" );
    }

    unsigned index = _constants.size();
    _constants.push_back( sc );
    _string_map.emplace( sv, index );
    return { IR_O_STRING, index };
}

ir_operand foldk_ir::insert_selector( ir_operand operand )
{
    assert( operand.kind == IR_O_SELECTOR );
    ir_selector sc = _f->selectors.at( operand.index );
    std::string_view sv( sc.text, sc.size );

    auto i = _selector_map.find( sv );
    if ( i != _selector_map.end() )
    {
        return { IR_O_SELECTOR, i->second };
    }

    // Maximum selector count is currently 0xFFFF.
    if ( _selectors.size() > 0xFFFF )
    {
        throw std::out_of_range( "too many selectors" );
    }

    unsigned index = _selectors.size();
    _selectors.push_back( sc );
    _selector_map.emplace( sv, index );
    return { IR_O_SELECTOR, index };
}

}
