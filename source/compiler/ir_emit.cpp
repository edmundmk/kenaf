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
#include <algorithm>
#include "ast.h"

namespace kf
{

const ir_emit::emit_shape ir_emit::SHAPES[] =
{
    { IR_LENGTH,        1, { IR_O_OP                            },  OP_LEN,         AB      },
    { IR_NEG,           1, { IR_O_OP                            },  OP_NEG,         AB      },
    { IR_POS,           1, { IR_O_OP                            },  OP_POS,         AB      },
    { IR_BITNOT,        1, { IR_O_OP                            },  OP_BITNOT,      AB      },
    { IR_MUL,           2, { IR_O_OP, IR_O_OP                   },  OP_MUL,         AB      },
    { IR_MUL,           2, { IR_O_OP, IR_O_NUMBER               },  OP_MULN,        AB      },
    { IR_DIV,           2, { IR_O_OP, IR_O_OP                   },  OP_DIV,         AB      },
    { IR_INTDIV,        2, { IR_O_OP, IR_O_OP                   },  OP_INTDIV,      AB      },
    { IR_MOD,           2, { IR_O_OP, IR_O_OP                   },  OP_MOD,         AB      },
    { IR_ADD,           2, { IR_O_OP, IR_O_OP                   },  OP_ADD,         AB      },
    { IR_ADD,           2, { IR_O_OP, IR_O_NUMBER               },  OP_ADDN,        AB      },
    { IR_SUB,           2, { IR_O_OP, IR_O_OP                   },  OP_SUB,         AB_SWAP },
    { IR_SUB,           2, { IR_O_NUMBER, IR_O_OP               },  OP_SUBN,        AB_SWAP },
    { IR_CONCAT,        2, { IR_O_OP, IR_O_OP                   },  OP_CONCAT,      AB      },
    { IR_CONCAT,        2, { IR_O_OP, IR_O_STRING               },  OP_CONCATS,     AB      },
    { IR_CONCAT,        2, { IR_O_STRING, IR_O_OP               },  OP_RCONCATS,    AB_SWAP },
    { IR_LSHIFT,        2, { IR_O_OP, IR_O_OP                   },  OP_LSHIFT,      AB      },
    { IR_RSHIFT,        2, { IR_O_OP, IR_O_OP                   },  OP_RSHIFT,      AB      },
    { IR_ASHIFT,        2, { IR_O_OP, IR_O_OP                   },  OP_ASHIFT,      AB      },
    { IR_BITAND,        2, { IR_O_OP, IR_O_OP                   },  OP_BITAND,      AB      },
    { IR_BITXOR,        2, { IR_O_OP, IR_O_OP                   },  OP_BITXOR,      AB      },
    { IR_BITOR,         2, { IR_O_OP, IR_O_OP                   },  OP_BITOR,       AB      },

    { IR_CONST,         1, { IR_O_NULL                          },  OP_LDV,         C       },
    { IR_CONST,         1, { IR_O_TRUE                          },  OP_LDV,         C       },
    { IR_CONST,         1, { IR_O_FALSE                         },  OP_LDV,         C       },
    { IR_CONST,         1, { IR_O_NUMBER                        },  OP_LDK,         C       },
    { IR_CONST,         1, { IR_O_STRING                        },  OP_LDK,         C       },

    { IR_EQ,            2, { IR_O_OP, IR_O_OP                   },  OP_JEQ,         JUMP    },
    { IR_EQ,            2, { IR_O_OP, IR_O_NUMBER               },  OP_JEQN,        JUMP    },
    { IR_EQ,            2, { IR_O_OP, IR_O_STRING               },  OP_JEQS,        JUMP    },
    { IR_NE,            2, { IR_O_OP, IR_O_OP                   },  OP_JEQ,         JUMP    },
    { IR_NE,            2, { IR_O_OP, IR_O_NUMBER               },  OP_JEQN,        JUMP    },
    { IR_NE,            2, { IR_O_OP, IR_O_STRING               },  OP_JEQS,        JUMP    },
    { IR_LT,            2, { IR_O_OP, IR_O_OP                   },  OP_JLT,         JUMP    },
    { IR_LT,            2, { IR_O_OP, IR_O_NUMBER               },  OP_JLTN,        JUMP    },
    { IR_LT,            2, { IR_O_NUMBER, IR_O_OP               },  OP_JGTN,        J_SWAP  },
    { IR_LE,            2, { IR_O_OP, IR_O_OP                   },  OP_JLE,         JUMP    },
    { IR_LE,            2, { IR_O_OP, IR_O_NUMBER               },  OP_JLEN,        JUMP    },
    { IR_LE,            2, { IR_O_NUMBER, IR_O_OP               },  OP_JGEN,        J_SWAP  },

    { IR_IS,            1, { IR_O_OP, IR_O_OP                   },  OP_IS,          AB      },
    { IR_NOT,           1, { IR_O_OP                            },  OP_NOT,         AB      },

    { IR_GET_GLOBAL,    1, { IR_O_SELECTOR                      },  OP_GET_GLOBAL,  C       },
    { IR_GET_KEY,       2, { IR_O_OP, IR_O_SELECTOR             },  OP_GET_KEY,     AB      },
    { IR_SET_KEY,       3, { IR_O_OP, IR_O_SELECTOR, IR_O_OP    },  OP_SET_KEY,     AB      },
    { IR_GET_INDEX,     2, { IR_O_OP, IR_O_OP                   },  OP_GET_INDEX,   AB      },
    { IR_GET_INDEX,     2, { IR_O_OP, IR_O_IMMEDIATE            },  OP_GET_INDEXI,  AB      },
    { IR_SET_INDEX,     3, { IR_O_OP, IR_O_OP, IR_O_OP          },  OP_SET_INDEX,   AB      },
    { IR_SET_INDEX,     3, { IR_O_OP, IR_O_IMMEDIATE, IR_O_OP   },  OP_SET_INDEXI,  AB      },
    { IR_NEW_ENV,       1, { IR_O_IMMEDIATE                     },  OP_NEW_ENV,     C       },
    { IR_GET_ENV,       2, { IR_O_OP, IR_O_ENVSLOT              },  OP_GET_VARENV,  AB      },
    { IR_GET_ENV,       2, { IR_O_OUTENV, IR_O_ENVSLOT          },  OP_GET_OUTENV,  AB      },
    { IR_SET_ENV,       3, { IR_O_OP, IR_O_ENVSLOT, IR_O_OP     },  OP_SET_VARENV,  AB      },
    { IR_SET_ENV,       3, { IR_O_OUTENV, IR_O_ENVSLOT, IR_O_OP },  OP_SET_OUTENV,  AB      },
    { IR_NEW_ARRAY,     1, { IR_O_IMMEDIATE                     },  OP_NEW_ARRAY,   C       },
    { IR_NEW_TABLE,     1, { IR_O_IMMEDIATE                     },  OP_NEW_TABLE,   C       },
    { IR_APPEND,        2, { IR_O_OP, IR_O_OP                   },  OP_APPEND,      AB_NO_R },

    { IR_JUMP_THROW,    1, { IR_O_OP                            },  OP_THROW,       AB_NO_R },

    { IR_OP_INVALID,    0, {                                    },  OP_MOV,         AB      },
};

ir_emit::ir_emit( report* report, code_unit* unit )
    :   _report( report )
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
    _u->function.code_flags = 0;
    if ( _f->ast->is_varargs )
    {
        _u->function.code_flags |= CODE_VARARG;
        _u->function.param_count -= 1;
    }
    if ( _f->ast->is_generator )
    {
        _u->function.code_flags |= CODE_GENERATOR;
    }
    _u->debug.function_name = _unit->debug_heap.size();
    _unit->debug_heap.insert( _unit->debug_heap.end(), _f->ast->name.begin(), _f->ast->name.end() );
    _unit->debug_heap.push_back( '\0' );

    emit_constants();
    assemble();
    fixup_jumps();

    _max_r = std::max( _max_r, _u->function.param_count + 1u );
    _u->function.stack_size = _max_r + 1;
    _fixups.clear();
    _labels.clear();
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
            kk = code_constant( (uint32_t)_unit->heap.size(), (uint32_t)k.size );
            _unit->heap.insert( _unit->heap.end(), k.text, k.text + k.size );
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
        _u->selectors.push_back( { (uint32_t)_unit->heap.size(), (uint32_t)k.size } );
        _unit->heap.insert( _unit->heap.end(), k.text, k.text + k.size );
    }

    _u->functions.reserve( _f->functions.size() );
    for ( const ast_function* f : _f->functions )
    {
        _u->functions.push_back( f->index );
    }
}

void ir_emit::assemble()
{
    for ( unsigned op_index = 0; op_index < _f->ops.size(); ++op_index )
    {
        const ir_op* iop = &_f->ops[ op_index ];
        if ( iop->opcode == IR_PHI || iop->opcode == IR_REF || iop->opcode == IR_NOP )
        {
            continue;
        }

        // Search for entry in shapes.
        const emit_shape* shape = std::lower_bound
        (
            std::begin( SHAPES ),
            std::end( SHAPES ),
            (ir_opcode)iop->opcode,
            []( const emit_shape& shape, ir_opcode iopcode ) { return shape.iopcode < iopcode; }
        );

        if ( shape != std::end( SHAPES ) && shape->iopcode == iop->opcode )
        {
            op_index = with_shape( op_index, iop, shape );
            continue;
        }

        switch ( iop->opcode )
        {
        case IR_PARAM:
        case IR_MOV:
        case IR_SELECT:
        {
            op_index = with_moves( op_index, iop );
            break;
        }

        case IR_CALL:
        case IR_YCALL:
        case IR_YIELD:
        case IR_VARARG:
        case IR_UNPACK:
        case IR_JUMP_RETURN:
        case IR_EXTEND:
        {
            op_index = with_stacked( op_index, iop );
            break;
        }

        case IR_JUMP_FOR_EGEN:
        {
            op_index = with_for_each( op_index, iop );
            break;
        }

        case IR_JUMP_FOR_SGEN:
        {
            op_index = with_for_step( op_index, iop );
            break;
        }

        case IR_NEW_FUNCTION:
        {
            assert( iop->ocount >= 2 );
            ir_operand operand = _f->operands[ iop->oindex + 0 ];
            assert( operand.kind == IR_O_IFUNCREF );
            if ( ! check_r( iop, "result" ) ) break;
            _max_r = std::max( _max_r, iop->r );
            emit( iop->sloc, op::op_c( OP_FUNCTION, iop->r, operand.index ) );

            ir_operand omethod = _f->operands[ iop->oindex + 1 ];
            if ( omethod.kind != IR_O_NONE )
            {
                assert( omethod.kind == IR_O_OP );
                const ir_op* oop = &_f->ops[ omethod.index ];
                if ( ! check_r( oop, "method object" ) ) break;
                emit( iop->sloc, op::op_ab( OP_F_METHOD, iop->r, oop->r, 0 ) );
            }

            unsigned outenv_index = 0;
            for ( unsigned j = 2; j < iop->ocount; ++j )
            {
                ir_operand operand = _f->operands[ iop->oindex + j ];
                if ( operand.kind == IR_O_OP )
                {
                    const ir_op* vop = &_f->ops[ operand.index ];
                    if ( ! check_r( vop, "varenv" ) ) break;
                    emit( iop->sloc, op::op_ab( OP_F_VARENV, iop->r, outenv_index++, vop->r ) );
                }
                else if ( operand.kind == IR_O_OUTENV )
                {
                    emit( iop->sloc, op::op_ab( OP_F_OUTENV, iop->r, outenv_index++, operand.index ) );
                }
                else
                {
                    assert( ! "invalid function environment operand" );
                }
            }
            break;
        }

        case IR_NEW_OBJECT:
        {
            if ( ! check_r( iop, "result" ) ) break;
            if ( ! check_s( iop, "constructor call" ) ) break;

            assert( iop->ocount == 1 );
            ir_operand a = _f->operands[ iop->oindex ];
            assert( a.kind == IR_O_OP );

            const ir_op* aop = &_f->ops[ a.index ];
            if ( ! check_r( aop, "a operand" ) ) break;

            emit( iop->sloc, op::op_ab( OP_NEW_OBJECT, iop->r, aop->r, iop->s ) );
            break;
        }

        case IR_SUPER:
        {
            if ( ! check_r( iop, "result" ) ) break;
            emit( iop->sloc, op::op_ab( OP_SUPER, iop->r, 0, 0 ) );
            break;
        }

        case IR_BLOCK:
        {
            _labels.push_back( { op_index, (unsigned)_u->ops.size() } );
            break;
        }

        case IR_JUMP:
        {
            assert( iop->ocount == 1 );
            ir_operand j = _f->operands[ iop->oindex ];
            assert( j.kind == IR_O_JUMP );
            if ( j.index != next( op_index, IR_BLOCK ) )
            {
                _fixups.push_back( { (unsigned)_u->ops.size(), j.index } );
                emit( iop->sloc, op::op_j( OP_JMP, 0, 0 ) );
            }
            break;
        }

        case IR_JUMP_TEST:
        {
            assert( iop->ocount == 3 );
            ir_operand u = _f->operands[ iop->oindex + 0 ];
            ir_operand jt = _f->operands[ iop->oindex + 1 ];
            ir_operand jf = _f->operands[ iop->oindex + 2 ];
            assert( u.kind == IR_O_OP );
            assert( jt.kind == IR_O_JUMP );
            assert( jf.kind == IR_O_JUMP );

            const ir_op* uop = &_f->ops[ u.index ];
            if ( ! check_r( uop, "test" ) ) break;

            bool test_true = true;
            unsigned jnext_index = next( op_index, IR_BLOCK );
            if ( jt.index == jnext_index )
            {
                std::swap( jt, jf );
                test_true = ! test_true;
            }

            _fixups.push_back( { (unsigned)_u->ops.size(), jt.index } );
            emit( iop->sloc, op::op_j( test_true ? OP_JT : OP_JF, uop->r, 0 ) );
            if ( jf.index != jnext_index )
            {
                _fixups.push_back( { (unsigned)_u->ops.size(), jf.index } );
                emit( iop->sloc, op::op_j( OP_JMP, 0, 0 ) );
            }

            break;
        }

        default:
        {
            _report->error( iop->sloc, "internal: unhanded ir opcode at :%04X", op_index );
            break;
        }
        }
    }
}

unsigned ir_emit::with_shape( unsigned op_index, const ir_op* iop, const emit_shape* shape )
{
    // Search for matching shape.
    while ( true )
    {
        if ( iop->opcode != shape->iopcode )
        {
            _report->error( iop->sloc, "internal: no matching instruction shape :%04X", op_index );
            return op_index;
        }

        assert( shape < std::end( SHAPES ) );
        if ( match_operands( iop, shape ) )
        {
            break;
        }

        ++shape;
    }

    // Comparison must always be followed by a jump instruction.
    unsigned jtest_index = op_index;
    unsigned jnext_index = op_index;
    if ( shape->kind == JUMP || shape->kind == J_SWAP )
    {
        while ( true )
        {
            jtest_index += 1;
            const ir_op* jop = &_f->ops[ jtest_index ];
            if ( jop->opcode == IR_JUMP_TEST )
            {
                assert( jop->ocount == 3 );
                ir_operand operand = _f->operands[ jop->oindex + 0 ];
                if ( operand.kind != IR_O_OP || operand.index != op_index )
                {
                    _report->error( iop->sloc, "internal: next jump after comparison does not use it" );
                    return op_index;
                }
                break;
            }
            if ( jop->opcode != IR_PHI && jop->opcode != IR_REF && jop->opcode != IR_NOP )
            {
                _report->error( iop->sloc, "internal: comparison without associated jump" );
                return op_index;
            }
        }

        jnext_index = next( jtest_index, IR_BLOCK );
    }

    // Get operands.
    uint8_t r = 0;
    if ( shape->kind == AB || shape->kind == AB_SWAP || shape->kind == C || shape->kind == J )
    {
        if ( shape->ocount < 3 )
        {
            if ( ! check_r( iop, "result" ) ) return op_index;
            r = iop->r;
        }
        else
        {
            ir_operand w = _f->operands[ iop->oindex + 2 ];
            assert( w.kind == IR_O_OP );
            const ir_op* wop = &_f->ops[ w.index ];
            if ( ! check_r( wop, "w operand" ) ) return op_index;
            r = wop->r;
        }

        _max_r = std::max< unsigned >( _max_r, r );
    }

    if ( shape->kind == C )
    {
        assert( shape->ocount == 1 );
        ir_operand u = _f->operands[ iop->oindex + 0 ];
        assert( u.kind != IR_O_OP );
        uint16_t c = 0;
        if ( u.kind == IR_O_TRUE )
            c = 2;
        else if ( u.kind == IR_O_FALSE )
            c = 1;
        else if ( u.kind == IR_O_NULL )
            c = 0;
        else
            c = u.index;
        emit( iop->sloc, op::op_c( shape->copcode, r, c ) );
        return op_index;
    }

    if ( shape->kind == J )
    {
        assert( shape->ocount == 1 );
        ir_operand u = _f->operands[ iop->oindex + 0 ];
        assert( u.kind == IR_O_IMMEDIATE );
        int16_t j = (int16_t)u.index;
        emit( iop->sloc, op::op_j( shape->copcode, r, j ) );
        return op_index;
    }

    assert( shape->ocount >= 1 );
    ir_operand u = _f->operands[ iop->oindex + 0 ];
    ir_operand v = shape->ocount >= 2 ? _f->operands[ iop->oindex + 1 ] : ir_operand{ IR_O_NONE };

    if ( shape->kind == AB_SWAP || shape->kind == J_SWAP )
    {
        std::swap( u, v );
    }

    uint8_t a;
    if ( u.kind == IR_O_OP )
    {
        assert( u.kind == IR_O_OP );
        const ir_op* uop = &_f->ops[ u.index ];
        if ( ! check_r( uop, "a operand" ) ) return op_index;
        a = uop->r;
    }
    else
    {
        a = u.index;
    }

    op op;
    uint8_t b = 0;
    if ( v.kind == IR_O_OP )
    {
        const ir_op* vop = &_f->ops[ v.index ];
        if ( ! check_r( vop, "b operand" ) ) return op_index;
        b = vop->r;
    }
    else
    {
        b = v.index;
    }

    op = op::op_ab( shape->copcode, r, a, b );

    if ( shape->kind == JUMP || shape->kind == J_SWAP )
    {
        // Comparison + jump.
        const ir_op* test = &_f->ops[ jtest_index ];
        assert( test->ocount == 3 );
        ir_operand jt = _f->operands[ test->oindex + 1 ];
        ir_operand jf = _f->operands[ test->oindex + 2 ];
        assert( jt.kind == IR_O_JUMP );
        assert( jf.kind == IR_O_JUMP );

        // r controls whether we jump on true ( r == 1 ) or false ( r == 0 ).
        op.r = iop->opcode != IR_NE;
        if ( jt.index == jnext_index )
        {
            std::swap( jt, jf );
            op.r = ! op.r;
        }

        // Emit jump.
        emit( iop->sloc, op );
        _fixups.push_back( { (unsigned)_u->ops.size(), jt.index } );
        emit( test->sloc, op::op_j( OP_JMP, 0, 0 ) );
        if ( jf.index != jnext_index )
        {
            _fixups.push_back( { (unsigned)_u->ops.size(), jf.index } );
            emit( test->sloc, op::op_j( OP_JMP, 0, 0 ) );
        }

        op_index = jtest_index;
    }
    else
    {
        emit( iop->sloc, op );
    }

    return op_index;
}

unsigned ir_emit::with_moves( unsigned op_index, const ir_op* iop )
{
    switch ( iop->opcode )
    {
    case IR_PARAM:
    {
        while ( true )
        {
            assert( iop->opcode == IR_PARAM );
            if ( ! check_r( iop, "result" ) ) return op_index;

            // Move from parameter.
            assert( iop->ocount == 1 );
            ir_operand operand = _f->operands[ iop->oindex ];
            assert( operand.kind == IR_O_LOCAL );
            move( iop->sloc, iop->r, operand.index + 1 );

            // Check if next instruction is a parameter.
            unsigned next_index = next( op_index, IR_PARAM );
            if ( next_index == IR_INVALID_INDEX )
            {
                break;
            }

            // Perform that move too.
            op_index = next_index;
            iop = &_f->ops[ op_index ];
        }
        move_emit();
        break;
    }

    case IR_SELECT:
    {
        while ( true )
        {
            assert( iop->opcode == IR_SELECT );
            if ( ! check_r( iop, "result" ) ) return op_index;

            // Move from multiple-result instruction.
            assert( iop->ocount == 2 );
            ir_operand u = _f->operands[ iop->oindex + 0 ];
            ir_operand i = _f->operands[ iop->oindex + 1 ];
            assert( u.kind == IR_O_OP );
            assert( i.kind == IR_O_SELECT );

            const ir_op* unpack = &_f->ops[ u.index ];
            if ( ! check_s( unpack, "unpack" ) ) return op_index;
            move( iop->sloc, iop->r, unpack->s + i.index );

            // Check if next instruction is another select.
            unsigned next_index = next( op_index, IR_SELECT );
            if ( next_index == IR_INVALID_INDEX )
            {
                break;
            }

            // Perform that select too.
            op_index = next_index;
            iop = &_f->ops[ op_index ];
        }
        move_emit();
        break;
    }

    case IR_MOV:
    {
        if ( ! check_r( iop, "result" ) ) return op_index;

        // Individual move.
        assert( iop->ocount == 1 );
        ir_operand operand = _f->operands[ iop->oindex ];
        assert( operand.kind == IR_O_OP );

        ir_op* mop = &_f->ops[ operand.index ];
        if ( ! check_r( mop, "move source" ) ) return op_index;

        move( iop->sloc, iop->r, mop->r );
        move_emit();
        break;
    }
    }

    return op_index;
}

unsigned ir_emit::with_stacked( unsigned op_index, const ir_op* iop )
{
    // Stacked instructions operate on the stack top at r.  They consume
    // registers r:a and produce results r:b.

    // Move all operands into the correct registers.
    if ( ! check_s( iop, "instruction" ) ) return op_index;
    unsigned sp = iop->s;

    unsigned j = 0;
    if ( iop->opcode == IR_UNPACK || iop->opcode == IR_EXTEND )
    {
        j += 1;
    }

    for ( ; j < iop->ocount; ++j )
    {
        ir_operand operand = _f->operands[ iop->oindex + j ];
        assert( operand.kind == IR_O_OP );
        const ir_op* uop = &_f->ops[ operand.index ];
        if ( uop->unpack() != IR_UNPACK_ALL )
        {
            if ( ! check_r( uop, "argument" ) ) return op_index;
            move( iop->sloc, sp, uop->r );
            sp += 1;
        }
        else
        {
            assert( j == iop->ocount - 1u );
        }
    }

    move_emit();

    unsigned a = IR_INVALID_REGISTER; // arguments
    unsigned b = IR_INVALID_REGISTER; // results

    opcode opcode;
    switch ( iop->opcode )
    {
    case IR_CALL:
        // Check if we can encode as CALLR.
        if ( iop->unpack() == 1 && iop->r != iop->s )
        {
            opcode = OP_CALLR;
            if ( ! check_r( iop, "call result" ) ) return op_index;
            _max_r = std::max( _max_r, iop->r );
            b = iop->r; // explicitly encoded result.
        }
        else
        {
            opcode = OP_CALL;
        }
        break;

    case IR_YCALL:
        opcode = OP_YCALL;
        break;

    case IR_YIELD:
        opcode = OP_YIELD;
        break;

    case IR_JUMP_RETURN:
        opcode = OP_RETURN;
        b = 0; // no results.
        break;

    case IR_VARARG:
        opcode = OP_VARARG;
        a = 0; // no arguments.
        break;

    case IR_UNPACK:
        // Get unpack argument.
        {
            opcode = OP_UNPACK;
            const ir_op* uop = u_operand( iop );
            if ( ! check_r( uop, "unpack argument" ) ) return op_index;
            a = uop->r; // explicitly encoded argument.
        }
        break;

    case IR_EXTEND:
        // Get extend target.
        {
            opcode = OP_EXTEND;
            const ir_op* uop = u_operand( iop );
            if ( ! check_r( uop, "extend target" ) ) return op_index;
            b = uop->r; // explicitly encoded argument, no results.
        }
        break;

    default:
        assert( ! "invalid stacked instruction" );
        return op_index;
    }

    // Arguments.
    if ( a == IR_INVALID_REGISTER )
    {
        // Get arguments.
        a = iop->s + iop->ocount;

        // Deal with unpack argument.
        if ( iop->ocount )
        {
            ir_operand operand = _f->operands[ iop->oindex + iop->ocount - 1 ];
            assert( operand.kind == IR_O_OP );
            const ir_op* uop = &_f->ops[ operand.index ];
            if ( uop->unpack() == IR_UNPACK_ALL )
            {
                a = OP_STACK_MARK;
            }
        }
    }

    // Results.
    unsigned r = iop->s;
    unsigned m = IR_INVALID_REGISTER;
    if ( b == IR_INVALID_REGISTER )
    {
        if ( iop->unpack() == 1 )
        {
            if ( ! check_r( iop, "result" ) ) return op_index;
            _max_r = std::max( _max_r, iop->r );

            if ( iop->opcode == IR_VARARG || iop->opcode == IR_UNPACK )
            {
                r = iop->r;
                b = iop->r + 1;
            }
            else
            {
                b = iop->r + 1;
                m = iop->r;
            }
        }
        else if ( iop->unpack() != IR_UNPACK_ALL )
        {
            b = iop->s + iop->unpack();
            _max_r = std::max( _max_r, b );
        }
        else
        {
            b = OP_STACK_MARK;
        }
    }

    // Emit.
    emit( iop->sloc, op::op_ab( opcode, r, a, b ) );

    // Move single result if necessary.
    if ( m != IR_INVALID_REGISTER )
    {
        move( iop->sloc, m, iop->s );
        move_emit();
    }

    return op_index;
}

unsigned ir_emit::with_for_each( unsigned op_index, const ir_op* iop )
{
    /*
        intermediate:
            %0 <- JUMP_FOR_EGEN g, :a
            BLOCK :a
            JUMP_FOR_EACH %0, :b, :break
            BLOCK :b
            %1 <- FOR_EACH_ITEMS %0, maybe unpacked

        code:
            GENERATE %0[2], g
            FOR_EACH %1, %0[2], @unpack / JMP :break
    */

    // Expect everything to have been emitted next to each other.
    unsigned block_a_index = next( op_index, IR_BLOCK );
    unsigned jump_index = next( block_a_index, IR_JUMP_FOR_EACH );
    unsigned block_b_index = next( jump_index, IR_BLOCK );
    unsigned items_index = next( block_b_index, IR_FOR_EACH_ITEMS );

    if ( block_a_index == IR_INVALID_INDEX || jump_index == IR_INVALID_INDEX
        || block_b_index == IR_INVALID_INDEX || items_index == IR_INVALID_INDEX )
    {
        _report->error( iop->sloc, "internal: malformed for-each loop" );
        return op_index;
    }

    op_index = items_index;

    // Generate the hidden variables from the generator.
    if ( ! check_r( iop, "for-each generator" ) ) return op_index;
    assert( iop->ocount == 2 );
    assert( _f->operands[ iop->oindex + 1 ].kind == IR_O_JUMP );
    assert( _f->operands[ iop->oindex + 1 ].index == block_a_index );
    ir_operand goperand = _f->operands[ iop->oindex + 0 ];

    assert( goperand.kind == IR_O_OP );
    const ir_op* gop = &_f->ops[ goperand.index ];
    if ( ! check_r( gop, "for-each g slot" ) ) return op_index;
    _max_r = std::max( _max_r, iop->r + 1u );
    emit( iop->sloc, op::op_ab( OP_GENERATE, iop->r, gop->r, 0 ) );

    // A block.
    _labels.push_back( { block_a_index, (unsigned)_u->ops.size() } );

    // Emit for loop instruction.
    const ir_op* jop = &_f->ops[ jump_index ];
    assert( jop->r == iop->r );
    assert( _f->operands[ jop->oindex + 0 ].kind == IR_O_OP );
    assert( _f->operands[ jop->oindex + 1 ].kind == IR_O_JUMP );
    assert( _f->operands[ jop->oindex + 1 ].index == block_b_index );
    ir_operand j = _f->operands[ jop->oindex + 2 ];
    assert( j.kind == IR_O_JUMP );

    const ir_op* xop = &_f->ops[ items_index ];
    if ( xop->unpack() == 1 )
    {
        if ( ! check_r( xop, "for-each i slot" ) ) return op_index;
        _max_r = std::max( _max_r, xop->r );
        emit( xop->sloc, op::op_ab( OP_FOR_EACH, xop->r, iop->r, xop->r + 1 ) );
    }
    else
    {
        if ( ! check_s( xop, "for-each unpack" ) ) return op_index;
        assert( xop->unpack() != IR_UNPACK_ALL );
        _max_r = std::max( _max_r, xop->s + xop->unpack() );
        emit( xop->sloc, op::op_ab( OP_FOR_EACH, xop->s, iop->r, xop->s + xop->unpack() ) );
    }

    _fixups.push_back( { (unsigned)_u->ops.size(), j.index } );
    emit( jop->sloc, op::op_j( OP_JMP, 0, 0 ) );

    // B block.
    _labels.push_back( { block_b_index, (unsigned)_u->ops.size() } );

    return op_index;
}

unsigned ir_emit::with_for_step( unsigned op_index, const ir_op* iop )
{
    /*
        intermediate:
            %0 <- JUMP_FOR_SGEN start, limit, step, :a
            BLOCK :a
            JUMP_FOR_STEP %0, :b, :break
            BLOCK :b
            %1 <- FOR_STEP_INDEX %0

        code:
            MOV %0[0], start
            MOV %0[1], limit
            MOV %0[2], step
            FOR_STEP %1, %0[3] / JMP :break
    */

    unsigned block_a_index = next( op_index, IR_BLOCK );
    unsigned jump_index = next( block_a_index, IR_JUMP_FOR_STEP );
    unsigned block_b_index = next( jump_index, IR_BLOCK );
    unsigned index_index = next( block_b_index, IR_FOR_STEP_INDEX );

    if ( block_a_index == IR_INVALID_INDEX || jump_index == IR_INVALID_INDEX
        || block_b_index == IR_INVALID_INDEX || index_index == IR_INVALID_INDEX )
    {
        _report->error( iop->sloc, "internal: malformed for-step loop" );
        return op_index;
    }

    op_index = index_index;

    // Move start, limit, step into the correct registers.
    if ( ! check_r( iop, "for-step slots" ) ) return op_index;

    assert( iop->ocount == 4 );
    assert( _f->operands[ iop->oindex + 3 ].kind == IR_O_JUMP );
    assert( _f->operands[ iop->oindex + 3 ].index == block_a_index );

    for ( unsigned j = 0; j < 3; ++j )
    {
        ir_operand operand = _f->operands[ iop->oindex + j ];
        assert( operand.kind == IR_O_OP );
        const ir_op* vop = &_f->ops[ operand.index ];
        if ( ! check_r( vop, "for-step start/limit/step" ) ) return op_index;
        move( vop->sloc, iop->r + j, vop->r );
    }
    move_emit();

    // A block.
    _labels.push_back( { block_a_index, (unsigned)_u->ops.size() } );

    // Emit for loop instruction.
    const ir_op* jop = &_f->ops[ jump_index ];
    assert( jop->r == iop->r );
    assert( _f->operands[ jop->oindex + 0 ].kind == IR_O_OP );
    assert( _f->operands[ jop->oindex + 1 ].kind == IR_O_JUMP );
    assert( _f->operands[ jop->oindex + 1 ].index == block_b_index );
    ir_operand j = _f->operands[ jop->oindex + 2 ];
    assert( j.kind == IR_O_JUMP );

    const ir_op* xop = &_f->ops[ index_index ];
    if ( xop->r == IR_INVALID_INDEX )
    {
        _report->error( iop->sloc, "internal: no allocated for-step index register" );
        return op_index;
    }

    _max_r = std::max( _max_r, xop->r );
    emit( xop->sloc, op::op_ab( OP_FOR_STEP, xop->r, iop->r, 0 ) );

    _fixups.push_back( { (unsigned)_u->ops.size(), j.index } );
    emit( jop->sloc, op::op_j( OP_JMP, 0, 0 ) );

    // B block.
    _labels.push_back( { block_b_index, (unsigned)_u->ops.size() } );

    return op_index;
}

unsigned ir_emit::next( unsigned op_index, ir_opcode iopcode )
{
    if ( op_index == IR_INVALID_INDEX )
    {
        return IR_INVALID_INDEX;
    }
    while ( ++op_index < _f->ops.size() )
    {
        const ir_op* iop = &_f->ops[ op_index ];
        if ( iop->opcode == iopcode )
        {
            return op_index;
        }
        if ( iop->opcode != IR_PHI && iop->opcode != IR_REF && iop->opcode != IR_NOP )
        {
            return IR_INVALID_INDEX;
        }
    }
    return IR_INVALID_INDEX;
}

bool ir_emit::match_operands( const ir_op* iop, const emit_shape* shape )
{
    if ( iop->ocount != shape->ocount )
    {
        return false;
    }

    for ( unsigned j = 0; j < iop->ocount; ++j )
    {
        ir_operand operand = _f->operands[ iop->oindex + j ];
        if ( operand.kind != shape->okind[ j ] )
        {
            return false;
        }
    }

    return true;
}

const ir_op* ir_emit::u_operand( const ir_op* iop )
{
    assert( iop->ocount >= 1 );
    ir_operand u = _f->operands[ iop->oindex ];
    assert( u.kind == IR_O_OP );
    return &_f->ops[ u.index ];
}

bool ir_emit::check_r( const ir_op* iop, const char* detail )
{
    if ( iop->r == IR_INVALID_REGISTER )
    {
        _report->error( iop->sloc, "internal: no allocated register for %s", detail );
        return false;
    }
    return true;
}

bool ir_emit::check_s( const ir_op* iop, const char* detail )
{
    if ( iop->s == IR_INVALID_REGISTER )
    {
        _report->error( iop->sloc, "internal: no allocated stack top for %s", detail );
        return false;
    }
    return true;
}

void ir_emit::move( srcloc sloc, unsigned target, unsigned source )
{
    if ( target != source )
    {
        _moves.push_back( { sloc, target, source } );
    }
}

bool ir_emit::move_is_source( unsigned target )
{
    // Do a linear search since the number of moves is likely to be tiny.
    for ( const move_entry& move : _moves )
    {
        if ( move.source == target )
        {
            return true;
        }
    }
    return false;
}

void ir_emit::move_emit()
{
    // Sort by target register.
    std::sort
    (
        _moves.begin(),
        _moves.end(),
        []( const move_entry& a, const move_entry& b ) { return a.target < b.target; }
    );

    // Perform moves where the target register is not used elsewhere.
    while ( true )
    {
        unsigned i = 0;
        for ( ; i < _moves.size(); ++i )
        {
            if ( ! move_is_source( _moves[ i ].target ) )
            {
                break;
            }
        }

        if ( i < _moves.size() )
        {
            move_entry move = _moves[ i ];
            _max_r = std::max( _max_r, move.target );
            emit( move.sloc, op::op_ab( OP_MOV, move.target, move.source, 0 ) );
            _moves.erase( _moves.begin() + i );
        }
        else
        {
            break;
        }
    }

    // Remaining moves form loops.
    for ( unsigned sweep = 0; sweep < _moves.size(); ++sweep )
    {
        move_entry move = _moves[ sweep ];

        // Previous swaps make last move in loop unecessary.
        if ( move.target == move.source )
        {
            continue;
        }

        // Swap.
        _max_r = std::max( _max_r, move.target );
        emit( move.sloc, op::op_ab( OP_SWP, move.target, move.source, 0 ) );

        // Replace occurrences of target in unperformed moves with source.
        for ( unsigned i = sweep + 1; i < _moves.size(); ++i )
        {
            move_entry& next = _moves[ i ];
            assert( next.target != move.target );
            if ( next.source == move.target )
            {
                next.source = move.source;
            }
        }
    }

    // Done.
    _moves.clear();
}

void ir_emit::emit( srcloc sloc, op op )
{
    _u->ops.push_back( op );
    _u->debug_slocs.push_back( sloc );
}

void ir_emit::fixup_jumps()
{
    for ( const jump_fixup& fixup : _fixups )
    {
        auto i = std::lower_bound
        (
            _labels.begin(),
            _labels.end(),
            fixup.iaddress,
            []( const jump_label& label, unsigned iaddress ) { return label.iaddress < iaddress; }
        );

        if ( i == _labels.end() || i->iaddress != fixup.iaddress )
        {
            _report->error( 0, "internal: jump :%04X to invalid address :%04X", fixup.jaddress, fixup.iaddress );
            continue;
        }

        op* jop = &_u->ops.at( fixup.jaddress );
        jop->j = i->caddress - ( (int)fixup.jaddress + 1 );
    }
}

}

