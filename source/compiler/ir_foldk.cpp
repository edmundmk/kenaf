//
//  ir_foldk.cpp
//
//  Created by Edmund Kapusniak on 15/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "ir_foldk.h"
#include <string.h>
#include "ir_fold.h"
#include "ast.h"

namespace kf
{

ir_foldk::ir_foldk( report* report )
    :   _report( report )
    ,   _f( nullptr )
{
    (void)_report;
}

ir_foldk::~ir_foldk()
{
}

bool ir_foldk::foldk( ir_function* function )
{
    _f = function;

    // Build lists of constants/selectors.
    inline_operands();
    alloc_constants();

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

void ir_foldk::inline_operands()
{
    /*
        The following constant operands can be inlined:

            ADD v, n            ->  ADDN v, n
            ADD n, v            ->  ADDN v, n
            SUB v, n            ->  ADDN v, -n
            SUB n, v            ->  SUBN v, n
            MUL v, n            ->  MULN v, n
            MUL n, v            ->  MULN v, n
            CONCAT v, s         ->  CONCATS v, s
            CONCAT s, v         ->  RCONCATS v, s
            EQ v, n; JUMP       ->  JEQTN v, n
            NE v, n; JUMP       ->  JEQFN v, n
            EQ v, s; JUMP       ->  JEQTS v, s
            NE v, s; JUMP       ->  JEQFS v, s
            LT v, n; JUMP       ->  JLTTN v, n
            LT n, v; JUMP       ->  JGTTN v, n
            LE v, n; JUMP       ->  JLETN v, n
            LE n, v; JUMP       ->  JGETN v, n
            GET_INDEX v, b      ->  GET_INDEXI v, b
            SET_INDEX v, b, u   ->  SET_INDEXI v, b, u

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
            ir_operand* u = &_f->operands[ op->oindex + 0 ];
            ir_operand* v = &_f->operands[ op->oindex + 1 ];
            ir_operand fold_u = ir_fold_operand( _f, *u );
            ir_operand fold_v = ir_fold_operand( _f, *v );

            if ( fold_v.kind == IR_O_NUMBER )
            {
                // Second operand is constant.
                *v = insert_number( fold_v );
            }
            else if ( fold_u.kind == IR_O_NUMBER )
            {
                // Operation is commutative, switch operands.
                *u = *v;
                *v = insert_number( fold_u );
            }

            break;
        }

        case IR_SUB:
        {
            ir_operand* u = &_f->operands[ op->oindex + 0 ];
            ir_operand* v = &_f->operands[ op->oindex + 1 ];
            ir_operand fold_u = ir_fold_operand( _f, *u );
            ir_operand fold_v = ir_fold_operand( _f, *v );

            if ( fold_v.kind == IR_O_NUMBER )
            {
                // Convert to ADD with negated operand.
                double constant = _f->constants[ fold_v.index ].n;
                fold_v.index = _f->constants.append( ir_constant( -constant ) );
                op->opcode = IR_ADD;
                *v = insert_number( fold_v );
            }
            else if ( fold_u.kind == IR_O_NUMBER )
            {
                // First operand is constant.
                *u = insert_number( fold_u );
            }

            break;
        }

        case IR_CONCAT:
        {
            ir_operand* u = &_f->operands[ op->oindex + 0 ];
            ir_operand* v = &_f->operands[ op->oindex + 1 ];
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
            ir_op* jump = &_f->ops[ op_index + 1 ];
            if ( jump->opcode != IR_JUMP_TEST )
            {
                break;
            }
            ir_operand test = _f->operands[ jump->oindex ];
            if ( test.kind != IR_O_OP || test.index != op_index )
            {
                break;
            }

            // Can inline.
            ir_operand* u = &_f->operands[ op->oindex + 0 ];
            ir_operand* v = &_f->operands[ op->oindex + 1 ];
            ir_operand fold_u = ir_fold_operand( _f, *u );
            ir_operand fold_v = ir_fold_operand( _f, *v );

            if ( fold_v.kind == IR_O_NUMBER )
            {
                *v = insert_number( fold_v );
            }
            else if ( ( op->opcode == IR_EQ || op->opcode == IR_NE ) && fold_v.kind == IR_O_STRING )
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
            else if ( ( op->opcode == IR_EQ || op->opcode == IR_NE ) && fold_u.kind == IR_O_STRING )
            {
                *u = insert_string( fold_u );
                std::swap( *u, *v );
            }

            break;
        }

        case IR_GET_KEY:
        case IR_SET_KEY:
        {
            // Do key selectors in first pass.
            ir_operand* s = &_f->operands[ op->oindex + 1 ];
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
            ir_operand* i = &_f->operands[ op->oindex + 1 ];
            ir_operand fold_i = ir_fold_operand( _f, *i );

            if ( fold_i.kind == IR_O_NUMBER )
            {
                *i = b_immediate( fold_i );
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

void ir_foldk::alloc_constants()
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
            ir_operand* u = &_f->operands[ op->oindex ];
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
            ir_operand* s = &_f->operands[ op->oindex ];
            *s = insert_selector( *s );
            break;
        }

        case IR_NEW_FUNCTION:
        {
            ir_operand* f = &_f->operands[ op->oindex ];
            *f = insert_function( *f );
            break;
        }

        default: break;
        }
    }
}

ir_operand ir_foldk::b_immediate( ir_operand operand )
{
    if ( operand.kind == IR_O_NUMBER )
    {
        double number = _f->constants[ operand.index ].n;
        uint8_t imm8 = (uint8_t)number;
        if ( (double)imm8 == number )
        {
            return { IR_O_IMMEDIATE, (unsigned)imm8 };
        }
    }

    return operand;
}

ir_operand ir_foldk::insert_number( ir_operand operand )
{
    assert( operand.kind == IR_O_NUMBER );
    double number = _f->constants[ operand.index ].n;

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

ir_operand ir_foldk::insert_string( ir_operand operand )
{
    assert( operand.kind == IR_O_STRING );
    ir_constant sc = _f->constants[ operand.index ];
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

ir_operand ir_foldk::insert_selector( ir_operand operand )
{
    assert( operand.kind == IR_O_SELECTOR );
    ir_selector sc = _f->selectors[ operand.index ];
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

ir_operand ir_foldk::insert_function( ir_operand operand )
{
    assert( operand.kind == IR_O_FUNCTION );
    for ( unsigned index = 0; index < _f->functions.size(); ++index )
    {
        if ( _f->functions[ index ]->index == operand.index )
        {
            return { IR_O_IFUNCREF, index };
        }
    }

    ast_function* function = _f->ast->script->functions[ operand.index ].get();
    return { IR_O_IFUNCREF, _f->functions.append( function ) };
}

}
