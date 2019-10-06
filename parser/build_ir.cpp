//
//  build_ir.cpp
//
//  Created by Edmund Kapusniak on 02/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "build_ir.h"

namespace kf
{

build_ir::build_ir( source* source )
    :   _source( source )
    ,   _block_index( IR_INVALID_INDEX )
{
    _loop_stack.push_back( IR_INVALID_INDEX );
}

build_ir::~build_ir()
{
}

std::unique_ptr< ir_function > build_ir::build( ast_function* function )
{
    // Set up for building.
    _f = std::make_unique< ir_function >();
    _f->ast = function;

    // Visit AST.
    node_index node = { &_f->ast->nodes.back(), (unsigned)_f->ast->nodes.size() - 1 };
    visit( node );
    assert( _o.empty() );

    // Done.
    return std::move( _f );
}

build_ir::node_index build_ir::child_node( node_index node )
{
    unsigned child_index = node.node->child_index;
    return { &_f->ast->nodes[ child_index ], child_index };
}

build_ir::node_index build_ir::next_node( node_index node )
{
    unsigned next_index = node.node->next_index;
    return { &_f->ast->nodes[ next_index ], next_index };
}


ir_operand build_ir::visit( node_index node )
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
        node_index u = child_node( node );
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
    case AST_EXPR_BITAND:
    case AST_EXPR_BITXOR:
    case AST_EXPR_BITOR:
    {
        node_index u = child_node( node );
        node_index v = next_node( u );
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

        node_index u = child_node( node );
        node_index op = next_node( u );
        node_index v = next_node( op );

        unsigned ocount = 0;
        goto_scope goto_else = goto_open( GOTO_ELSE );
        goto_scope goto_endif = goto_open( GOTO_ENDIF );

        ir_operand last = visit( u );
        ir_operand comp = { IR_O_NONE };
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

            op = next_node( v );
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
            v = next_node( op );
        }

        if ( ocount )
        {
            _o.push_back( comp );
            goto_branch( goto_endif );
            comp = emit( node->sloc, IR_B_PHI, ocount + 1 );
        }

        return comp;
    }

    case AST_EXPR_NOT:
    {
        node_index u = child_node( node );
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

        node_index u = child_node( node );
        node_index v = next_node( u );

        goto_scope goto_else = goto_open( GOTO_ELSE );
        goto_scope goto_endif = goto_open( GOTO_ENDIF );

        ir_operand lhs = visit( u );

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

        node_index u = child_node( node );
        node_index v = next_node( u );

        goto_scope goto_else = goto_open( GOTO_ELSE );
        goto_scope goto_endif = goto_open( GOTO_ENDIF );

        ir_operand lhs = visit( u );

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

        node_index kw = node;
        node_index test = child_node( kw );
        node_index expr = next_node( test );
        node_index next = next_node( expr );

        size_t ocount = 0;
        goto_scope goto_else = goto_open( GOTO_ELSE );
        goto_scope goto_endif = goto_open( GOTO_ENDIF );

        while ( true )
        {
            _o.push_back( visit( test ) );
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
            test = child_node( kw );
            expr = next_node( test );
            next = next_node( kw );
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
        node_index u = child_node( node );
        _o.push_back( visit( u ) );
        _o.push_back( string_operand( node ) );
        return emit( node->sloc, IR_GET_KEY, 2 );
    }

    case AST_EXPR_INDEX:
    {
        node_index u = child_node( node );
        node_index v = next_node( u );
        _o.push_back( visit( u ) );
        _o.push_back( visit( v ) );
        return emit( node->sloc, IR_GET_KEY, 2 );
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
        ir_operand array = emit( node->sloc, IR_NEW_ARRAY, 0 );
        for ( node_index el = child_node( node ); el.index < node.index; el = next_node( el ) )
        {
            _o.push_back( array );
            if ( el->kind != AST_EXPR_UNPACK )
            {
                _o.push_back( visit( el ) );
                emit( node->sloc, IR_APPEND, 2 );
            }
            else
            {
                _o.push_back( expr_unpack( el, IR_UNPACK_ALL ) );
                emit( node->sloc, IR_EXTEND, 2 );
            }
        }
        return array;
    }

    case AST_EXPR_TABLE:
    {
        ir_operand table = emit( node->sloc, IR_NEW_TABLE, 0 );
        for ( node_index kv = child_node( node ); kv.index < node.index; kv = next_node( kv ) )
        {
            assert( kv->kind == AST_TABLE_KEY );
            node_index k = child_node( kv );
            node_index v = next_node( kv );
            _o.push_back( table );
            _o.push_back( visit( k ) );
            _o.push_back( visit( v ) );
            emit( node->sloc, IR_SET_INDEX, 3 );
        }
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
        node_index names = child_node( node );
        node_index rvals = next_node( names );

        // Might have a list of names.
        node_index name = names;
        node_index name_done = next_node( name );
        if ( names->kind == AST_NAME_LIST )
        {
            name = child_node( names );
            name_done = names;
        }

        // Count number of names.
        unsigned rvcount = 0;
        for ( node_index c = name; c.index < name_done.index; c = next_node( c ) )
        {
            rvcount += 1;
        }

        if ( rvals.index < node.index )
        {
            // Evaluate rvals.
            unsigned rvindex = rval_list( rvals, rvcount );

            // Assign.
            unsigned rv = rvindex;
            for ( ; name.index < name_done.index; name = next_node( name ), ++rv )
            {
                assert( name->kind == AST_LOCAL_DECL );
                def( name->sloc, name->leaf_index().index, _o.at( rv ) );
            }

            _o.resize( rvindex );
        }
        else
        {
            // Assign null.
            for ( ; name.index < name_done.index; name = next_node( name ) )
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
        node_index qname = child_node( node );
        node_index value = next_node( qname );

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

    // -- SCOPE --

    case AST_FUNCTION:
    {
        visit_children( node );
        _o.push_back( { IR_O_NULL } );
        end_block( emit( node->sloc, IR_JUMP_RETURN, 1 ) );
        return { IR_O_NONE };
    }

    case AST_PARAMETERS:
    {
        for ( node_index param = child_node( node ); param.index < node.index; param = next_node( param ) )
        {
            if ( param->kind == AST_VARARG_PARAM )
            {
                continue;
            }

            unsigned local = param->leaf_index().index;
            _o.push_back( { IR_O_LOCAL_INDEX, local } );
            def( param->sloc, local, emit( param->sloc, IR_PARAM, 1 ) );
        }
        return { IR_O_NONE };
    }

    case AST_BLOCK:
    {
        visit_children( node );
        close_upstack( node );
        return { IR_O_NONE };
    }

    // -- STATEMENTS --
/*

    case AST_STMT_IF:
    {
        node_index expr = child_node( node );
        node_index body = next_node( expr );
        node_index next = next_node( body );

        test_fixup link;
        size_t endif_index = _jump_endif.size();

        while ( true )
        {
            // Test ends current block.
            link = block_test( node->sloc, visit( expr ) );

            // Body of if.
            fixup( link.if_true, block_head( body->sloc ) );
            visit( body );
            _jump_endif.push_back( block_jump( body->sloc ) );

            if ( next.index < node.index && next->kind == AST_STMT_ELIF )
            {
                fixup( link.if_false, block_head( next->sloc ) );
                expr = child_node( next );
                body = next_node( expr );
                next = next_node( next );
                continue;
            }
            else
            {
                break;
            }
        }

        if ( next.index < node.index )
        {
            // Else clause.
            assert( next->kind == AST_BLOCK );
            fixup( link.if_false, block_head( next->sloc ) );
            visit( next );
            _jump_endif.push_back( block_jump( next->sloc ) );
        }
        else
        {
            _jump_endif.push_back( link.if_false );
        }

        // Link all bodies to endif.
        unsigned endif = block_head( node->sloc );
        for ( size_t i = endif_index; i < _jump_endif.size(); ++i )
        {
            fixup( _jump_endif[ i ], endif );
        }
        _jump_endif.resize( endif_index );

        break;
    }

    case AST_STMT_FOR_STEP:
    {
        node_index decl = child_node( node );
        node_index start = next_node( decl );
        node_index limit = next_node( start );
        node_index step = next_node( limit );
        node_index body = next_node( step );

        // Evaluate start : limit : step
        ir_operand o[ 3 ];
        o[ 0 ] = visit( start );
        o[ 1 ] = visit( limit );
        o[ 2 ] = visit( step );
        // TODO: assign i from start

        // Close current block.
        jump_fixup link_previous = block_jump( node->sloc );

        // Mark break/continue stacks.
        size_t continue_index = _jump_continue.size();
        size_t break_index = _jump_break.size();

        // Loop header including condition.
        unsigned loop_continue = block_head( node->sloc );
        fixup( link_previous, loop_continue );
        // TODO: Make phi/some kind of loop var for i/limit/step?
        test_fixup link_loop = block_test( node->sloc, emit( node->sloc, IR_FOR_STEP, o, 3 ) );
        _jump_break.push_back( link_loop.if_false );

        // Loop body.
        fixup( link_loop.if_true, block_head( body->sloc ) );
        // TODO: assign decl from i
        visit( body );
        _jump_continue.push_back( block_jump( body->sloc ) );

        // Fixup break/continue.
        unsigned loop_break = block_head( node->sloc );
        break_continue( break_index, loop_break, continue_index, loop_continue );
        break;
    }

    case AST_STMT_FOR_EACH:
    {
        node_index decl = child_node( node );
        node_index expr = next_node( decl );
        node_index body = next_node( expr );

        // Evaluate generator expression and construct generator from it.
        ir_operand o[ 1 ];
        o[ 0 ] = visit( expr );
        o[ 0 ] = emit( node->sloc, IR_GENERATE, o, 1 );
        // TODO: assign g/i from result of generate?

        // Close current block.
        jump_fixup link_previous = block_jump( node->sloc );

        // Mark break/continue stacks.
        size_t continue_index = _jump_continue.size();
        size_t break_index = _jump_break.size();

        // Loop header including condition.
        unsigned loop_continue = block_head( node->sloc );
        fixup( link_previous, loop_continue );
        // TODO: make phi/some kind of loop var for g/i
        test_fixup link_loop = block_test( node->sloc, emit( node->sloc, IR_FOR_EACH, o, 1 ) );
        _jump_break.push_back( link_loop.if_false );

        // Loop body.
        fixup( link_loop.if_true, block_head( body->sloc ) );
        // TODO: assign decls from unpacking g/i
        visit( body );
        _jump_continue.push_back( block_jump( body->sloc ) );

        // Fixup break/continue.
        unsigned loop_break = block_head( node->sloc );
        break_continue( break_index, loop_break, continue_index, loop_continue );
        break;
    }

    case AST_STMT_WHILE:
    {
        node_index expr = child_node( node );
        node_index body = next_node( expr );

        // Close previous block.
        jump_fixup link_previous = block_jump( node->sloc );

        // Mark break/continue stacks.
        size_t continue_index = _jump_continue.size();
        size_t break_index = _jump_break.size();

        // Loop header including condition.
        unsigned loop_continue = block_head( node->sloc );
        fixup( link_previous, loop_continue );
        test_fixup link_loop = block_test( node->sloc, visit( expr ) );
        _jump_break.push_back( link_loop.if_false );

        // Loop body.
        fixup( link_loop.if_true, block_head( body->sloc ) );
        visit( body );
        _jump_continue.push_back( block_jump( body->sloc ) );

        // Fixup break/continue.
        unsigned loop_break = block_head( node->sloc );
        break_continue( break_index, loop_break, continue_index, loop_continue );
        break;
    }

    case AST_STMT_REPEAT:
    {
        node_index body = child_node( node );
        node_index expr = next_node( body );

        // Close previous block.
        jump_fixup link_previous = block_jump( node->sloc );

        // Mark break/continue stacks.
        size_t continue_index = _jump_continue.size();
        size_t break_index = _jump_break.size();

        // Loop header/body.
        unsigned loop_header = block_head( node->sloc );
        fixup( link_previous, loop_header );
        visit( body );

        // Make continue block if there are any continues.
        unsigned loop_continue = IR_INVALID_INDEX;
        if ( continue_index < _jump_continue.size() )
        {
            jump_fixup link_previous = block_jump( node->sloc );
            loop_continue = block_head( node->sloc );
            fixup( link_previous, loop_continue );
        }

        // Loop condition.
        test_fixup link_loop = block_test( node->sloc, visit( expr ) );
        _jump_break.push_back( link_loop.if_true );
        fixup( link_loop.if_false, loop_header );

        // Fixup break/continue.
        unsigned loop_break = block_head( node->sloc );
        break_continue( break_index, loop_break, continue_index, loop_continue );
        break;
    }

    case AST_STMT_BREAK:
    {
        close_upstack( node );
        _jump_break.push_back( block_jump( node->sloc ) );
        break;
    }

    case AST_STMT_CONTINUE:
    {
        close_upstack( node );
        _jump_continue.push_back( block_jump( node->sloc ) );
        break;
    }

    case AST_STMT_RETURN:
    {
        if ( child_node( node ).index < node.index )
        {
            block_last( list_op( IR_BLOCK_RETURN, node ) );
        }
        else
        {
            ir_operand o[ 1 ];
            o[ 0 ] = { IR_O_NULL };
            block_last( emit( node->sloc, IR_BLOCK_RETURN, o, 1 ) );
        }
        break;
    }

    case AST_STMT_THROW:
    {
        ir_operand o[ 1 ];
        o[ 0 ] = visit( child_node( node ) );
        block_last( emit( node->sloc, IR_BLOCK_THROW, o, 1 ) );
        break;
    }

*/

    case AST_NAME:
    {
        assert( ! "unexpected NAME node" );
        return { IR_O_NONE };
    }

    case AST_LOCAL_DECL:
    {
//        assert( ! "unexpected LOCAL_DECL node" );
        return { IR_O_NONE };
    }

    case AST_GLOBAL_NAME:
    {
        _o.push_back( string_operand( node ) );
        return emit( node->sloc, IR_GET_GLOBAL, 1 );
    }

    case AST_UPVAL_NAME:
    case AST_UPVAL_NAME_SUPER:
    {
        _o.push_back( { IR_O_UPVAL_INDEX, node->leaf_index().index } );
        ir_operand value = emit( node->sloc, IR_GET_UPVAL, 1 );
        if ( node->kind == AST_UPVAL_NAME_SUPER )
        {
            _o.push_back( value );
            value = emit( node->sloc, IR_SUPEROF, 1 );
        }
        return value;
    }

    case AST_LOCAL_NAME:
    case AST_LOCAL_NAME_SUPER:
    {
        unsigned local_index = node->leaf_index().index;

        // TODO: Find SSA definition that reaches this point.
        _o.push_back( { IR_O_LOCAL_INDEX, local_index } );
        ir_operand value = emit( node->sloc, IR_VAL, 1 );
        _f->ops.at( value.index ).local = local_index;

        if ( node->kind == AST_UPVAL_NAME_SUPER )
        {
            _o.push_back( value );
            value = emit( node->sloc, IR_SUPEROF, 1 );
        }
        else if ( _f->ast->locals.at( local_index ).upstack_index != AST_INVALID_INDEX )
        {
            // We have to pin all locals which escape the function, as they
            // may be clobbered by calls.
            value = pin( node->sloc, value );
        }

        return value;
    }

    default:
    {
        // TODO: remove this once all cases handled.
        visit_children( node );
        return { IR_O_NONE };
    }

    }
}

void build_ir::visit_children( node_index node )
{
    for ( node_index child = child_node( node ); child.index < node.index; child = next_node( child ) )
    {
        visit( child );
    }
}

unsigned build_ir::rval_list( node_index node, unsigned unpack )
{
    // Push rvcount rvals onto the evaluation stack, and return the index of
    // the first rval on the evaluation stack.
    unsigned rvindex = _o.size();
    unsigned rvcount = 0;

    if ( node->kind == AST_RVAL_ASSIGN )
    {
        // a, b, c = rvals
        node_index lvals = child_node( node );
        node_index rvals = next_node( lvals );

        // Might have a list of rvals.
        node_index lval = lvals;
        node_index lval_done = next_node( lval );
        if ( lvals->kind == AST_LVAL_LIST )
        {
            lval = child_node( lvals );
            lval_done = lvals;
        }

        // Count number of lvals.
        unsigned inner_unpack = 0;
        for ( node_index c = lval; c.index < lval_done.index; c = next_node( c ) )
        {
            inner_unpack += 1;
        }

        // Push rvals onto stack.
        unsigned inner_rvindex = rval_list( rvals, inner_unpack );
        assert( inner_rvindex == rvindex );

        // Perform assignments.
        unsigned rv = inner_rvindex;
        for ( ; lval.index < lval_done.index; lval = next_node( lval ), ++rv )
        {
            // Assign this value.
            assign( lval, _o.at( rv ) );

            // If the rval is not going to be reused then remove it from the
            // stack, preventing pointless upgrade of pins.
            if ( unpack <= rv )
            {
                _o[ rv ] = { IR_O_NONE };
            }
        }

        // Leave rvals on the stack, as our contribution.
        assert( rv == inner_rvindex + inner_unpack );
        assert( rv == _o.size() );
        rvcount += inner_unpack;
    }
    else if ( node->kind == AST_RVAL_OP_ASSIGN )
    {
        // a *= b
        node_index lval = child_node( node );
        node_index op = next_node( lval );
        node_index rval = next_node( op );

        // Evaluate left hand side, but remember operands.
        ir_operand uoperand = { IR_O_NONE };
        ir_operand voperand = { IR_O_NONE };
        if ( lval->kind == AST_EXPR_KEY )
        {
            uoperand = visit( child_node( lval ) );
            voperand = string_operand( lval );
            _o.push_back( uoperand );
            _o.push_back( voperand );
            _o.push_back( emit( lval->sloc, IR_GET_KEY, 2 ) );
        }
        else if ( lval->kind == AST_EXPR_INDEX )
        {
            node_index u = child_node( lval );
            node_index v = child_node( rval );
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
            assign( lval, _o.back() );
        }
    }
    else if ( node->kind == AST_RVAL_LIST )
    {
        // a, b, c ...
        for ( node_index rval = child_node( node ); rval.index < node.index; rval = next_node( rval ) )
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
        // References to locals on right hand side must be pinned in case an
        // assignment clobbers it before it can be used.
        _o.push_back( pin( node->sloc, visit( node ) ) );
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

ir_operand build_ir::expr_unpack( node_index node, unsigned unpack )
{
    assert( node->kind == AST_EXPR_UNPACK );

    // Evaluate expression we want to unpack.
    ir_operand operand = { IR_O_NONE };
    node_index u = child_node( node );
    if ( u->kind == AST_LOCAL_NAME && _f->ast->locals.at( u->leaf_index().index ).is_vararg_param )
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
    ir_op* op = &_f->ops.at( operand.index );
    assert( op->opcode == IR_VARARG || op->opcode == IR_CALL || op->opcode == IR_YCALL || op->opcode == IR_YIELD || op->opcode == IR_UNPACK );
    op->unpack = unpack;

    // Return op that unpacks.
    return operand;
}

void build_ir::assign( node_index lval, ir_operand rval )
{
    if ( lval->kind == AST_LOCAL_NAME )
    {
        def( lval->sloc, lval->leaf_index().index, rval );
    }
    else if ( lval->kind == AST_UPVAL_NAME )
    {
        _o.push_back( { IR_O_UPVAL_INDEX, lval->leaf_index().index } );
        _o.push_back( rval );
        emit( lval->sloc, IR_SET_UPVAL, 2 );
    }
    else if ( lval->kind == AST_EXPR_KEY )
    {
        _o.push_back( visit( child_node( lval ) ) );
        _o.push_back( string_operand( lval ) );
        _o.push_back( rval );
        emit( lval->sloc, IR_SET_KEY, 3 );
    }
    else if ( lval->kind == AST_EXPR_INDEX )
    {
        node_index u = child_node( lval );
        node_index v = next_node( lval );
        _o.push_back( visit( u ) );
        _o.push_back( visit( v ) );
        _o.push_back( rval );
        emit( lval->sloc, IR_SET_INDEX, 3 );
    }
    else
    {
        _source->error( lval->sloc, "internal: lhs is not assignable" );
    }
}

ir_operand build_ir::call_op( node_index node, ir_opcode opcode )
{
    unsigned ocount = 0;
    node_index arg = child_node( node );

    if ( opcode == IR_CALL || opcode == IR_YCALL )
    {
        // Pass self parameter to method calls.
        if ( arg->kind == AST_EXPR_KEY )
        {
            ir_operand self = visit( child_node( arg ) );
            _o.push_back( self );
            _o.push_back( string_operand( arg ) );
            _o.push_back( emit( arg->sloc, IR_GET_KEY, 2 ) );
            _o.push_back( self );
            ocount += 2;
        }
        else
        {
            _o.push_back( visit( arg ) );
            ocount += 1;
        }

        arg = next_node( arg );
    }

    for ( ; arg.index < node.index; arg = next_node( arg ) )
    {
        if ( arg->kind != AST_EXPR_UNPACK )
            _o.push_back( visit( arg ) );
        else
            _o.push_back( expr_unpack( arg, IR_UNPACK_ALL ) );
        ocount += 1;
    }

    ir_operand call = emit( node->sloc, opcode, ocount );
    fix_upval_pins();
    return call;
}

ir_operand build_ir::number_operand( node_index node )
{
    unsigned index = _f->numbers.size();
    _f->numbers.push_back( { node->leaf_number().n } );
    return { IR_O_NUMBER, index };
}

ir_operand build_ir::string_operand( node_index node )
{
    unsigned index = _f->strings.size();
    _f->strings.push_back( { node->leaf_string().text, node->leaf_string().size } );
    return { IR_O_STRING, index };
}

ir_operand build_ir::emit( srcloc sloc, ir_opcode opcode, unsigned ocount )
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

    unsigned op_index = _f->ops.size();
    _f->ops.push_back( op );

    assert( ocount <= _o.size() );
    unsigned oindex = _o.size() - ocount;
    for ( unsigned i = 0; i < ocount; ++i )
    {
        _f->operands.push_back( ignore_pin( _o[ oindex + i ] ) );
    }
    _o.resize( oindex );

    return { IR_O_OP, op_index };
}

void build_ir::close_upstack( node_index node )
{
    unsigned close_index = node->leaf_index().index;
    if ( close_index != AST_INVALID_INDEX )
    {
        _o.push_back( { IR_O_UPSTACK_INDEX, close_index } );
        emit( node->sloc, IR_CLOSE_UPSTACK, 1 );
    }
}

ir_operand build_ir::pin( srcloc sloc, ir_operand operand )
{
    /*
        On the right hand side of assignments, and for any local that is used
        as an upval, a load of the current definition of a local require a pin.
        This pin is upgraded to a real value if the local is assigned to, or
        if a function is called before the pin is popped from the stack.
    */

    operand = ignore_pin( operand );

    // Ignore operands that aren't definitions of locals.
    if ( operand.kind != IR_O_OP )
    {
        return operand;
    }

    ir_op* op = &_f->ops.at( operand.index );
    if ( op->local == IR_INVALID_LOCAL )
    {
        return operand;
    }

    // Emit pin.  Pins aren't definitions, but they do use the local field.
    _o.push_back( operand );
    operand = emit( sloc, IR_PIN, 1 );
    operand.kind = IR_O_PIN;
    _f->ops.at( operand.index ).local = op->local;

    // Done.
    return operand;
}

ir_operand build_ir::ignore_pin( ir_operand operand )
{
    while ( operand.kind == IR_O_PIN )
    {
        ir_op* op = &_f->ops.at( operand.index );
        assert( op->opcode == IR_PIN );
        assert( op->ocount == 1 );
        operand = _f->operands.at( op->oindex );
    }
    return operand;
}

void build_ir::fix_local_pins( unsigned local )
{
    for ( size_t i = 0; i < _o.size(); ++i )
    {
        ir_operand* pin = &_o[ i ];
        if ( pin->kind != IR_O_PIN )
        {
            continue;
        }

        ir_op* op = &_f->ops.at( pin->index );
        assert( op->opcode == IR_PIN );
        if ( op->local != local )
        {
            continue;
        }

        pin->kind = IR_O_OP;
        op->opcode = IR_VAL;
        op->local = IR_INVALID_LOCAL;
    }
}

void build_ir::fix_upval_pins()
{
    for ( size_t i = 0; i < _o.size(); ++i )
    {
        ir_operand* pin = &_o[ i ];
        if ( pin->kind != IR_O_PIN )
        {
            continue;
        }

        ir_op* op = &_f->ops.at( pin->index );
        assert( op->opcode == IR_PIN );
        if ( _f->ast->locals.at( op->local ).upstack_index == AST_INVALID_INDEX )
        {
            continue;
        }

        pin->kind = IR_O_OP;
        op->opcode = IR_VAL;
        op->local = IR_INVALID_LOCAL;
    }
}

void build_ir::def( srcloc sloc, unsigned local, ir_operand operand )
{
    // TODO: SSA definition.

    // Be robust against failures.
    if ( operand.kind == IR_O_NONE )
    {
        return;
    }

    // Upgrade pins on the stack that refer to the same local.
    fix_local_pins( local );

    // Get op which produces the value assigned to the local.
    operand = ignore_pin( operand );
    assert( operand.kind == IR_O_OP );
    ir_op* op = &_f->ops.at( operand.index );

    // If defining from a previous definition of a local, create new value.
    if ( op->local != IR_INVALID_LOCAL )
    {
        _o.push_back( { IR_O_OP, operand.index } );
        operand = emit( sloc, IR_VAL, 1 );
        op = &_f->ops.at( operand.index );
    }

    // op is the new definition of the local.
    assert( op->local == IR_INVALID_LOCAL );
    op->local = local;
}

build_ir::goto_scope build_ir::goto_open( goto_kind kind )
{
    unsigned index = _goto_stacks[ kind ].fixups.size();
    return { kind, index };
}

void build_ir::goto_branch( goto_scope scope )
{
    unsigned label = _f->ops.size();
    goto_stack& stack = _goto_stacks[ scope.kind ];
    for ( unsigned i = scope.index; i < stack.fixups.size(); ++i )
    {
        goto_fixup fixup = stack.fixups[ i ];
        assert( fixup.block_index == _block_index );
        ir_operand* operand = &_f->operands.at( fixup.operand_index );
        assert( operand->kind == IR_O_JUMP );
        operand->index = label;
    }
    stack.fixups.resize( scope.index );
    stack.index = scope.index;
}

void build_ir::goto_block( goto_scope scope )
{
    assert( _block_index == IR_INVALID_INDEX );
    goto_stack& stack = _goto_stacks[ scope.kind ];
    stack.index = scope.index;
}

ir_block_index build_ir::new_block( srcloc sloc, ir_block_kind kind )
{
    if ( _block_index != IR_INVALID_INDEX )
    {
        goto_scope goto_else = goto_open( GOTO_ELSE );
        end_block( emit_jump( sloc, IR_JUMP, 0, GOTO_ELSE ) );
        goto_block( goto_else );
    }

    ir_block block;
    block.kind = kind;
    block.loop = _loop_stack.back();
    block.lower = _f->ops.size();
    block.preceding_lower = _f->preceding_blocks.size();

    for ( size_t goto_kind = 0; goto_kind < GOTO_MAX; ++goto_kind )
    {
        goto_stack& stack = _goto_stacks[ goto_kind ];
        for ( unsigned i = stack.index; i < stack.fixups.size(); ++i )
        {
            goto_fixup fixup = stack.fixups[ i ];
            _f->preceding_blocks.push_back( fixup.block_index );
        }
        stack.fixups.resize( stack.index );
    }

    if ( kind == IR_BLOCK_UNSEALED )
    {
        _f->preceding_blocks.push_back( IR_INVALID_INDEX );
    }

    block.preceding_upper = _f->preceding_blocks.size();

    assert( _block_index == IR_INVALID_INDEX );
    _block_index = _f->blocks.size();
    _f->blocks.push_back( block );

    _o.push_back( { IR_O_BLOCK, _block_index } );
    emit( sloc, IR_BLOCK, 1 );

    return _block_index;
}

ir_block_index build_ir::new_loop( ir_block_index loop_header )
{
    assert( loop_header == _block_index );
    assert( _f->blocks.at( loop_header ).kind == IR_BLOCK_UNSEALED );
    _loop_stack.push_back( loop_header );
    return loop_header;
}

void build_ir::end_loop( ir_block_index loop_header, goto_scope scope )
{
    // Pop block from loop stack.
    ir_block* block = &_f->blocks.at( loop_header );
    assert( block->kind == IR_BLOCK_UNSEALED );
    assert( _loop_stack.back() == loop_header );
    _loop_stack.pop_back();

    // Add predecessor blocks to the block's predecessor list.
    goto_stack& stack = _goto_stacks[ scope.kind ];
    assert( stack.index == stack.fixups.size() );
    assert( scope.index <= stack.fixups.size() );

    // One block we can just add in the preallocated slot.
    unsigned back_index = scope.index;
    if ( back_index < stack.fixups.size() )
    {
        assert( block->preceding_lower < block->preceding_upper );
        ir_block_index* preceding = &_f->preceding_blocks.at( block->preceding_upper - 1 );
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
            ir_block* next_block = &_f->blocks.at( block_index );
            next_block->preceding_lower += count;
            next_block->preceding_upper += count;
        }
    }

    // Fixup back edges.
    unsigned label = block->lower;
    for ( unsigned i = scope.index; i < stack.fixups.size(); ++i )
    {
        goto_fixup fixup = stack.fixups[ i ];
        ir_operand* operand = &_f->operands.at( fixup.operand_index );
        assert( operand->kind == IR_O_JUMP );
        operand->index = label;
    }
    stack.fixups.resize( scope.index );
    stack.index = scope.index;

    // Seal loop.
    block->kind = IR_BLOCK_LOOP;

    // TODO: Now loop is sealed, perform SSA search backwards?
}

ir_operand build_ir::emit_jump( srcloc sloc, ir_opcode opcode, unsigned ocount, goto_kind goto_kind )
{
    unsigned operand_index = _f->operands.size() + ocount;
    _o.push_back( { IR_O_JUMP, IR_INVALID_INDEX } );
    ir_operand jump = emit( sloc, opcode, ocount + 1 );

    goto_stack& stack = _goto_stacks[ goto_kind ];
    assert( stack.index == stack.fixups.size() );
    stack.fixups.push_back( { _block_index, operand_index } );
    stack.index += 1;

    return jump;
}

ir_operand build_ir::emit_test( srcloc sloc, ir_opcode opcode, unsigned ocount, goto_kind goto_true, goto_kind goto_false )
{
    unsigned operand_index = _f->operands.size() + ocount;
    _o.push_back( { IR_O_JUMP, IR_INVALID_INDEX } );
    _o.push_back( { IR_O_JUMP, IR_INVALID_INDEX } );
    ir_operand test = emit( sloc, opcode, ocount + 1 );

    goto_stack& stack_true = _goto_stacks[ goto_true ];
    assert( stack_true.index == stack_true.fixups.size() );
    stack_true.fixups.push_back( { _block_index, operand_index + 0 } );
    stack_true.index += 1;

    goto_stack& stack_false = _goto_stacks[ goto_false ];
    assert( stack_false.index == stack_false.fixups.size() );
    stack_false.fixups.push_back( { _block_index, operand_index + 1 } );
    stack_false.index += 1;

    return test;
}

ir_operand build_ir::end_block( ir_operand jump )
{
    assert( jump.kind == IR_O_OP );
    const ir_op& op = _f->ops.at( jump.index );
    assert( op.opcode == IR_JUMP
        || op.opcode == IR_JUMP_TEST
        || op.opcode == IR_JUMP_TFOR
        || op.opcode == IR_JUMP_THROW
        || op.opcode == IR_JUMP_RETURN );

    assert( _block_index != IR_INVALID_INDEX );
    ir_block* block = &_f->blocks.at( _block_index );
    block->upper = _f->ops.size();
    _block_index = IR_INVALID_INDEX;

    return jump;
}

}

