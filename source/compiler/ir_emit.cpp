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
    { IR_MUL,           2, { IR_O_OP, IR_O_NUMBER               },  OP_MULK,        AB      },
    { IR_MUL,           2, { IR_O_OP, IR_O_IMMEDIATE            },  OP_MULI,        AI      },
    { IR_DIV,           2, { IR_O_OP, IR_O_OP                   },  OP_DIV,         AB      },
    { IR_INTDIV,        2, { IR_O_OP, IR_O_OP                   },  OP_INTDIV,      AB      },
    { IR_MOD,           2, { IR_O_OP, IR_O_OP                   },  OP_MOD,         AB      },
    { IR_ADD,           2, { IR_O_OP, IR_O_OP                   },  OP_ADD,         AB      },
    { IR_ADD,           2, { IR_O_OP, IR_O_NUMBER               },  OP_ADDK,        AB      },
    { IR_ADD,           2, { IR_O_OP, IR_O_IMMEDIATE            },  OP_ADDI,        AI      },
    { IR_SUB,           2, { IR_O_OP, IR_O_OP                   },  OP_SUB,         AB_SWAP },
    { IR_SUB,           2, { IR_O_NUMBER, IR_O_OP               },  OP_SUBK,        AB_SWAP },
    { IR_SUB,           2, { IR_O_IMMEDIATE, IR_O_OP            },  OP_SUBI,        AI_SWAP },
    { IR_CONCAT,        2, { IR_O_OP, IR_O_OP                   },  OP_CONCAT,      AB      },
    { IR_CONCAT,        2, { IR_O_OP, IR_O_STRING               },  OP_CONCATK,     AB      },
    { IR_CONCAT,        2, { IR_O_STRING, IR_O_OP               },  OP_RCONCATK,    AB_SWAP },
    { IR_LSHIFT,        2, { IR_O_OP, IR_O_OP                   },  OP_LSHIFT,      AB      },
    { IR_RSHIFT,        2, { IR_O_OP, IR_O_OP                   },  OP_RSHIFT,      AB      },
    { IR_ASHIFT,        2, { IR_O_OP, IR_O_OP                   },  OP_ASHIFT,      AB      },
    { IR_BITAND,        2, { IR_O_OP, IR_O_OP                   },  OP_BITAND,      AB      },
    { IR_BITXOR,        2, { IR_O_OP, IR_O_OP                   },  OP_BITXOR,      AB      },
    { IR_BITOR,         2, { IR_O_OP, IR_O_OP                   },  OP_BITOR,       AB      },

    { IR_CONST,         1, { IR_O_NULL                          },  OP_NULL,        C       },
    { IR_CONST,         1, { IR_O_TRUE                          },  OP_BOOL,        C       },
    { IR_CONST,         1, { IR_O_FALSE                         },  OP_BOOL,        C       },
    { IR_CONST,         1, { IR_O_NUMBER                        },  OP_LDK,         C       },
    { IR_CONST,         1, { IR_O_STRING                        },  OP_LDK,         C       },

    { IR_EQ,            2, { IR_O_OP, IR_O_OP                   },  OP_JEQ,         JUMP    },
    { IR_EQ,            2, { IR_O_OP, IR_O_NUMBER               },  OP_JEQK,        JUMP    },
    { IR_EQ,            2, { IR_O_OP, IR_O_STRING               },  OP_JEQK,        JUMP    },
    { IR_EQ,            2, { IR_O_NUMBER, IR_O_OP               },  OP_JEQK,        J_SWAP  },
    { IR_EQ,            2, { IR_O_STRING, IR_O_OP               },  OP_JEQK,        J_SWAP  },
    { IR_NE,            2, { IR_O_OP, IR_O_OP                   },  OP_JEQ,         JUMP    },
    { IR_NE,            2, { IR_O_OP, IR_O_NUMBER               },  OP_JEQK,        JUMP    },
    { IR_NE,            2, { IR_O_OP, IR_O_STRING               },  OP_JEQK,        JUMP    },
    { IR_NE,            2, { IR_O_NUMBER, IR_O_OP               },  OP_JEQK,        J_SWAP  },
    { IR_NE,            2, { IR_O_STRING, IR_O_OP               },  OP_JEQK,        J_SWAP  },
    { IR_LT,            2, { IR_O_OP, IR_O_OP                   },  OP_JLT,         JUMP    },
    { IR_LT,            2, { IR_O_OP, IR_O_NUMBER               },  OP_JLTK,        JUMP    },
    { IR_LT,            2, { IR_O_OP, IR_O_STRING               },  OP_JLTK,        JUMP    },
    { IR_LT,            2, { IR_O_NUMBER, IR_O_OP               },  OP_JGTK,        J_SWAP  },
    { IR_LT,            2, { IR_O_STRING, IR_O_OP               },  OP_JGTK,        J_SWAP  },
    { IR_LE,            2, { IR_O_OP, IR_O_OP                   },  OP_JLE,         JUMP    },
    { IR_LE,            2, { IR_O_OP, IR_O_NUMBER               },  OP_JLEK,        JUMP    },
    { IR_LE,            2, { IR_O_OP, IR_O_STRING               },  OP_JLEK,        JUMP    },
    { IR_LE,            2, { IR_O_NUMBER, IR_O_OP               },  OP_JGEK,        J_SWAP  },
    { IR_LE,            2, { IR_O_STRING, IR_O_OP               },  OP_JGEK,        J_SWAP  },

    { IR_IS,            1, { IR_O_OP, IR_O_OP                   },  OP_IS,          AB      },
    { IR_NOT,           1, { IR_O_OP                            },  OP_NOT,         AB      },

    { IR_GET_GLOBAL,    1, { IR_O_SELECTOR                      },  OP_GET_GLOBAL,  C       },
    { IR_GET_KEY,       2, { IR_O_OP, IR_O_SELECTOR             },  OP_GET_KEY,     AB      },
    { IR_SET_KEY,       3, { IR_O_OP, IR_O_SELECTOR, IR_O_OP    },  OP_SET_KEY,     AB      },
    { IR_GET_INDEX,     2, { IR_O_OP, IR_O_OP                   },  OP_SET_INDEX,   AB      },
    { IR_GET_INDEX,     2, { IR_O_OP, IR_O_NUMBER               },  OP_SET_INDEXK,  AB      },
    { IR_GET_INDEX,     2, { IR_O_OP, IR_O_STRING               },  OP_SET_INDEXK,  AB      },
    { IR_GET_INDEX,     2, { IR_O_OP, IR_O_IMMEDIATE            },  OP_SET_INDEXI,  AI      },
    { IR_SET_INDEX,     3, { IR_O_OP, IR_O_OP, IR_O_OP          },  OP_SET_INDEX,   AB      },
    { IR_SET_INDEX,     3, { IR_O_OP, IR_O_NUMBER, IR_O_OP      },  OP_SET_INDEXK,  AB      },
    { IR_SET_INDEX,     3, { IR_O_OP, IR_O_STRING, IR_O_OP      },  OP_SET_INDEXK,  AB      },
    { IR_SET_INDEX,     3, { IR_O_OP, IR_O_IMMEDIATE, IR_O_OP   },  OP_SET_INDEXI,  AI      },
    { IR_NEW_ENV,       1, { IR_O_IMMEDIATE                     },  OP_NEW_ENV,     C       },
    { IR_GET_ENV,       2, { IR_O_OP, IR_O_ENVSLOT              },  OP_GET_VARENV,  AB      },
    { IR_GET_ENV,       2, { IR_O_OUTENV, IR_O_ENVSLOT          },  OP_GET_OUTENV,  AB      },
    { IR_SET_ENV,       3, { IR_O_OP, IR_O_ENVSLOT, IR_O_OP     },  OP_SET_VARENV,  AB      },
    { IR_SET_ENV,       3, { IR_O_OUTENV, IR_O_ENVSLOT, IR_O_OP },  OP_SET_OUTENV,  AB      },
    { IR_NEW_OBJECT,    1, { IR_O_OP                            },  OP_NEW_OBJECT,  AB      },
    { IR_NEW_ARRAY,     1, { IR_O_IMMEDIATE                     },  OP_NEW_ARRAY,   C       },
    { IR_NEW_TABLE,     1, { IR_O_IMMEDIATE                     },  OP_NEW_TABLE,   C       },
    { IR_SUPER,         1, { IR_O_OP                            },  OP_SUPER,       AB      },
    { IR_APPEND,        2, { IR_O_OP, IR_O_OP                   },  OP_APPEND,      AB_NO_R },

    { IR_JUMP_THROW,    1, { IR_O_OP                            },  OP_THROW,       AB      },

    { IR_OP_INVALID,    0, {                                    },  OP_MOV,         AB      },
};

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
    fixup_jumps();

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
        const ir_op* iop = &_f->ops[ op_index ];

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

        // Jump instructions.
        switch ( iop->opcode )
        {
        case IR_BLOCK:
        {
            _labels.push_back( { op_index, (unsigned)_u->ops.size() } );
            continue;
        }

        case IR_JUMP:
        {
            assert( iop->ocount == 1 );
            ir_operand j = _f->operands[ iop->oindex ];
            assert( j.kind == IR_O_JUMP );
            if ( j.index != next_block( op_index ) )
            {
                _fixups.push_back( { (unsigned)_u->ops.size(), j.index } );
                emit( iop->sloc, op::op_j( OP_JMP, 0, 0 ) );
            }
            continue;
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
            if ( uop->r == IR_INVALID_REGISTER )
            {
                _source->error( iop->sloc, "internal: no allocated test register" );
                continue;
            }

            bool test_true = true;
            unsigned jnext_index = next_block( op_index );
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

            continue;
        }
        }
    }
}

unsigned ir_emit::next_block( unsigned op_index )
{
    while ( ++op_index < _f->ops.size() )
    {
        const ir_op* bop = &_f->ops[ op_index ];
        if ( bop->opcode == IR_BLOCK )
        {
            return op_index;
        }
        if ( bop->opcode != IR_PHI && bop->opcode != IR_REF && bop->opcode != IR_NOP )
        {
            _source->error( bop->sloc, "internal: jump not followed by block" );
            break;
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

unsigned ir_emit::with_shape( unsigned op_index, const ir_op* iop, const emit_shape* shape )
{
    // Search for matching shape.
    while ( true )
    {
        if ( iop->opcode != shape->iopcode )
        {
            _source->error( iop->sloc, "internal: no matching instruction shape :%04X", op_index );
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
                    _source->error( iop->sloc, "internal: next jump after comparison does not use it" );
                    return op_index;
                }
                break;
            }
            if ( jop->opcode != IR_PHI && jop->opcode != IR_REF && jop->opcode != IR_NOP )
            {
                _source->error( iop->sloc, "internal: comparison without associated jump" );
                return op_index;
            }
        }

        jnext_index = next_block( jtest_index );
    }

    // Get operands.
    uint8_t r = 0;
    if ( shape->kind == AB || shape->kind == AB_SWAP || shape->kind == AI || shape->kind == AI_SWAP || shape->kind == C )
    {
        if ( shape->ocount < 3 )
        {
            if ( iop->r == IR_INVALID_REGISTER )
            {
                _source->error( iop->sloc, "internal: no allocated result register" );
                return op_index;
            }

            r = iop->r;
        }
        else
        {
            ir_operand w = _f->operands[ iop->oindex + 2 ];
            assert( w.kind == IR_O_OP );

            const ir_op* wop = &_f->ops[ w.index ];
            if ( wop->r == IR_INVALID_REGISTER )
            {
                _source->error( iop->sloc, "internal: no allocated w register" );
                return op_index;
            }

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
            c = 1;
        else if ( u.kind == IR_O_FALSE )
            c = 0;
        else
            c = u.index;
        emit( iop->sloc, op::op_c( shape->copcode, r, c ) );
        return op_index;
    }

    assert( shape->ocount >= 1 );
    ir_operand u = _f->operands[ iop->oindex + 0 ];
    ir_operand v = shape->ocount >= 2 ? _f->operands[ iop->oindex + 1 ] : ir_operand{ IR_O_NONE };

    if ( shape->kind == AB_SWAP || shape->kind == AI_SWAP || shape->kind == J_SWAP )
    {
        std::swap( u, v );
    }

    uint8_t a;
    if ( u.kind == IR_O_OP )
    {
        assert( u.kind == IR_O_OP );
        const ir_op* uop = &_f->ops[ u.index ];
        if ( uop->r == IR_INVALID_REGISTER )
        {
            _source->error( iop->sloc, "internal: no allocated a register" );
            return op_index;
        }
    }
    else
    {
        a = u.index;
    }

    op op;
    if ( shape->kind == AI || shape->kind == AI_SWAP )
    {
        assert( v.kind == IR_O_IMMEDIATE );
        int8_t i = (int8_t)v.index;
        op = op::op_ai( shape->copcode, r, a, i );
    }
    else
    {
        uint8_t b = 0;
        if ( v.kind == IR_O_OP )
        {
            const ir_op* vop = &_f->ops[ v.index ];
            if ( vop->r == IR_INVALID_REGISTER )
            {
                _source->error( iop->sloc, "internal: no allocated b register" );
                return op_index;
            }

            b = vop->r;
        }
        else
        {
            b = v.index;
        }

        op = op::op_ab( shape->copcode, r, a, b );
    }

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
            _source->error( 0, "internal: jump :%04X to invalid address :%04X", fixup.jaddress, fixup.iaddress );
            continue;
        }

        op* jop = &_u->ops.at( fixup.jaddress );
        jop->j = i->caddress - ( (int)fixup.jaddress + 1 );
    }
}

}

