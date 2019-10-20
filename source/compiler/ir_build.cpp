//
//  ir_build.cpp
//
//  Created by Edmund Kapusniak on 02/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "ir_build.h"

namespace kf
{

ir_build::ir_build( source* source )
    :   _source( source )
    ,   _block_index( IR_INVALID_INDEX )
{
}

ir_build::~ir_build()
{
}

std::unique_ptr< ir_function > ir_build::build( ast_function* function )
{
    // Set up for building.
    _f = std::make_unique< ir_function >();
    _f->ast = function;

    // Visit AST.
    ast_node_index node = { &_f->ast->nodes.back(), (unsigned)_f->ast->nodes.size() - 1 };
    visit( node );

    // Clean up.
    assert( _o.empty() );
    for ( size_t i = 0; i < GOTO_MAX; ++i )
    {
        assert( _goto_stacks[ i ].fixups.empty() );
        assert( _goto_stacks[ i ].index == 0 );
    }
    assert( _block_index == IR_INVALID_INDEX );
    assert( _def_stack.empty() );
    _defs.clear();

    // Done.
    return std::move( _f );
}

ir_operand ir_build::visit( ast_node_index node )
{
    switch ( node->kind )
    {
    case AST_NONE:
    {
        return { IR_O_NONE };
    }

    // -- ARITHMETIC --

    case AST_EXPR_LENGTH:
    case AST_EXPR_NEG:
    case AST_EXPR_POS:
    case AST_EXPR_BITNOT:
    {
        ast_node_index u = ast_child_node( _f->ast, node );
        _o.push_back( visit( u ) );
        return emit( node->sloc, (ir_opcode)node->kind, 1 );
    }

    case AST_EXPR_MUL:
    case AST_EXPR_DIV:
    case AST_EXPR_INTDIV:
    case AST_EXPR_MOD:
    case AST_EXPR_ADD:
    case AST_EXPR_SUB:
    case AST_EXPR_CONCAT:
    case AST_EXPR_LSHIFT:
    case AST_EXPR_RSHIFT:
    case AST_EXPR_ASHIFT:
    case AST_EXPR_BITAND:
    case AST_EXPR_BITXOR:
    case AST_EXPR_BITOR:
    {
        ast_node_index u = ast_child_node( _f->ast, node );
        ast_node_index v = ast_next_node( _f->ast, u );
        _o.push_back( visit( u ) );
        _o.push_back( visit( v ) );
        return emit( node->sloc, (ir_opcode)node->kind, 2 );
    }

    // -- CONSTANTS --

    case AST_EXPR_NULL:
    {
        _o.push_back( { IR_O_NULL } );
        return emit( node->sloc, IR_CONST, 1 );
    }

    case AST_EXPR_FALSE:
    {
        _o.push_back( { IR_O_FALSE } );
        return emit( node->sloc, IR_CONST, 1 );
    }

    case AST_EXPR_TRUE:
    {
        _o.push_back( { IR_O_TRUE } );
        return emit( node->sloc, IR_CONST, 1 );
    }

    case AST_EXPR_NUMBER:
    {
        _o.push_back( number_operand( node ) );
        return emit( node->sloc, IR_CONST, 1 );
    }

    case AST_EXPR_STRING:
    {
        _o.push_back( string_operand( node ) );
        return emit( node->sloc, IR_CONST, 1 );
    }

    // -- LOGICAL --

    case AST_EXPR_COMPARE:
    {
        /*
            a < b

                :0000   a
                :0001   b
                :0002   LT :0000, :0001

            a < b < c < d

                :0000   a
                :0001   b
                :0002   LT :0000, :0001
                :0003   B_AND :0002, @0005
                :0004   B_DEF :0003, :0002, @000B
                :0005   c
                :0006   LT :0001, :0005
                :0007   B_AND :0006, @0009
                :0008   B_DEF :0007, :0006, @000B
                :0009   d
                :000A   LT :0005, :0009
                :000B   B_PHI :0004, :0008, :000A
        */

        ast_node_index u = ast_child_node( _f->ast, node );
        ast_node_index op = ast_next_node( _f->ast, u );
        ast_node_index v = ast_next_node( _f->ast, op );

        unsigned ocount = 0;
        ir_operand last = visit( u );
        ir_operand comp = { IR_O_NONE };
        goto_scope goto_else = goto_open( node->sloc, GOTO_ELSE );
        goto_scope goto_endif = goto_open( node->sloc, GOTO_ENDIF );

        while ( true )
        {
            /*
                We apply the following transformations:

                    u > v becomes v < u
                    u >= v becomes v <= u
                    u is not v becomes not u is v

                I'm pretty sure that these hold even considering NaN.
            */

            _o.push_back( last );
            _o.push_back( last = visit( v ) );

            ir_opcode opcode = IR_NOP;
            switch ( op->kind )
            {
            case AST_OP_EQ: opcode = IR_EQ; break;
            case AST_OP_NE: opcode = IR_NE; break;
            case AST_OP_LT: opcode = IR_LT; break;
            case AST_OP_LE: opcode = IR_LE; break;
            case AST_OP_GT: opcode = IR_LT; std::swap( _o.back(), _o[ _o.size() - 2 ] ); break;
            case AST_OP_GE: opcode = IR_LE; std::swap( _o.back(), _o[ _o.size() - 2 ] ); break;
            case AST_OP_IS: opcode = IR_IS; break;
            case AST_OP_IS_NOT: opcode = IR_IS; break;
            default: break;
            }

            comp = emit( op->sloc, opcode, 2 );

            if ( op->kind == AST_OP_IS_NOT )
            {
                _o.push_back( comp );
                comp = emit( op->sloc, IR_NOT, 1 );
            }

            op = ast_next_node( _f->ast, v );
            if ( op.index >= node.index )
            {
                break;
            }

            _o.push_back( comp );
            ir_operand op_and = emit_jump( op->sloc, IR_B_AND, 1, GOTO_ELSE );

            _o.push_back( op_and );
            _o.push_back( comp );
            _o.push_back( emit_jump( op->sloc, IR_B_DEF, 2, GOTO_ENDIF ) );
            ocount += 1;

            goto_branch( goto_else );
            v = ast_next_node( _f->ast, op );
        }

        if ( ocount )
        {
            _o.push_back( comp );
            goto_branch( goto_endif );
            comp = emit( node->sloc, IR_B_PHI, ocount + 1 );
        }

        return comp;
    }

    case AST_OP_EQ:
    case AST_OP_NE:
    case AST_OP_LT:
    case AST_OP_LE:
    case AST_OP_GT:
    case AST_OP_GE:
    case AST_OP_IS:
    case AST_OP_IS_NOT:
    {
        assert( ! "unexpected OP node" );
        return { IR_O_NONE };
    }

    case AST_EXPR_NOT:
    {
        ast_node_index u = ast_child_node( _f->ast, node );
        _o.push_back( visit( u ) );
        return emit( node->sloc, IR_NOT, 1 );
    }

    case AST_EXPR_AND:
    {
        /*
            a and b

                :0000   a
                :0001   B_AND :0000, @0003
                :0002   B_DEF :0001, :0000, @0004
                :0003   b
                :0004   B_PHI :0002, :0003
        */

        ast_node_index u = ast_child_node( _f->ast, node );
        ast_node_index v = ast_next_node( _f->ast, u );

        ir_operand lhs = visit( u );
        goto_scope goto_else = goto_open( node->sloc, GOTO_ELSE );
        goto_scope goto_endif = goto_open( node->sloc, GOTO_ENDIF );

        _o.push_back( lhs );
        ir_operand op_and = emit_jump( node->sloc, IR_B_AND, 1, GOTO_ELSE );

        _o.push_back( op_and );
        _o.push_back( lhs );
        _o.push_back( emit_jump( node->sloc, IR_B_DEF, 2, GOTO_ENDIF ) );

        goto_branch( goto_else );
        _o.push_back( visit( v ) );

        goto_branch( goto_endif );
        return emit( node->sloc, IR_B_PHI, 2 );
    }

    case AST_EXPR_OR:
    {
        /*
            a or b

                :0000   a
                :0001   B_CUT :0000, @0003
                :0002   B_DEF :0001, :0000, @0004
                :0003   b
                :0004   B_PHI :0002, :0003
        */

        ast_node_index u = ast_child_node( _f->ast, node );
        ast_node_index v = ast_next_node( _f->ast, u );

        ir_operand lhs = visit( u );
        goto_scope goto_else = goto_open( node->sloc, GOTO_ELSE );
        goto_scope goto_endif = goto_open( node->sloc, GOTO_ENDIF );

        _o.push_back( lhs );
        ir_operand op_and = emit_jump( node->sloc, IR_B_CUT, 1, GOTO_ELSE );

        _o.push_back( op_and );
        _o.push_back( lhs );
        _o.push_back( emit_jump( node->sloc, IR_B_DEF, 2, GOTO_ENDIF ) );

        goto_branch( goto_else );
        _o.push_back( visit( v ) );

        goto_branch( goto_endif );
        return emit( node->sloc, IR_B_PHI, 2 );
    }

    case AST_EXPR_IF:
    {
        /*
            if x then y else z

                :0000   x
                :0001   B_CUT :0000, @0004
                :0002   y
                :0003   B_DEF :0001, :0002, @0005
                :0004   z
                :0005   B_PHI :0003, :0004

            if x then y elif p then q else z

                :0000   x
                :0001   B_CUT :0000, @0004
                :0002   y
                :0003   B_DEF :0001, :0008, @0009
                :0004   p
                :0005   B_CUT :0004, @0008
                :0006   q
                :0007   B_DEF :0005, :0006, @0009
                :0008   z
                :0009   B_PHI :0003, :0007, :0008
        */

        ast_node_index kw = node;
        ast_node_index test = ast_child_node( _f->ast, kw );
        ast_node_index expr = ast_next_node( _f->ast, test );
        ast_node_index next = ast_next_node( _f->ast, expr );

        size_t ocount = 0;
        _o.push_back( visit( test ) );
        goto_scope goto_else = goto_open( node->sloc, GOTO_ELSE );
        goto_scope goto_endif = goto_open( node->sloc, GOTO_ENDIF );
        while ( true )
        {
            ir_operand op_cut = emit_jump( kw->sloc, IR_B_CUT, 1, GOTO_ELSE );

            _o.push_back( op_cut );
            _o.push_back( visit( expr ) );
            _o.push_back( emit_jump( kw->sloc, IR_B_DEF, 2, GOTO_ENDIF ) );
            ocount += 1;

            goto_branch( goto_else );

            if ( next->kind != AST_EXPR_ELIF )
            {
                break;
            }

            kw = next;
            test = ast_child_node( _f->ast, kw );
            expr = ast_next_node( _f->ast, test );
            next = ast_next_node( _f->ast, kw );

            _o.push_back( visit( test ) );
        }

        _o.push_back( visit( next ) );
        goto_branch( goto_endif );
        return emit( node->sloc, IR_B_PHI, ocount + 1 );
    }

    case AST_EXPR_ELIF:
    {
        assert( ! "unexpected ELIF node" );
        return { IR_O_NONE };
    }

    // -- EXPRESSIONS --

    case AST_EXPR_KEY:
    {
        ast_node_index u = ast_child_node( _f->ast, node );
        _o.push_back( visit( u ) );
        _o.push_back( selector_operand( node ) );
        return emit( node->sloc, IR_GET_KEY, 2 );
    }

    case AST_EXPR_INDEX:
    {
        ast_node_index u = ast_child_node( _f->ast, node );
        ast_node_index v = ast_next_node( _f->ast, u );
        _o.push_back( visit( u ) );
        _o.push_back( visit( v ) );
        return emit( node->sloc, IR_GET_INDEX, 2 );
    }

    case AST_EXPR_CALL:
    {
        return call_op( node, IR_CALL );
    }

    case AST_EXPR_UNPACK:
    {
        assert( ! "unexpected EXPR_UNPACK node" );
        return { IR_O_NONE };
    }

    case AST_EXPR_ARRAY:
    {
        _o.push_back( { IR_O_IMMEDIATE, 0 } );
        ir_operand array = emit( node->sloc, IR_NEW_ARRAY, 1 );

        unsigned elcount = 0;
        for ( ast_node_index el = ast_child_node( _f->ast, node ); el.index < node.index; el = ast_next_node( _f->ast, el ) )
        {
            _o.push_back( array );
            if ( el->kind != AST_EXPR_UNPACK )
            {
                _o.push_back( visit( el ) );
                emit( node->sloc, IR_APPEND, 2 );
                elcount += 1;
            }
            else
            {
                _o.push_back( expr_unpack( el, IR_UNPACK_ALL ) );
                emit( node->sloc, IR_EXTEND, 2 );
                elcount = ( elcount + 31 ) & ~(unsigned)31;
            }
        }

        ir_operand* operand = &_f->operands[ _f->ops[ array.index ].oindex ];
        operand->index = elcount;

        return array;
    }

    case AST_EXPR_TABLE:
    {
        _o.push_back( { IR_O_IMMEDIATE, 0 } );
        ir_operand table = emit( node->sloc, IR_NEW_TABLE, 0 );

        unsigned kvcount = 0;
        for ( ast_node_index kv = ast_child_node( _f->ast, node ); kv.index < node.index; kv = ast_next_node( _f->ast, kv ) )
        {
            assert( kv->kind == AST_TABLE_KEY );
            ast_node_index k = ast_child_node( _f->ast, kv );
            ast_node_index v = ast_next_node( _f->ast, kv );
            _o.push_back( table );
            _o.push_back( visit( k ) );
            _o.push_back( visit( v ) );
            emit( node->sloc, IR_SET_INDEX, 3 );
            kvcount += 1;
        }

        ir_operand* operand = &_f->operands[ _f->ops[ table.index ].oindex ];
        operand->index = kvcount;

        return table;
    }

    case AST_TABLE_KEY:
    {
        assert( ! "unexpected TABLE_KEY node" );
        return { IR_O_NONE };
    }

    // -- YIELD --

    case AST_EXPR_YIELD:
    {
        // yield a, b, c ...
        return call_op( node, IR_YIELD );
    }

    case AST_EXPR_YIELD_FOR:
    {
        // yield for a()
        return call_op( node, IR_YCALL );
    }

    // -- DECLARATION AND ASSIGNMENT --

    case AST_DECL_VAR:
    {
        ast_node_index names = ast_child_node( _f->ast, node );
        ast_node_index rvals = ast_next_node( _f->ast, names );

        // Might have a list of names.
        ast_node_index name = names;
        ast_node_index name_done = ast_next_node( _f->ast, name );
        if ( names->kind == AST_NAME_LIST )
        {
            name = ast_child_node( _f->ast, names );
            name_done = names;
        }

        // Count number of names.
        unsigned rvcount = 0;
        for ( ast_node_index c = name; c.index < name_done.index; c = ast_next_node( _f->ast, c ) )
        {
            rvcount += 1;
        }

        if ( rvals.index < node.index )
        {
            // Evaluate rvals.
            unsigned rvindex = rval_list( rvals, rvcount );

            // Assign.
            unsigned rv = rvindex;
            for ( ; name.index < name_done.index; name = ast_next_node( _f->ast, name ), ++rv )
            {
                assert( name->kind == AST_LOCAL_DECL );
                def( name->sloc, name->leaf_index().index, _o.at( rv ) );
            }

            _o.resize( rvindex );
        }
        else
        {
            // Assign null.
            for ( ; name.index < name_done.index; name = ast_next_node( _f->ast, name ) )
            {
                assert( name->kind == AST_LOCAL_DECL );
                _o.push_back( { IR_O_NULL } );
                def( name->sloc, name->leaf_index().index, emit( name->sloc, IR_CONST, 1 ) );
            }
        }

        return { IR_O_NONE };
    }

    case AST_DECL_DEF:
    {
        ast_node_index qname = ast_child_node( _f->ast, node );
        ast_node_index value = ast_next_node( _f->ast, qname );

        ir_operand object = visit( value );
        if ( qname->kind == AST_LOCAL_DECL )
        {
            def( node->sloc, qname->leaf_index().index, object );
        }
        else
        {
            assert( qname->kind == AST_EXPR_KEY );
            assign( qname, object );
        }

        return { IR_O_NONE };
    }

    case AST_RVAL_ASSIGN:
    case AST_RVAL_OP_ASSIGN:
    {
        // Assignments are themselves rvals, use the same machinery.
        rval_list( node, 0 );
        return { IR_O_NONE };
    }

    case AST_NAME_LIST:
    case AST_LVAL_LIST:
    case AST_RVAL_LIST:
    {
        assert( ! "unexpected LIST node" );
        return { IR_O_NONE };
    }

    // -- SCOPE --

    case AST_FUNCTION:
    {
        ast_node_index parameters = ast_child_node( _f->ast, node );
        ast_node_index block = ast_next_node( _f->ast, parameters );

        unsigned pindex = _o.size();

        if ( _f->ast->implicit_self )
        {
            const ast_local& self = _f->ast->locals[ 0 ];
            assert( self.is_parameter );
            assert( self.is_self );
            _o.push_back( { IR_O_LOCAL, 0 } );
            _o.push_back( emit( node->sloc, IR_PARAM, 1 ) );
        }

        for ( ast_node_index param = ast_child_node( _f->ast, parameters ); param.index < parameters.index; param = ast_next_node( _f->ast, param ) )
        {
            if ( param->kind == AST_VARARG_PARAM )
            {
                continue;
            }

            assert( param->kind == AST_LOCAL_DECL );
            unsigned local_index = param->leaf_index().index;
            _o.push_back( { IR_O_LOCAL, local_index } );
            _o.push_back( emit( param->sloc, IR_PARAM, 1 ) );
        }

        block_varenv( block );

        for ( unsigned local_index = 0; local_index < _f->ast->parameter_count; ++local_index )
        {
            if ( _f->ast->locals[ local_index ].is_vararg )
            {
                continue;
            }

            ir_operand param = _o.at( pindex + local_index );
            srcloc sloc = _f->ops[ param.index ].sloc;
            def( sloc, local_index, param );
        }

        _o.resize( pindex );

        visit_children( block );
        end_block( emit( node->sloc, IR_JUMP_RETURN, 0 ) );
        return { IR_O_NONE };
    }

    case AST_PARAMETERS:
    {
        assert( ! "unexpected PARAMETERS node" );
        return { IR_O_NONE };
    }

    case AST_VARARG_PARAM:
    {
        assert( ! "unexpected VARAG_PARAM node" );
        return { IR_O_NONE };
    }

    case AST_BLOCK:
    {
        block_varenv( node );
        visit_children( node );
        return { IR_O_NONE };
    }

    // -- STATEMENTS --

    case AST_STMT_IF:
    {
        ast_node_index expr = ast_child_node( _f->ast, node );
        ast_node_index body = ast_next_node( _f->ast, expr );
        ast_node_index next = ast_next_node( _f->ast, body );

        _o.push_back( visit( expr ) );
        goto_scope goto_else = goto_open( node->sloc, GOTO_ELSE );
        goto_scope goto_endif = goto_open( node->sloc, GOTO_ENDIF );

        while ( true )
        {
            // Check if condition.
            goto_scope goto_next = goto_open( node->sloc, GOTO_ENDIF );
            end_block( emit_test( node->sloc, IR_JUMP_TEST, 1, GOTO_ENDIF, GOTO_ELSE ) );
            goto_block( goto_next );

            // Output body.
            visit( body );
            if ( _block_index != IR_INVALID_INDEX )
            {
                end_block( emit_jump( node->sloc, IR_JUMP, 0, GOTO_ENDIF ) );
            }

            if ( next.index < node.index && next->kind == AST_STMT_ELIF )
            {
                expr = ast_child_node( _f->ast, next );
                body = ast_next_node( _f->ast, expr );
                next = ast_next_node( _f->ast, next );

                goto_block( goto_else );
                _o.push_back( visit( expr ) );
                continue;
            }
            else
            {
                break;
            }
        }

        goto_block( goto_else );
        if ( next.index < node.index )
        {
            // Else clause.
            assert( next->kind == AST_BLOCK );
            visit( next );
            if ( _block_index != IR_INVALID_INDEX )
            {
                end_block( emit_jump( next->sloc, IR_JUMP, 0, GOTO_ENDIF ) );
            }
        }

        goto_block( goto_endif );
        return { IR_O_NONE };
    }

    case AST_STMT_ELIF:
    {
        assert( ! "unexpected ELIF node" );
        return { IR_O_NONE };
    }

    case AST_STMT_FOR_STEP:
    {
        ast_node_index name = ast_child_node( _f->ast, node );
        ast_node_index start = ast_next_node( _f->ast, name );
        ast_node_index limit = ast_next_node( _f->ast, start );
        ast_node_index step = ast_next_node( _f->ast, limit );
        ast_node_index body = ast_next_node( _f->ast, step );

        unsigned local_index = node->leaf_index().index;

        // Evaluate start : limit : step
        _o.push_back( visit( start ) );
        _o.push_back( visit( limit ) );
        _o.push_back( visit( step ) );
        goto_scope goto_else = goto_open( node->sloc, GOTO_ELSE );
        ir_operand sgen = emit_jump( node->sloc, IR_JUMP_FOR_SGEN, 3, GOTO_ELSE );
        def( node->sloc, local_index, sgen );
        end_block( sgen );
        goto_block( goto_else );

        // Start of loop.
        ir_block_index loop = new_loop( new_block( node->sloc, IR_BLOCK_UNSEALED ) );

        // Mark break/continue stacks.
        goto_scope goto_continue = goto_open( node->sloc, GOTO_CONTINUE );
        goto_scope goto_break = goto_open( node->sloc, GOTO_BREAK );

        // For loop.
        goto_scope goto_next = goto_open( node->sloc, GOTO_ENDIF );
        _o.push_back( use( node->sloc, local_index ) );
        ir_operand test = emit_test( node->sloc, IR_JUMP_FOR_STEP, 1, GOTO_ENDIF, GOTO_BREAK );
        def( node->sloc, local_index, test );
        end_block( test );
        goto_block( goto_next );

        // Get index at head of loop.
        assert( name->kind == AST_LOCAL_DECL );
        def( name->sloc, name->leaf_index().index, emit( node->sloc, IR_FOR_STEP_INDEX, 0 ) );

        // Visit the body of the loop.
        visit( body );
        end_block( emit_jump( node->sloc, IR_JUMP, 0, GOTO_CONTINUE ) );
        end_loop( loop, goto_continue );

        // Break to after loop.
        goto_block( goto_break );
        return { IR_O_NONE };
    }

    case AST_STMT_FOR_EACH:
    {
        ast_node_index names = ast_child_node( _f->ast, node );
        ast_node_index expr = ast_next_node( _f->ast, names );
        ast_node_index body = ast_next_node( _f->ast, expr );

        unsigned local_index = node->leaf_index().index;

        // Evaluate generator expression.
        _o.push_back( visit( expr ) );
        goto_scope goto_else = goto_open( node->sloc, GOTO_ELSE );
        ir_operand egen = emit_jump( node->sloc, IR_JUMP_FOR_EGEN, 1, GOTO_ELSE );
        def( node->sloc, local_index, egen );
        end_block( egen );
        goto_block( goto_else );

        // Start of loop.
        ir_block_index loop = new_loop( new_block( node->sloc, IR_BLOCK_UNSEALED ) );

        // Mark break/continue stacks.
        goto_scope goto_continue = goto_open( node->sloc, GOTO_CONTINUE );
        goto_scope goto_break = goto_open( node->sloc, GOTO_BREAK );

        // For loop.
        goto_scope goto_next = goto_open( node->sloc, GOTO_ENDIF );
        _o.push_back( use( node->sloc, local_index ) );
        ir_operand test = emit_test( node->sloc, IR_JUMP_FOR_EACH, 1, GOTO_ENDIF, GOTO_BREAK );
        def( node->sloc, local_index, test );
        end_block( test );
        goto_block( goto_next );

        // Assign generated items.
        ir_operand items = emit( node->sloc, IR_FOR_EACH_ITEMS, 0 );
        if ( names->kind == AST_NAME_LIST )
        {
            ast_node_index name = ast_child_node( _f->ast, names );
            ast_node_index name_done = names;

            ir_op* op = &_f->ops[ items.index ];
            unsigned unpack = 0;

            for ( ; name.index < name_done.index; name = ast_next_node( _f->ast, name ) )
            {
                assert( name->kind == AST_LOCAL_DECL );
                _o.push_back( items );
                _o.push_back( { IR_O_SELECT, unpack++ } );
                def( name->sloc, name->leaf_index().index, emit( name->sloc, IR_SELECT, 2 ) );
            }

            assert( op->local() == IR_INVALID_LOCAL );
            op->set_unpack( unpack );
        }
        else
        {
            ast_node_index name = names;
            assert( name->kind == AST_LOCAL_DECL );
            def( name->sloc, name->leaf_index().index, items );
        }

        // Visit the body of the loop.
        visit( body );
        end_block( emit_jump( node->sloc, IR_JUMP, 0, GOTO_CONTINUE ) );
        end_loop( loop, goto_continue );

        // Break to after loop.
        goto_block( goto_break );
        return { IR_O_NONE };
    }

    case AST_STMT_WHILE:
    {
        ast_node_index expr = ast_child_node( _f->ast, node );
        ast_node_index body = ast_next_node( _f->ast, expr );

        // Open loop header.
        ir_block_index loop = new_loop( new_block( node->sloc, IR_BLOCK_UNSEALED ) );

        // Mark break/continue stacks.
        goto_scope goto_continue = goto_open( node->sloc, GOTO_CONTINUE );
        goto_scope goto_break = goto_open( node->sloc, GOTO_BREAK );

        // Check condition.
        _o.push_back( visit( expr ) );
        goto_scope goto_next = goto_open( node->sloc, GOTO_ENDIF );
        end_block( emit_test( node->sloc, IR_JUMP_TEST, 1, GOTO_ENDIF, GOTO_BREAK ) );
        goto_block( goto_next );

        // Body of loop.
        visit( body );
        end_block( emit_jump( node->sloc, IR_JUMP, 0, GOTO_CONTINUE ) );
        end_loop( loop, goto_continue );

        // Break to after loop.
        goto_block( goto_break );
        return { IR_O_NONE };
    }

    case AST_STMT_REPEAT:
    {
        ast_node_index body = ast_child_node( _f->ast, node );
        ast_node_index expr = ast_next_node( _f->ast, body );

        // Open loop header.
        ir_block_index loop = new_loop( new_block( node->sloc, IR_BLOCK_UNSEALED ) );

        // Mark break/continue stacks.
        goto_scope goto_continue = goto_open( node->sloc, GOTO_CONTINUE );
        goto_scope goto_break = goto_open( node->sloc, GOTO_BREAK );

        // Body of loop.
        visit( body );

        // Continue to condition.
        if ( goto_continue.index < _goto_stacks[ GOTO_CONTINUE ].fixups.size() )
        {
            end_block( emit_jump( node->sloc, IR_JUMP, 0, GOTO_CONTINUE ) );
            goto_block( goto_continue );
        }

        // Check condition and loop.
        _o.push_back( visit( expr ) );
        goto_scope goto_loop = goto_open( node->sloc, GOTO_CONTINUE );
        end_block( emit_test( node->sloc, IR_JUMP_TEST, 1, GOTO_BREAK, GOTO_CONTINUE ) );
        end_loop( loop, goto_loop );

        // Break to after loop.
        goto_block( goto_break );
        return { IR_O_NONE };
    }

    case AST_STMT_BREAK:
    {
        end_block( emit_jump( node->sloc, IR_JUMP, 0, GOTO_BREAK ) );
        return { IR_O_NONE };
    }

    case AST_STMT_CONTINUE:
    {
        end_block( emit_jump( node->sloc, IR_JUMP, 0, GOTO_CONTINUE ) );
        return { IR_O_NONE };
    }

    case AST_STMT_RETURN:
    {
        if ( ast_child_node( _f->ast, node ).index < node.index )
        {
            end_block( call_op( node, IR_JUMP_RETURN ) );
        }
        else
        {
            end_block( emit( node->sloc, IR_JUMP_RETURN, 0 ) );
        }
        return { IR_O_NONE };
    }

    case AST_STMT_THROW:
    {
        _o.push_back( visit( ast_child_node( _f->ast, node ) ) );
        end_block( emit( node->sloc, IR_JUMP_THROW, 1 ) );
        return { IR_O_NONE };
    }

    case AST_DEF_FUNCTION:
    {
        ast_function* function = node->leaf_function().function;

        unsigned ocount = 1;
        _o.push_back( { IR_O_FUNCTION, function->index } );

        for ( unsigned i = 0; i < function->outenvs.size(); ++i )
        {
            const ast_outenv& outenv = function->outenvs[ i ];
            if ( outenv.outer_outenv )
            {
                _o.push_back( { IR_O_OUTENV, outenv.outer_index } );
            }
            else
            {
                _o.push_back( use( node->sloc, outenv.outer_index ) );
            }
            ocount += 1;
        }

        return emit( node->sloc, IR_NEW_FUNCTION, ocount );
    }

    case AST_DEF_OBJECT:
    {
        ast_node_index child = ast_child_node( _f->ast, node );

        // Get prototype.
        if ( child.index < node.index && child->kind == AST_OBJECT_PROTOTYPE )
        {
            ast_node_index proto_expr = ast_child_node( _f->ast, child );
            _o.push_back( visit( proto_expr ) );
            child = ast_next_node( _f->ast, child );
        }
        else
        {
            _o.push_back( { IR_O_NULL } );
            _o.push_back( emit( node->sloc, IR_CONST, 1 ) );
        }

        // Create object.
        ir_operand object = emit( node->sloc, IR_NEW_OBJECT, 1 );

        // Assign keys.
        for ( ; child.index < node.index; child = ast_next_node( _f->ast, child ) )
        {
            assert( child->kind == AST_DECL_DEF || child->kind == AST_OBJECT_KEY );
            ast_node_index name = ast_child_node( _f->ast, child );
            ast_node_index value = ast_next_node( _f->ast, name );

            assert( name->kind == AST_OBJKEY_DECL );
            _o.push_back( object );
            _o.push_back( selector_operand( name ) );
            _o.push_back( visit( value ) );
            emit( child->sloc, IR_SET_KEY, 3 );
        }

        return object;
    }

    case AST_OBJECT_PROTOTYPE:
    {
        assert( ! "unexpected OBJECT_PROTOTYPE node" );
        return { IR_O_NONE };
    }

    case AST_OBJECT_KEY:
    {
        assert( ! "unexpected OBJECT_KEY node" );
        return { IR_O_NONE };
    }

    case AST_NAME:
    {
        assert( ! "unexpected NAME node" );
        return { IR_O_NONE };
    }

    case AST_OBJKEY_DECL:
    {
        assert( ! "unexpected OBJKEY_DECL node" );
        return { IR_O_NONE };
    }

    case AST_LOCAL_DECL:
    {
        assert( ! "unexpected LOCAL_DECL node" );
        return { IR_O_NONE };
    }

    case AST_LOCAL_NAME:
    case AST_SUPER_NAME:
    {
        unsigned local_index = node->leaf_index().index;
        ir_operand value = use( node->sloc, local_index );
        if ( node->kind == AST_SUPER_NAME )
        {
            _o.push_back( value );
            value = emit( node->sloc, IR_SUPER, 1 );
        }
        return value;
    }

    case AST_OUTENV_NAME:
    {
        const ast_leaf_outenv& outenv = node->leaf_outenv();
        _o.push_back( { IR_O_OUTENV, outenv.outenv_index } );
        _o.push_back( { IR_O_ENVSLOT, outenv.outenv_slot } );
        return emit( node->sloc, IR_GET_ENV, 2 );
    }

    case AST_GLOBAL_NAME:
    {
        _o.push_back( selector_operand( node ) );
        return emit( node->sloc, IR_GET_GLOBAL, 1 );
    }

    }

    return { IR_O_NONE };
}

void ir_build::block_varenv( ast_node_index node )
{
    unsigned varenv_index = node->leaf_index().index;
    if ( varenv_index != AST_INVALID_INDEX )
    {
        const ast_local& varenv = _f->ast->locals[ varenv_index ];
        _o.push_back( { IR_O_IMMEDIATE, varenv.varenv_slot } );
        def( node->sloc, varenv_index, emit( node->sloc, IR_NEW_ENV, 1 ) );
    }
}

void ir_build::visit_children( ast_node_index node )
{
    for ( ast_node_index child = ast_child_node( _f->ast, node ); child.index < node.index; child = ast_next_node( _f->ast, child ) )
    {
        visit( child );
    }
}

unsigned ir_build::rval_list( ast_node_index node, unsigned unpack )
{
    // Push rvcount rvals onto the evaluation stack, and return the index of
    // the first rval on the evaluation stack.
    unsigned rvindex = _o.size();
    unsigned rvcount = 0;

    if ( node->kind == AST_RVAL_ASSIGN )
    {
        // a, b, c = rvals
        ast_node_index lvals = ast_child_node( _f->ast, node );
        ast_node_index rvals = ast_next_node( _f->ast, lvals );

        // Might have a list of rvals.
        ast_node_index lval = lvals;
        ast_node_index lval_done = ast_next_node( _f->ast, lval );
        if ( lvals->kind == AST_LVAL_LIST )
        {
            lval = ast_child_node( _f->ast, lvals );
            lval_done = lvals;
        }

        // Count number of lvals.
        unsigned inner_unpack = 0;
        for ( ast_node_index c = lval; c.index < lval_done.index; c = ast_next_node( _f->ast, c ) )
        {
            inner_unpack += 1;
        }

        // Push rvals onto stack.
        unsigned inner_rvindex = rval_list( rvals, inner_unpack );
        assert( inner_rvindex == rvindex );

        if ( inner_unpack == 1 )
        {
            // Single assignment is simpler.
            _o[ inner_rvindex ] = assign( lval, _o.at( inner_rvindex ) );
        }
        else
        {
            // List of assignments.  They can interfere with each other.
            unsigned rv = assign_list( lval, lval_done, inner_rvindex, unpack );
            assert( rv == inner_rvindex + inner_unpack );
        }

        // Leave rvals on the stack, as our contribution.
        rvcount += inner_unpack;
    }
    else if ( node->kind == AST_RVAL_OP_ASSIGN )
    {
        // a *= b
        ast_node_index lval = ast_child_node( _f->ast, node );
        ast_node_index op = ast_next_node( _f->ast, lval );
        ast_node_index rval = ast_next_node( _f->ast, op );

        // Evaluate left hand side, but remember operands.
        ir_operand uoperand = { IR_O_NONE };
        ir_operand voperand = { IR_O_NONE };
        if ( lval->kind == AST_EXPR_KEY )
        {
            uoperand = visit( ast_child_node( _f->ast, lval ) );
            voperand = selector_operand( lval );
            _o.push_back( uoperand );
            _o.push_back( voperand );
            _o.push_back( emit( lval->sloc, IR_GET_KEY, 2 ) );
        }
        else if ( lval->kind == AST_EXPR_INDEX )
        {
            ast_node_index u = ast_child_node( _f->ast, lval );
            ast_node_index v = ast_child_node( _f->ast, rval );
            uoperand = visit( u ); _o.push_back( uoperand );
            voperand = visit( v ); _o.push_back( voperand );
            _o.push_back( emit( lval->sloc, IR_GET_INDEX, 2 ) );
        }
        else
        {
            _o.push_back( visit( lval ) );
        }

        // Evaluate rval (which is really an rval, so can yield etc).
        rval_list( rval, 1 );

        // Perform operation.
        ir_operand result = emit( op->sloc, (ir_opcode)op->kind, 2 );
        _o.push_back( result );

        // Perform assignment, leaving result on the stack.
        if ( lval->kind == AST_EXPR_KEY )
        {
            _o.push_back( uoperand );
            _o.push_back( voperand );
            _o.push_back( result );
            emit( lval->sloc, IR_SET_KEY, 3 );
        }
        else if ( lval->kind == AST_EXPR_INDEX )
        {
            _o.push_back( uoperand );
            _o.push_back( voperand );
            _o.push_back( result );
            emit( lval->sloc, IR_SET_INDEX, 3 );
        }
        else
        {
            _o.back() = assign( lval, _o.back() );
        }
    }
    else if ( node->kind == AST_RVAL_LIST )
    {
        // a, b, c ...
        for ( ast_node_index rval = ast_child_node( _f->ast, node ); rval.index < node.index; rval = ast_next_node( _f->ast, rval ) )
        {
            unsigned inner_unpack = 1;
            if ( rval->kind == AST_EXPR_UNPACK )
                inner_unpack = unpack - std::min( rvcount, unpack );
            rval_list( rval, inner_unpack );
            rvcount += inner_unpack;
        }
    }
    else if ( node->kind == AST_EXPR_UNPACK )
    {
        // a ...
        ir_operand rval = expr_unpack( node, unpack );
        if ( unpack == 1 )
        {
            _o.push_back( rval );
            rvcount += 1;
        }
        else
        {
            for ( ; rvcount < unpack; ++rvcount )
            {
                _o.push_back( rval );
                _o.push_back( { IR_O_SELECT, rvcount } );
                _o.push_back( emit( node->sloc, IR_SELECT, 2 ) );
            }
        }
    }
    else
    {
        _o.push_back( visit( node ) );
        rvcount += 1;
    }

    if ( unpack == 0 )
    {
        _o.resize( rvindex );
        rvcount = 0;
    }

    if ( rvcount != unpack )
    {
        _source->error( node->sloc, "internal: mismatched rval count %d, expected %d", rvcount, unpack );
        for ( ; rvcount < unpack; rvcount += 1 )
        {
            _o.push_back( { IR_O_NONE } );
        }
        _o.resize( rvindex + unpack );
    }

    return rvindex;
}

unsigned ir_build::assign_list( ast_node_index lval_init, ast_node_index lval_done, unsigned rvindex, unsigned unpack )
{
    /*
        Assigning a list of values involves emitting explicit MOV instructions,
        as simply defining at the op where the result is calculated might
        cause the new definition of a local to overlap with uses of an old one.

        Additionally, before a local is defined, if any uses of it remain on
        the rval stack, then the current value must be preserved and the rvals
        replaced.

        Hopefully some or all of the MOVs can be elided by register allocation.
    */
    unsigned rv = rvindex;
    for ( ast_node_index lval = lval_init; lval.index < lval_done.index; lval = ast_next_node( _f->ast, lval ) )
    {
        ir_operand rval = _o.at( rv );
        if ( lval->kind == AST_LOCAL_NAME )
        {
            unsigned local_index = lval->leaf_index().index;

            // Check rval stack for uses of lval.
            ir_operand mov = { IR_O_NONE };
            for ( unsigned j = 0; j < _o.size(); ++j )
            {
                if ( _o[ j ].kind != IR_O_OP )
                    continue;
                if ( _f->ops[ _o[ j ].index ].local() != local_index )
                    continue;

                // Preserve current value of local.
                if ( mov.kind == IR_O_NONE )
                {
                    _o.push_back( use( lval->sloc, local_index ) );
                    mov = emit( lval->sloc, IR_MOV, 1 );
                }

                // Replace rval with preserved value.
                _o[ j ] = mov;
            }

            // Define using MOV.
            _o.push_back( rval );
            rval = emit( lval->sloc, IR_MOV, 1 );
        }

        // Just assign.
        _o[ rv++ ] = assign( lval, rval );
    }

    assert( rv == _o.size() );
    if ( unpack == 0 )
    {
        return rv;
    }

    /*
        If this assignment is itself an rval, we have to re-evaluate the lvals,
        otherwise expressions like p, q = a, a = 3, 4 give a different result
        from a, a = 3, 4; p, q = a, a.
    */
    _o.resize( rvindex );
    for ( ast_node_index lval = lval_init; lval.index < lval_done.index; lval = ast_next_node( _f->ast, lval ) )
    {
        _o.push_back( visit( lval ) );
    }

    assert( rv == _o.size() );
    return rv;
}

ir_operand ir_build::assign( ast_node_index lval, ir_operand rval )
{
    if ( lval->kind == AST_LOCAL_NAME )
    {
        unsigned local_index = lval->leaf_index().index;
        return def( lval->sloc, local_index, rval );
    }
    else if ( lval->kind == AST_OUTENV_NAME )
    {
        const ast_leaf_outenv& outenv = lval->leaf_outenv();
        _o.push_back( { IR_O_OUTENV, outenv.outenv_index } );
        _o.push_back( { IR_O_ENVSLOT, outenv.outenv_slot } );
        _o.push_back( rval );
        emit( lval->sloc, IR_SET_ENV, 3 );
        return rval;
    }
    else if ( lval->kind == AST_EXPR_KEY )
    {
        _o.push_back( visit( ast_child_node( _f->ast, lval ) ) );
        _o.push_back( selector_operand( lval ) );
        _o.push_back( rval );
        emit( lval->sloc, IR_SET_KEY, 3 );
        return rval;
    }
    else if ( lval->kind == AST_EXPR_INDEX )
    {
        ast_node_index u = ast_child_node( _f->ast, lval );
        ast_node_index v = ast_next_node( _f->ast, u );
        _o.push_back( visit( u ) );
        _o.push_back( visit( v ) );
        _o.push_back( rval );
        emit( lval->sloc, IR_SET_INDEX, 3 );
        return rval;
    }
    else
    {
        _source->error( lval->sloc, "internal: lhs is not assignable" );
        return rval;
    }
}

ir_operand ir_build::expr_unpack( ast_node_index node, unsigned unpack )
{
    assert( node->kind == AST_EXPR_UNPACK );

    // Evaluate expression we want to unpack.
    ir_operand operand = { IR_O_NONE };
    ast_node_index u = ast_child_node( _f->ast, node );
    if ( u->kind == AST_LOCAL_NAME && _f->ast->locals[ u->leaf_index().index ].is_vararg )
    {
        // args ...
        operand = emit( node->sloc, IR_VARARG, 0 );
    }
    else if ( u->kind == AST_EXPR_CALL )
    {
        // a() ...
        operand = call_op( u, IR_CALL );
    }
    else if ( u->kind == AST_EXPR_YIELD_FOR )
    {
        // yield a() ...
        operand = call_op( u, IR_YCALL );
    }
    else if ( u->kind == AST_EXPR_YIELD )
    {
        // yield ... a, b, c
        operand = call_op( u, IR_YIELD );
    }
    else
    {
        // a ...
        _o.push_back( visit( u ) );
        operand = emit( node->sloc, IR_UNPACK, 1 );
    }

    // Actually ask it to unpack.
    assert( operand.kind == IR_O_OP );
    ir_op* op = &_f->ops[ operand.index ];
    assert( op->opcode == IR_VARARG || op->opcode == IR_CALL || op->opcode == IR_YCALL || op->opcode == IR_YIELD || op->opcode == IR_UNPACK );
    assert( op->local() == IR_INVALID_LOCAL );
    op->set_unpack( unpack );

    // Return op that unpacks.
    return operand;
}

ir_operand ir_build::call_op( ast_node_index node, ir_opcode opcode )
{
    unsigned ocount = 0;
    ast_node_index arg = ast_child_node( _f->ast, node );

    if ( opcode == IR_CALL || opcode == IR_YCALL )
    {
        // Pass self parameter to method calls.
        if ( arg->kind == AST_EXPR_KEY )
        {
            ir_operand self = visit( ast_child_node( _f->ast, arg ) );
            _o.push_back( self );
            _o.push_back( selector_operand( arg ) );
            _o.push_back( emit( arg->sloc, IR_GET_KEY, 2 ) );
            _o.push_back( self );
            ocount += 2;
        }
        else
        {
            _o.push_back( visit( arg ) );
            ocount += 1;
        }

        arg = ast_next_node( _f->ast, arg );
    }

    for ( ; arg.index < node.index; arg = ast_next_node( _f->ast, arg ) )
    {
        if ( arg->kind != AST_EXPR_UNPACK )
            _o.push_back( visit( arg ) );
        else
            _o.push_back( expr_unpack( arg, IR_UNPACK_ALL ) );
        ocount += 1;
    }

    ir_operand call = emit( node->sloc, opcode, ocount );
    return call;
}

ir_operand ir_build::number_operand( ast_node_index node )
{
    return { IR_O_NUMBER, _f->constants.append( ir_constant( node->leaf_number().n ) ) };
}

ir_operand ir_build::string_operand( ast_node_index node )
{
    return { IR_O_STRING, _f->constants.append( ir_constant( node->leaf_string().text, node->leaf_string().size ) ) };
}

ir_operand ir_build::selector_operand( ast_node_index node )
{
    return { IR_O_SELECTOR, _f->selectors.append( { node->leaf_string().text, node->leaf_string().size } ) };
}

ir_operand ir_build::emit( srcloc sloc, ir_opcode opcode, unsigned ocount )
{
    if ( _block_index == IR_INVALID_INDEX )
    {
        new_block( sloc, IR_BLOCK_BASIC );
    }

    ir_op op;
    op.opcode = opcode;
    op.ocount = ocount;
    op.oindex = ocount ? _f->operands.size() : IR_INVALID_INDEX;
    op.sloc = sloc;

    unsigned op_index = _f->ops.append( op );

    assert( ocount <= _o.size() );
    unsigned oindex = _o.size() - ocount;
    for ( unsigned i = 0; i < ocount; ++i )
    {
        if ( _f->operands.size() >= IR_INVALID_INDEX )
            throw std::out_of_range( "too many operands" );
        _f->operands.append( _o[ oindex + i ] );
    }
    _o.resize( oindex );

    return { IR_O_OP, op_index };
}

ir_build::goto_scope ir_build::goto_open( srcloc sloc, goto_kind kind )
{
    if ( _block_index == IR_INVALID_INDEX )
    {
        new_block( sloc, IR_BLOCK_BASIC );
    }

    assert( kind < GOTO_MAX );
    unsigned index = _goto_stacks[ kind ].fixups.size();
    return { kind, index };
}

void ir_build::goto_branch( goto_scope scope )
{
    unsigned label = _f->ops.size();
    assert( scope.kind < GOTO_MAX );
    goto_stack& stack = _goto_stacks[ scope.kind ];

    for ( unsigned i = scope.index; i < stack.fixups.size(); ++i )
    {
        goto_fixup fixup = stack.fixups[ i ];
        assert( fixup.block_index == _block_index );
        ir_operand* operand = &_f->operands[ fixup.operand_index ];
        assert( operand->kind == IR_O_JUMP );
        operand->index = label;
    }

    assert( scope.index <= stack.fixups.size() );
    stack.fixups.resize( scope.index );
    stack.index = scope.index;
}

void ir_build::goto_block( goto_scope scope )
{
    assert( _block_index == IR_INVALID_INDEX );
    assert( scope.kind < GOTO_MAX );
    goto_stack& stack = _goto_stacks[ scope.kind ];
    assert( scope.index <= stack.fixups.size() );
    stack.index = scope.index;
}

ir_block_index ir_build::new_block( srcloc sloc, ir_block_kind kind )
{
    if ( _block_index != IR_INVALID_INDEX )
    {
        goto_scope goto_else = goto_open( sloc, GOTO_ELSE );
        end_block( emit_jump( sloc, IR_JUMP, 0, GOTO_ELSE ) );
        goto_block( goto_else );
    }

    ir_block block;
    block.kind = kind;
    block.lower = _f->ops.size();
    block.preceding_lower = _f->preceding_blocks.size();

    unsigned label = _f->ops.size();
    for ( size_t goto_kind = 0; goto_kind < GOTO_MAX; ++goto_kind )
    {
        goto_stack& stack = _goto_stacks[ goto_kind ];
        for ( unsigned i = stack.index; i < stack.fixups.size(); ++i )
        {
            goto_fixup fixup = stack.fixups[ i ];
            _f->preceding_blocks.append( fixup.block_index );
            ir_operand* operand = &_f->operands[ fixup.operand_index ];
            assert( operand->kind == IR_O_JUMP );
            operand->index = label;
        }

        assert( stack.index <= stack.fixups.size() );
        stack.fixups.resize( stack.index );
    }

    if ( kind == IR_BLOCK_UNSEALED )
    {
        _f->preceding_blocks.append( IR_INVALID_INDEX );
    }

    block.preceding_upper = _f->preceding_blocks.size();

    assert( _block_index == IR_INVALID_INDEX );
    _block_index = _f->blocks.append( block );

    _o.push_back( { IR_O_BLOCK, _block_index } );
    emit( sloc, IR_BLOCK, 1 );

    return _block_index;
}

ir_block_index ir_build::new_loop( ir_block_index loop_header )
{
    assert( loop_header == _block_index );
    assert( _f->blocks[ loop_header ].kind == IR_BLOCK_UNSEALED );
    return loop_header;
}

void ir_build::end_loop( ir_block_index loop_header, goto_scope scope )
{
    // Find loop header block.
    ir_block* block = &_f->blocks[ loop_header ];
    assert( block->kind == IR_BLOCK_UNSEALED );

    // Add predecessor blocks to the block's predecessor list.
    assert( scope.kind < GOTO_MAX );
    goto_stack& stack = _goto_stacks[ scope.kind ];
    assert( stack.index == stack.fixups.size() );
    assert( scope.index <= stack.fixups.size() );

    // One block we can just add in the preallocated slot.
    unsigned back_index = scope.index;
    if ( back_index < stack.fixups.size() )
    {
        assert( block->preceding_lower < block->preceding_upper );
        ir_block_index* preceding = &_f->preceding_blocks[ block->preceding_upper - 1 ];
        assert( *preceding == IR_INVALID_INDEX );
        *preceding = stack.fixups.at( back_index ).block_index;
        back_index += 1;
    }

    // But we might need to insert additional blocks into the list.
    if ( back_index < stack.fixups.size() )
    {
        size_t count = stack.fixups.size() - back_index;
        _f->preceding_blocks.insert( _f->preceding_blocks.begin() + block->preceding_upper, count, IR_INVALID_INDEX );
        for ( size_t i = 0; i < count; ++i )
        {
            _f->preceding_blocks[ block->preceding_upper ] = stack.fixups.at( back_index ).block_index;
            block->preceding_upper += 1;
            back_index += 1;
        }

        for ( unsigned block_index = loop_header + 1; block_index < _f->blocks.size(); ++block_index )
        {
            ir_block* next_block = &_f->blocks[ block_index ];
            next_block->preceding_lower += count;
            next_block->preceding_upper += count;
        }
    }

    // Fixup back edges.
    unsigned label = block->lower;

    assert( scope.index <= stack.fixups.size() );
    for ( unsigned i = scope.index; i < stack.fixups.size(); ++i )
    {
        goto_fixup fixup = stack.fixups[ i ];
        ir_operand* operand = &_f->operands[ fixup.operand_index ];
        assert( operand->kind == IR_O_JUMP );
        operand->index = label;
    }

    stack.fixups.resize( scope.index );
    stack.index = scope.index;

    // Seal loop.
    seal_loop( loop_header );
}

ir_operand ir_build::emit_jump( srcloc sloc, ir_opcode opcode, unsigned ocount, goto_kind goto_kind )
{
    if ( opcode == IR_JUMP && _block_index == IR_INVALID_INDEX )
    {
        /*
            Avoid emitting an empty block containing a single jump.  Instead
            move all jumps that targeted the next block to the goto stack.
        */
        goto_stack& stack = _goto_stacks[ goto_kind ];
        for ( size_t other_goto_kind = 0; other_goto_kind < GOTO_MAX; ++other_goto_kind )
        {
            if ( other_goto_kind == goto_kind )
            {
                continue;
            }

            goto_stack& other_stack = _goto_stacks[ other_goto_kind ];
            stack.fixups.insert( stack.fixups.end(), other_stack.fixups.begin() + other_stack.index, other_stack.fixups.end() );
            other_stack.fixups.resize( other_stack.index );
        }

        stack.index = stack.fixups.size();
        return { IR_O_NONE };
    }

    _o.push_back( { IR_O_JUMP, IR_INVALID_INDEX } );
    ir_operand jump = emit( sloc, opcode, ocount + 1 );
    const ir_op& op = _f->ops[ jump.index ];

    goto_stack& stack = _goto_stacks[ goto_kind ];
    assert( stack.index == stack.fixups.size() );
    stack.fixups.push_back( { _block_index, op.oindex + ocount } );
    stack.index += 1;

    return jump;
}

ir_operand ir_build::emit_test( srcloc sloc, ir_opcode opcode, unsigned ocount, goto_kind goto_true, goto_kind goto_false )
{
    _o.push_back( { IR_O_JUMP, IR_INVALID_INDEX } );
    _o.push_back( { IR_O_JUMP, IR_INVALID_INDEX } );
    ir_operand test = emit( sloc, opcode, ocount + 2 );
    const ir_op& op = _f->ops[ test.index ];

    goto_stack& stack_true = _goto_stacks[ goto_true ];
    assert( stack_true.index == stack_true.fixups.size() );
    stack_true.fixups.push_back( { _block_index, op.oindex + ocount + 0 } );
    stack_true.index += 1;

    goto_stack& stack_false = _goto_stacks[ goto_false ];
    assert( stack_false.index == stack_false.fixups.size() );
    stack_false.fixups.push_back( { _block_index, op.oindex + ocount + 1 } );
    stack_false.index += 1;

    return test;
}

ir_operand ir_build::end_block( ir_operand jump )
{
    if ( jump.kind == IR_O_NONE )
    {
        assert( _block_index == IR_INVALID_INDEX );
        return jump;
    }

    assert( jump.kind == IR_O_OP );
    const ir_op& op = _f->ops[ jump.index ];
    assert( op.opcode == IR_JUMP || op.opcode == IR_JUMP_TEST
        || op.opcode == IR_JUMP_FOR_EGEN || op.opcode == IR_JUMP_FOR_SGEN
        || op.opcode == IR_JUMP_FOR_EACH || op.opcode == IR_JUMP_FOR_STEP
        || op.opcode == IR_JUMP_THROW || op.opcode == IR_JUMP_RETURN );

    assert( _block_index != IR_INVALID_INDEX );
    ir_block* block = &_f->blocks[ _block_index ];
    block->upper = _f->ops.size();
    _block_index = IR_INVALID_INDEX;

    return jump;
}

size_t ir_build::block_local_hash::operator () ( block_local bl ) const
{
    return std::hash< unsigned >::operator () ( bl.block_index ) ^ std::hash< unsigned >::operator () ( bl.local_index );
}

bool ir_build::block_local_equal::operator () ( block_local a, block_local b ) const
{
    return a.block_index == b.block_index && a.local_index == b.local_index;
}

ir_operand ir_build::def( srcloc sloc, unsigned local_index, ir_operand operand )
{
    // Check for definition of local which is in varenv.
    const ast_local& local = _f->ast->locals[ local_index ];
    if ( local.varenv_index != AST_INVALID_INDEX )
    {
        _o.push_back( search_def( _block_index, local.varenv_index ) );
        _o.push_back( { IR_O_ENVSLOT, local.varenv_slot } );
        _o.push_back( operand );
        emit( sloc, IR_SET_ENV, 3 );
        return operand;
    }

    // Get op which produces the value assigned to the local.
    assert( operand.kind == IR_O_OP );
    ir_op* op = &_f->ops[ operand.index ];

    // If defining from a previous definition of a local, create new value.
    if ( op->local() != IR_INVALID_LOCAL )
    {
        _o.push_back( { IR_O_OP, operand.index } );
        operand = emit( sloc, IR_MOV, 1 );
        op = &_f->ops[ operand.index ];
    }

    // op is the new definition of the local
    assert( op->local() == IR_INVALID_LOCAL );
    assert( op->unpack() == 1 );
    op->set_local( local_index );

    // Add to def lookup.  This overrides any previous def of
    // this local in this block.
    assert( _block_index != IR_INVALID_INDEX );
    _defs.insert_or_assign( block_local{ _block_index, local_index }, operand );
    return operand;
}

ir_operand ir_build::use( srcloc sloc, unsigned local_index )
{
    if ( _block_index == IR_INVALID_INDEX )
    {
        new_block( sloc, IR_BLOCK_BASIC );
    }

    const ast_local& local = _f->ast->locals[ local_index ];
    if ( local.varenv_index == AST_INVALID_INDEX )
    {
        return search_def( _block_index, local_index );
    }
    else
    {
        _o.push_back( search_def( _block_index, local.varenv_index ) );
        _o.push_back( { IR_O_ENVSLOT, local.varenv_slot } );
        return emit( sloc, IR_GET_ENV, 2 );
    }
}

ir_operand ir_build::search_def( ir_block_index block_index, unsigned local_index )
{
    // Search for definition in this block.
    assert( block_index != IR_INVALID_INDEX );
    auto i = _defs.find( block_local{ block_index, local_index } );
    if ( i != _defs.end() )
    {
        return i->second;
    }

    // Construct open phi.
    ir_op phi;
    phi.opcode = IR_PHI_OPEN;
    phi.set_local( local_index );
    phi.phi_next = IR_INVALID_INDEX;
    unsigned phi_index = _f->ops.append( phi );

    // Link into block's list of phi ops.
    ir_block* block = &_f->blocks[ block_index ];
    if ( block->phi_head != IR_INVALID_INDEX )
    {
        _f->ops[ block->phi_tail ].phi_next = phi_index;
        block->phi_tail = phi_index;
    }
    else
    {
        block->phi_head = block->phi_tail = phi_index;
    }

    // This phi acts as the def for this block, but only if the block doesn't
    // have a real definition already.
    ir_operand operand = { IR_O_OP, phi_index };
    _defs.try_emplace( block_local{ block_index, local_index }, operand );

    // If block is sealed, perform recursive search for defs now.
    if ( block->kind != IR_BLOCK_UNSEALED )
    {
        close_phi( block_index, local_index, phi_index );
    }

    return operand;
}

void ir_build::close_phi( ir_block_index block_index, unsigned local_index, unsigned phi_index )
{
    /*
        Construct phi op by searching for definitions that reach the block.
    */
    assert( block_index != IR_INVALID_INDEX );
    ir_block* block = &_f->blocks[ block_index ];
    size_t def_index = _def_stack.size();

    // Recursively search for definitions in predecessor blocks.
    size_t ref_count = 0;
    ir_operand ref = { IR_O_NONE };
    for ( unsigned index = block->preceding_lower; index < block->preceding_upper; ++index )
    {
        ir_block_index preceding_index = _f->preceding_blocks[ index ];

        // Find definition coming from this op.
        ir_operand def = { IR_O_NONE };
        if ( preceding_index != IR_INVALID_INDEX )
        {
            def = search_def( preceding_index, local_index );
            assert( def.kind == IR_O_OP );
        }

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

        // Add operand, in order of predecessor blocks.
        _def_stack.push_back( def );
    }

    // Modify open phi op.
    ir_op* op = &_f->ops[ phi_index ];
    assert( op->opcode == IR_PHI_OPEN );
    assert( op->local() == local_index );

    if ( ref_count != 1 )
    {
        // Add phi.
        op->opcode = IR_PHI;
        op->oindex = _f->operands.size();
        _f->operands.insert( _f->operands.end(), _def_stack.begin() + def_index, _def_stack.end() );
        op->ocount = _f->operands.size() - op->oindex;
    }
    else
    {
        // Add ref.
        op->opcode = IR_REF;
        op->oindex = _f->operands.append( ref );
        op->ocount = 1;
    }

    _def_stack.resize( def_index );
}

void ir_build::seal_loop( ir_block_index loop_header )
{
    assert( loop_header != IR_INVALID_INDEX );
    ir_block* block = &_f->blocks[ loop_header ];
    assert( block->kind == IR_BLOCK_UNSEALED );

    // Go through all phis and resolve them.
    for ( unsigned phi_index = block->phi_head; phi_index != IR_INVALID_INDEX; phi_index = _f->ops[ phi_index ].phi_next )
    {
        close_phi( loop_header, _f->ops[ phi_index ].local(), phi_index );
    }

    // Mark as sealed.
    block->kind = IR_BLOCK_LOOP;
}

}

