//
//  ir_emit.cpp
//
//  Created by Edmund Kapusniak on 19/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "ir_emit.h"
#include "ast.h"

namespace kf
{

ir_emit::ir_emit( source* source, code_unit* unit )
    :   _source( source )
    ,   _unit( unit )
    ,   _f( nullptr )
    ,   _max_r( 0 )
{
}

ir_emit::~ir_emit()
{
}

void ir_emit::emit( ir_function* function )
{
    _f = function;

    _u = std::make_unique< code_function_unit >();
    _u->function.outenv_count = _f->ast->outenvs.size();
    _u->function.param_count = _f->ast->parameter_count;
    _u->function.stack_size = 0;
    _u->function.flags = 0;
    if ( _f->ast->is_varargs )
        _u->function.flags |= CODE_FLAGS_VARARGS;
    if ( _f->ast->is_generator )
        _u->function.flags |= CODE_FLAGS_GENERATOR;
    _u->debug.function_name = _unit->debug_heap.size();
    _unit->debug_heap.insert( _unit->debug_heap.end(), _f->ast->name.begin(), _f->ast->name.end() );
    _unit->debug_heap.push_back( '\0' );

    emit_constants();
    assemble();

    _u->function.stack_size = _max_r + 1;
    _max_r = 0;

    _unit->functions.push_back( std::move( _u ) );
}

void ir_emit::emit_constants()
{
    _u->constants.reserve( _f->constants.size() );
    for ( const ir_constant& k : _f->constants )
    {
        code_constant kk;
        if ( k.text )
        {
            kk = code_constant( (uint32_t)_unit->heap.size() );
            _unit->heap.insert( _unit->heap.end(), k.text, k.text + k.size );
            _unit->heap.push_back( '\0' );
        }
        else
        {
            kk = code_constant( k.n );
        }
        _u->constants.push_back( kk );
    }

    _u->selectors.reserve( _f->selectors.size() );
    for ( const ir_selector& k : _f->selectors )
    {
        _u->selectors.push_back( { (uint32_t)_unit->heap.size() } );
        _unit->heap.insert( _unit->heap.end(), k.text, k.text + k.size );
        _unit->heap.push_back( '\0' );
    }
}

void ir_emit::assemble()
{
    for ( unsigned op_index = 0; op_index < _f->ops.size(); ++op_index )
    {
        const ir_op* op = &_f->ops[ op_index ];

        switch ( op->opcode )
        {
        case IR_LENGTH:         op_unary( op, OP_LEN );                         break;
        case IR_NEG:            op_unary( op, OP_NEG );                         break;
        case IR_POS:            op_unary( op, OP_POS );                         break;
        case IR_BITNOT:         op_unary( op, OP_BITNOT );                      break;
        case IR_NOT:            op_unary( op, OP_NOT );                         break;
        case IR_SUPER:          op_unary( op, OP_SUPER );                       break;
        case IR_JUMP_THROW:     op_unary( op, OP_THROW );                       break;

        case IR_DIV:            op_binary( op, OP_DIV );                        break;
        case IR_INTDIV:         op_binary( op, OP_INTDIV );                     break;
        case IR_MOD:            op_binary( op, OP_MOD );                        break;
        case IR_LSHIFT:         op_binary( op, OP_LSHIFT );                     break;
        case IR_RSHIFT:         op_binary( op, OP_RSHIFT );                     break;
        case IR_ASHIFT:         op_binary( op, OP_ASHIFT );                     break;
        case IR_BITAND:         op_binary( op, OP_BITAND );                     break;
        case IR_BITXOR:         op_binary( op, OP_BITXOR );                     break;
        case IR_BITOR:          op_binary( op, OP_BITOR );                      break;
        case IR_IS:             op_binary( op, OP_IS );                         break;

        case IR_ADD:            op_addmul( op, OP_ADD, OP_ADDK, OP_ADDI );      break;
        case IR_SUB:            op_addmul( op, OP_SUB, OP_SUBK, OP_SUBI );      break;
        case IR_MUL:            op_addmul( op, OP_MUL, OP_MULK, OP_MULI );      break;
        case IR_CONCAT:         op_concat( op );                                break;

        case IR_CONST:          op_const( op );                                 break;

        case IR_GET_GLOBAL:     op_genc( op, OP_GET_GLOBAL, IR_O_SELECTOR );    break;
        case IR_NEW_ENV:        op_genc( op, OP_NEW_ENV, IR_O_IMMEDIATE );      break;
        case IR_NEW_ARRAY:      op_genc( op, OP_NEW_ARRAY, IR_O_IMMEDIATE );    break;
        case IR_NEW_TABLE:      op_genc( op, OP_NEW_TABLE, IR_O_IMMEDIATE );    break;


        default: break;
        }
    }
}

void ir_emit::op_unary( const ir_op* rop, opcode o )
{
    assert( rop->ocount == 1 );
    ir_operand u = _f->operands[ rop->oindex ];
    if ( u.kind != IR_O_OP )
    {
        _source->error( rop->sloc, "internal: invalid operand to unary instruction" );
        return;
    }
    const ir_op* uop = &_f->ops[ u.index ];

    if ( rop->r == IR_INVALID_REGISTER || uop->r == IR_INVALID_REGISTER )
    {
        _source->error( rop->sloc, "internal: invalid register allocation" );
        return;
    }

    _max_r = std::max( _max_r, rop->r );
    emit( rop->sloc, op::op_ab( o, rop->r, uop->r, 0 ) );
}

void ir_emit::op_binary( const ir_op* rop, opcode o )
{
    assert( rop->ocount == 2 );
    ir_operand u = _f->operands[ rop->oindex + 0 ];
    ir_operand v = _f->operands[ rop->oindex + 1 ];
    if ( u.kind != IR_O_OP || v.kind != IR_O_OP )
    {
        _source->error( rop->sloc, "internal: invalid operand to binary instruction" );
        return;
    }
    const ir_op* uop = &_f->ops[ u.index ];
    const ir_op* vop = &_f->ops[ v.index ];

    if ( rop->r == IR_INVALID_REGISTER || uop->r == IR_INVALID_REGISTER || vop->r == IR_INVALID_REGISTER )
    {
        _source->error( rop->sloc, "internal: invalid register allocation" );
        return;
    }

    _max_r = std::max( _max_r, rop->r );
    emit( rop->sloc, op::op_ab( o, rop->r, uop->r, vop->r ) );
}

void ir_emit::op_addmul( const ir_op* rop, opcode o, opcode ok, opcode oi )
{
    assert( rop->ocount == 2 );
    ir_operand u = _f->operands[ rop->oindex + 0 ];
    ir_operand v = _f->operands[ rop->oindex + 1 ];

    // Operands to SUB instruction are reversed, to put constant in b slot.
    if ( rop->opcode == IR_SUB )
    {
        std::swap( u, v );
    }

    // Check registers.
    if ( u.kind != IR_O_OP )
    {
        _source->error( rop->sloc, "internal: invalid operand to addmul instruction" );
        return;
    }
    const ir_op* uop = &_f->ops[ u.index ];
    if ( rop->r == IR_INVALID_REGISTER || uop->r == IR_INVALID_REGISTER )
    {
        _source->error( rop->sloc, "internal: invalid register allocation" );
        return;
    }

    _max_r = std::max( _max_r, rop->r );

    // Select instruction variant.
    if ( v.kind == IR_O_OP )
    {
        const ir_op* vop = &_f->ops[ v.index ];
        if ( vop->r == IR_INVALID_REGISTER )
        {
            _source->error( rop->sloc, "internal: invalid register allocation" );
            return;
        }

        emit( rop->sloc, op::op_ab( o, rop->r, uop->r, vop->r ) );
    }
    else if ( v.kind == IR_O_NUMBER )
    {
        if ( v.index > 0xFF )
        {
            _source->error( rop->sloc, "internal: invalid constant index" );
            return;
        }

        emit( rop->sloc, op::op_ab( ok, rop->r, uop->r, v.index ) );
    }
    else if ( v.kind == IR_O_IMMEDIATE )
    {
        emit( rop->sloc, op::op_ai( oi, rop->r, uop->r, (int8_t)v.index ) );
    }
    else
    {
        _source->error( rop->sloc, "internal: invalid second operand to addmul instruction" );
    }
}

void ir_emit::op_concat( const ir_op* rop )
{
    opcode ok = OP_CONCATK;
    assert( rop->ocount == 2 );
    ir_operand u = _f->operands[ rop->oindex + 0 ];
    ir_operand v = _f->operands[ rop->oindex + 1 ];

    if ( u.kind == IR_O_STRING )
    {
        ok = OP_RCONCATK;
        std::swap( u, v );
    }

    if ( u.kind != IR_O_OP )
    {
        _source->error( rop->sloc, "internal: invalid operand to concat instruction" );
        return;
    }
    const ir_op* uop = &_f->ops[ u.index ];
    if ( rop->r == IR_INVALID_REGISTER || uop->r == IR_INVALID_REGISTER )
    {
        _source->error( rop->sloc, "internal: invalid register allocation" );
        return;
    }

    _max_r = std::max( _max_r, rop->r );

    if ( v.kind == IR_O_OP )
    {
        const ir_op* vop = &_f->ops[ v.index ];
        if ( vop->r == IR_INVALID_REGISTER )
        {
            _source->error( rop->sloc, "internal: invalid register allocation" );
            return;
        }

        emit( rop->sloc, op::op_ab( OP_CONCAT, rop->r, uop->r, vop->r ) );
    }
    else if ( v.kind == IR_O_STRING )
    {
        if ( v.index > 0xFF )
        {
            _source->error( rop->sloc, "internal: invalid constant index" );
            return;
        }

        emit( rop->sloc, op::op_ab( ok, rop->r, uop->r, v.index ) );
    }
    else
    {
        _source->error( rop->sloc, "internal: invalid second operand to concat instruction" );
    }
}

void ir_emit::op_const( const ir_op* rop )
{
    assert( rop->ocount == 1 );
    ir_operand k = _f->operands[ rop->oindex ];

    if ( rop->r == IR_INVALID_REGISTER )
    {
        _source->error( rop->sloc, "internal: invalid register allocation" );
        return;
    }

    _max_r = std::max( _max_r, rop->r );

    if ( k.kind == IR_O_NUMBER || k.kind == IR_O_STRING )
    {
        emit( rop->sloc, op::op_c( OP_LDK, rop->r, k.index ) );
    }
    else if ( k.kind == IR_O_NULL )
    {
        emit( rop->sloc, op::op_c( OP_NULL, rop->r, 0 ) );
    }
    else if ( k.kind == IR_O_TRUE )
    {
        emit( rop->sloc, op::op_c( OP_BOOL, rop->r, 1 ) );
    }
    else if ( k.kind == IR_O_FALSE )
    {
        emit( rop->sloc, op::op_c( OP_BOOL, rop->r, 0 ) );
    }
    else
    {
        _source->error( rop->sloc, "internal: invalid constant operand" );
    }
}

void ir_emit::op_genc( const ir_op* rop, opcode o, ir_operand_kind okind )
{
    assert( rop->ocount == 1 );
    ir_operand k = _f->operands[ rop->oindex ];
    if ( k.kind != okind )
    {
        _source->error( rop->sloc, "internal: invalid c operand" );
        return;
    }

    if ( rop->r == IR_INVALID_REGISTER )
    {
        _source->error( rop->sloc, "internal: invalid register allocation" );
        return;
    }

    _max_r = std::max( _max_r, rop->r );
    emit( rop->sloc, op::op_c( o, rop->r, k.index ) );
}

void ir_emit::emit( srcloc sloc, op op )
{
    _u->ops.push_back( op );
    _u->debug_slocs.push_back( sloc );
}


}

