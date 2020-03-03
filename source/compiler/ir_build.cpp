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

ir_build::ir_build( report* report )
    :   _report( report )
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
    assert( _loop_gotos.empty() );
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

    // -- PARENTHESES --

    case AST_EXPR_PAREN:
    {
        ast_node_index u = ast_child_node( _f->ast, node );
        return visit( u );
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
    case AST_EXPR_AND:
    case AST_EXPR_OR:
    {
        /*
            Materialize.
        */

        goto_label goto_const_true;
        goto_label goto_const_false;
        goto_label goto_result;

        unsigned local_index = temporary();
        materialize_goto jump = { &goto_const_true, &goto_result, &goto_const_false, &goto_result };
        materialize( node, local_index, &jump );

        if ( goto_const_true.size() )
        {
            goto_block( &goto_const_true );
            _o.push_back( { IR_O_TRUE } );
            def( node->sloc, local_index, emit( node->sloc, IR_CONST, 1 ) );
            end_block( emit_jump( node->sloc, IR_JUMP, 0, &goto_result ) );
        }

        if ( goto_const_false.size() )
        {
            goto_block( &goto_const_false );
            _o.push_back( { IR_O_FALSE } );
            def( node->sloc, local_index, emit( node->sloc, IR_CONST, 1 ) );
            end_block( emit_jump( node->sloc, IR_JUMP, 0, &goto_result ) );
        }

        goto_block( &goto_result );
        return use( node->sloc, local_index );
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

    case AST_EXPR_IF:
    {
        /*
            x = if test then u else v end

                    if test goto next else goto else
            next:   $ <- u
                    goto endif
            else:   $ <- v
                    goto endif
            endif:  $

            x = if test0 then u elif test1 then v else w end

                    if test0 then goto next else goto else
            next:   $ <- u
                    goto endif
            else:   if test1 then goto next else goto else
            next:   $ <- v
                    goto endif
            else:   $ <- w
                    goto endif
            endif:  $
        */

        ast_node_index test = ast_child_node( _f->ast, node );
        ast_node_index expr = ast_next_node( _f->ast, test );
        ast_node_index next = ast_next_node( _f->ast, expr );

        unsigned local_index = temporary();

        goto_label goto_next;
        goto_label goto_else;
        goto_label goto_endif;

        while ( true )
        {
            visit_test( test, &goto_next, &goto_else );
            goto_block( &goto_next );

            def( expr->sloc, local_index, visit( expr ) );
            end_block( emit_jump( node->sloc, IR_JUMP, 0, &goto_endif ) );
            goto_block( &goto_else );

            if ( next.index < node.index && next->kind == AST_EXPR_ELIF )
            {
                test = ast_child_node( _f->ast, next );
                expr = ast_next_node( _f->ast, test );
                next = ast_next_node( _f->ast, next );
            }
            else
            {
                break;
            }
        }

        def( expr->sloc, local_index, visit( next ) );
        end_block( emit_jump( node->sloc, IR_JUMP, 0, &goto_endif ) );
        goto_block( &goto_endif );

        return use( node->sloc, local_index );
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
        ir_operand table = emit( node->sloc, IR_NEW_TABLE, 1 );

        unsigned kvcount = 0;
        for ( ast_node_index kv = ast_child_node( _f->ast, node ); kv.index < node.index; kv = ast_next_node( _f->ast, kv ) )
        {
            assert( kv->kind == AST_TABLE_KEY );
            ast_node_index k = ast_child_node( _f->ast, kv );
            ast_node_index v = ast_next_node( _f->ast, k );
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

        if ( qname->kind == AST_LOCAL_DECL )
        {
            ir_operand object = visit( value );
            def( node->sloc, qname->leaf_index().index, object );
        }
        else
        {
            assert( qname->kind == AST_EXPR_KEY );
            if ( value->kind == AST_DEF_FUNCTION )
            {
                ir_operand omethod = visit( ast_child_node( _f->ast, qname ) );
                _o.push_back( omethod );
                _o.push_back( selector_operand( qname ) );
                _o.push_back( def_function( value, omethod ) );
                emit( qname->sloc, IR_SET_KEY, 3 );
            }
            else
            {
                ir_operand object = visit( value );
                assign( qname, object );
            }
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
            assert( self.kind == LOCAL_PARAM_SELF );
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
            if ( _f->ast->locals[ local_index ].kind == LOCAL_PARAM_VARARG )
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
        /*
            if test then body end

                    if test goto next else goto else
            next:   body
                    goto endif
            else:
            endif:  ...

            if test then body else tail end

                    if test goto next else goto else
            next:   body
                    goto endif
            else:   tail
                    goto endif
            endif:  ...

            if test0 then body0 elif test1 then body1 else tail end

                    if test0 goto next else goto else
            next:   body0
                    goto endif
            else:   if test1 goto next else goto else
            next:   body1
                    goto endif
            else:   tail
                    goto endif
            endif:  ...

        */

        ast_node_index test = ast_child_node( _f->ast, node );
        ast_node_index body = ast_next_node( _f->ast, test );
        ast_node_index next = ast_next_node( _f->ast, body );

        goto_label goto_next;
        goto_label goto_else;
        goto_label goto_endif;

        while ( true )
        {
            visit_test( test, &goto_next, &goto_else );
            goto_block( &goto_next );

            visit( body );
            end_block( emit_jump( node->sloc, IR_JUMP, 0, &goto_endif ) );
            goto_block( &goto_else );

            if ( next.index < node.index && next->kind == AST_STMT_ELIF )
            {
                test = ast_child_node( _f->ast, next );
                body = ast_next_node( _f->ast, test );
                next = ast_next_node( _f->ast, next );
            }
            else
            {
                break;
            }
        }

        if ( next.index < node.index )
        {
            assert( next->kind == AST_BLOCK );
            visit( next );
            end_block( emit_jump( node->sloc, IR_JUMP, 0, &goto_endif ) );
        }

        goto_block( &goto_endif );
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

        goto_label goto_next;
        ir_operand sgen = emit_jump( node->sloc, IR_JUMP_FOR_SGEN, 3, &goto_next );
        def( node->sloc, local_index, sgen );
        end_block( sgen );
        goto_block( &goto_next );

        // Start of loop.
        ir_block_index loop = new_loop( new_block( node->sloc, IR_BLOCK_UNSEALED ) );

        // Mark break/continue.
        goto_label goto_break;
        goto_label goto_continue;
        _loop_gotos.push_back( { &goto_break, &goto_continue } );

        // For loop.
        _o.push_back( use( node->sloc, local_index ) );
        ir_operand test = emit_test( node->sloc, IR_JUMP_FOR_STEP, 1, &goto_next, &goto_break );
        def( node->sloc, local_index, test );
        end_block( test );
        goto_block( &goto_next );

        // Get index at head of loop.
        assert( name->kind == AST_LOCAL_DECL );
        def( name->sloc, name->leaf_index().index, emit( node->sloc, IR_FOR_STEP_INDEX, 0 ) );

        // Visit the body of the loop.
        visit( body );
        end_block( emit_jump( node->sloc, IR_JUMP, 0, &goto_continue ) );
        end_loop( loop, &goto_continue );

        // Break to after loop.
        _loop_gotos.pop_back();
        goto_block( &goto_break );
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
        goto_label goto_next;
        ir_operand egen = emit_jump( node->sloc, IR_JUMP_FOR_EGEN, 1, &goto_next );
        def( node->sloc, local_index, egen );
        end_block( egen );
        goto_block( &goto_next );

        // Start of loop.
        ir_block_index loop = new_loop( new_block( node->sloc, IR_BLOCK_UNSEALED ) );

        // Mark break/continue stacks.
        goto_label goto_break;
        goto_label goto_continue;
        _loop_gotos.push_back( { &goto_break, &goto_continue } );

        // For loop.
        _o.push_back( use( node->sloc, local_index ) );
        ir_operand test = emit_test( node->sloc, IR_JUMP_FOR_EACH, 1, &goto_next, &goto_break );
        def( node->sloc, local_index, test );
        end_block( test );
        goto_block( &goto_next );

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
        end_block( emit_jump( node->sloc, IR_JUMP, 0, &goto_continue ) );
        end_loop( loop, &goto_continue );

        // Break to after loop.
        _loop_gotos.pop_back();
        goto_block( &goto_break );
        return { IR_O_NONE };
    }

    case AST_STMT_WHILE:
    {
        ast_node_index expr = ast_child_node( _f->ast, node );
        ast_node_index body = ast_next_node( _f->ast, expr );

        // Open loop header.
        ir_block_index loop = new_loop( new_block( node->sloc, IR_BLOCK_UNSEALED ) );

        // Mark break/continue stacks.
        goto_label goto_break;
        goto_label goto_continue;
        _loop_gotos.push_back( { &goto_break, &goto_continue } );

        // Check condition.
        goto_label goto_next;
        visit_test( expr, &goto_next, &goto_break );
        goto_block( &goto_next );

        // Body of loop.
        visit( body );
        end_block( emit_jump( node->sloc, IR_JUMP, 0, &goto_continue ) );
        end_loop( loop, &goto_continue );

        // Break to after loop.
        _loop_gotos.pop_back();
        goto_block( &goto_break );
        return { IR_O_NONE };
    }

    case AST_STMT_REPEAT:
    {
        ast_node_index body = ast_child_node( _f->ast, node );
        ast_node_index expr = ast_next_node( _f->ast, body );

        // Open loop header.
        ir_block_index loop = new_loop( new_block( node->sloc, IR_BLOCK_UNSEALED ) );

        // Mark break/continue stacks.
        goto_label goto_break;
        goto_label goto_continue;
        _loop_gotos.push_back( { &goto_break, &goto_continue } );

        // Body of loop.
        visit( body );

        // Continue to condition.
        if ( goto_continue.size() )
        {
            end_block( emit_jump( node->sloc, IR_JUMP, 0, &goto_continue ) );
            goto_block( &goto_continue );
        }

        // Check condition and loop.
        visit_test( expr, &goto_break, &goto_continue );
        end_loop( loop, &goto_continue );

        // Break to after loop.
        _loop_gotos.pop_back();
        goto_block( &goto_break );
        return { IR_O_NONE };
    }

    case AST_STMT_BREAK:
    {
        end_block( emit_jump( node->sloc, IR_JUMP, 0, _loop_gotos.back().loop_break ) );
        return { IR_O_NONE };
    }

    case AST_STMT_CONTINUE:
    {
        end_block( emit_jump( node->sloc, IR_JUMP, 0, _loop_gotos.back().loop_continue ) );
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
        return def_function( node, { IR_O_NONE } );
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

            if ( value->kind == AST_DEF_FUNCTION && child->kind == AST_DECL_DEF )
            {
                _o.push_back( def_function( value, object ) );
            }
            else
            {
                _o.push_back( visit( value ) );
            }
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
    {
        unsigned local_index = node->leaf_index().index;
        return use( node->sloc, local_index );
    }

    case AST_SUPER_NAME:
    {
        return emit( node->sloc, IR_SUPER, 0 );
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

ir_operand ir_build::def_function( ast_node_index node, ir_operand omethod )
{
    assert( node->kind == AST_DEF_FUNCTION );
    ast_function* function = node->leaf_function().function;

    unsigned ocount = 2;
    _o.push_back( { IR_O_FUNCTION, function->index } );
    _o.push_back( omethod );

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

void ir_build::visit_test( ast_node_index node, goto_label* goto_true, goto_label* goto_false )
{
    switch ( node->kind )
    {
    case AST_EXPR_COMPARE:
    {
        /*
            u < v

                    %0 <- u
                    %1 <- v
                    %2 <- u < v
                    if %2 goto true else goto false

            u < v < w

                    %0 <- u
                    %1 <- $ <- v
                    %2 <- u < v
                    if %2 goto next else goto false
            next:   %3 <- w
                    %4 <- $ < w
                    if %4 goto true else goto false
        */

        ast_node_index u = ast_child_node( _f->ast, node );
        ast_node_index op = ast_next_node( _f->ast, u );
        ast_node_index v = ast_next_node( _f->ast, op );
        ast_node_index chain_op = ast_next_node( _f->ast, v );

        _o.push_back( visit( u ) );

        goto_label goto_next;

        if ( chain_op.index < node.index )
        {
            unsigned local_index = temporary();

            while ( chain_op.index < node.index )
            {
                _o.push_back( def( v->sloc, local_index, visit( v ) ) );
                _o.push_back( comparison( op ) );

                goto_label goto_next;
                end_block( emit_test( op->sloc, IR_JUMP_TEST, 1, &goto_next, goto_false ) );
                goto_block( &goto_next );

                op = chain_op;
                v = ast_next_node( _f->ast, op );
                chain_op = ast_next_node( _f->ast, v );

                _o.push_back( use( op->sloc, local_index ) );
            }
        }

        _o.push_back( visit( v ) );
        _o.push_back( comparison( op ) );
        end_block( emit_test( op->sloc, IR_JUMP_TEST, 1, goto_true, goto_false ) );
        break;
    }

    case AST_EXPR_AND:
    {
        /*
            u and v

                    if u goto next else goto false
            next:   if v goto true else goto false
        */

        ast_node_index u = ast_child_node( _f->ast, node );
        ast_node_index v = ast_next_node( _f->ast, u );

        goto_label goto_next;
        visit_test( u, &goto_next, goto_false );
        goto_block( &goto_next );
        visit_test( v, goto_true, goto_false );
        break;
    }

    case AST_EXPR_OR:
    {
        /*
            u or v

                    if u goto true else goto next
            next:   if v goto true else goto false
        */

        ast_node_index u = ast_child_node( _f->ast, node );
        ast_node_index v = ast_next_node( _f->ast, u );

        goto_label goto_next;
        visit_test( u, goto_true, &goto_next );
        goto_block( &goto_next );
        visit_test( v, goto_true, goto_false );
        break;
    }

    case AST_EXPR_IF:
    {
        /*
            if test then u else v end

                    %0 <- test
                    if %0 goto next else goto else
            next:   %1 <- u
                    if %1 goto true else goto false
            else:   %2 <- v
                    if %2 goto true else goto false

            if test0 then u elif test1 then v else w end

                    %0 <- test0
                    if %0 goto next else goto else
            next:   %1 <- u
                    if %1 goto true else goto false
            else:   %2 <- test1
                    if %2 goto next else goto else
            next:   %3 <- v
                    if %3 goto true else goto false
            else:   %4 <- w
                    if %4 goto true else goto false
        */

        ast_node_index test = ast_child_node( _f->ast, node );
        ast_node_index expr = ast_next_node( _f->ast, test );
        ast_node_index next = ast_next_node( _f->ast, expr );

        goto_label goto_next;
        goto_label goto_else;

        while ( true )
        {
            visit_test( test, &goto_next, &goto_else );
            goto_block( &goto_next );

            visit_test( expr, goto_true, goto_false );
            goto_block( &goto_else );

            if ( next.index < node.index && next->kind == AST_EXPR_ELIF )
            {
                ast_node_index elif = next;
                test = ast_child_node( _f->ast, elif );
                expr = ast_next_node( _f->ast, test );
                next = ast_next_node( _f->ast, elif );
            }
            else
            {
                break;
            }
        }

        visit_test( next, goto_true, goto_false );
        break;
    }

    default:
    {
        _o.push_back( visit( node ) );
        end_block( emit_test( node->sloc, IR_JUMP_TEST, 1, goto_true, goto_false ) );
        break;
    }
    }
}

void ir_build::materialize( ast_node_index node, unsigned local_index, materialize_goto* jump )
{
    switch ( node->kind )
    {
    case AST_EXPR_COMPARE:
    {
        /*
                    if test then goto true else goto false
        */
        visit_test( node, jump->const_true, jump->const_false );
        break;
    }

    case AST_EXPR_AND:
    {
        /*
                    if u then goto next else goto false
            next:   if v then goto true else goto false
        */
        ast_node_index u = ast_child_node( _f->ast, node );
        ast_node_index v = ast_next_node( _f->ast, u );

        goto_label goto_next;
        materialize_goto ujump = { &goto_next, &goto_next, jump->const_false, jump->value_false };
        materialize( u, local_index, &ujump );
        goto_block( &goto_next );
        materialize( v, local_index, jump );
        break;
    }

    case AST_EXPR_OR:
    {
        /*
                    if u then goto true else goto next
            next:   if v then goto true else goto false
        */
        ast_node_index u = ast_child_node( _f->ast, node );
        ast_node_index v = ast_next_node( _f->ast, u );

        goto_label goto_next;
        materialize_goto ujump = { jump->const_true, jump->value_true, &goto_next, &goto_next };
        materialize( u, local_index, &ujump );
        goto_block( &goto_next );
        materialize( v, local_index, jump );
        break;
    }

    default:
    {
        /*
            materialize expr
                    %0 <- $ <- a
                    if %0 goto true else goto false
        */
        _o.push_back( def( node->sloc, local_index, visit( node ) ) );
        end_block( emit_test( node->sloc, IR_JUMP_TEST, 1, jump->value_true, jump->value_false ) );
        break;
    }
    }
}

ir_operand ir_build::comparison( ast_node_index op )
{
    /*
        We apply the following transformations:

            u > v becomes v < u
            u >= v becomes v <= u
            u is not v becomes not u is v

        I'm pretty sure that these hold even considering NaN.
    */

    ir_opcode opcode = IR_NOP;
    bool swap = false;
    bool cnot = false;

    switch ( op->kind )
    {
    case AST_OP_EQ:     opcode = IR_EQ;                 break;
    case AST_OP_NE:     opcode = IR_NE;                 break;
    case AST_OP_LT:     opcode = IR_LT;                 break;
    case AST_OP_LE:     opcode = IR_LE;                 break;
    case AST_OP_GT:     opcode = IR_LT; swap = true;    break;
    case AST_OP_GE:     opcode = IR_LE; swap = true;    break;
    case AST_OP_IS:     opcode = IR_IS;                 break;
    case AST_OP_IS_NOT: opcode = IR_IS; cnot = true;    break;
    default: break;
    }

    if ( swap )
    {
        std::swap( _o.back(), _o[ _o.size() - 2 ] );
    }

    ir_operand comparison = emit( op->sloc, opcode, 2 );

    if ( cnot )
    {
        _o.push_back( comparison );
        comparison = emit( op->sloc, IR_NOT, 1 );
    }

    return comparison;
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
        _report->error( node->sloc, "internal: mismatched rval count %d, expected %d", rvcount, unpack );
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
        _report->error( lval->sloc, "internal: lhs is not assignable" );
        return rval;
    }
}

ir_operand ir_build::expr_unpack( ast_node_index node, unsigned unpack )
{
    assert( node->kind == AST_EXPR_UNPACK );

    // Evaluate expression we want to unpack.
    ir_operand operand = { IR_O_NONE };
    ast_node_index u = ast_child_node( _f->ast, node );
    if ( u->kind == AST_LOCAL_NAME && _f->ast->locals[ u->leaf_index().index ].kind == LOCAL_PARAM_VARARG )
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
            ast_node_index object = ast_child_node( _f->ast, arg );
            ir_operand self = visit( object );

            _o.push_back( self );
            _o.push_back( selector_operand( arg ) );
            _o.push_back( emit( arg->sloc, IR_GET_KEY, 2 ) );

            if ( object->kind == AST_SUPER_NAME )
            {
                unsigned local_index = object->leaf_index().index;
                _o.push_back( use( object->sloc, local_index ) );
            }
            else
            {
                _o.push_back( self );
            }

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
    // If there's no block, then create one.
    if ( _block_index == IR_INVALID_INDEX )
    {
        new_block( sloc, IR_BLOCK_BASIC );
    }

    // Convert temporaries back to IR_O_OP by looking up the temporary in
    // this block.
    unsigned oindex = _o.size() - ocount;
    for ( size_t i = oindex; i < _o.size(); ++i )
    {
        ir_operand* operand = &_o[ i ];
        if ( operand->kind == IR_O_TEMP )
        {
            *operand = use( sloc, operand->index );
        }
    }

    // Emit op.
    ir_op op;
    op.opcode = opcode;
    op.ocount = ocount;
    op.oindex = ocount ? _f->operands.size() : IR_INVALID_INDEX;
    op.sloc = sloc;

    unsigned op_index = _f->ops.append( op );

    assert( ocount <= _o.size() );
    for ( unsigned i = 0; i < ocount; ++i )
    {
        _f->operands.append( _o[ oindex + i ] );
    }
    _o.resize( oindex );

    return { IR_O_OP, op_index };
}

void ir_build::goto_block( goto_label* label )
{
    _goto_block.splice( _goto_block.end(), *label );
}

ir_block_index ir_build::new_block( srcloc sloc, ir_block_kind kind )
{
    // If a block exists, close it with a jump to this new block.
    if ( _block_index != IR_INVALID_INDEX )
    {
        goto_label goto_next;
        end_block( emit_jump( sloc, IR_JUMP, 0, &goto_next ) );
        goto_block( &goto_next );
    }

    // Construct new block.
    ir_block block;
    block.kind = kind;
    block.lower = _f->ops.size();
    block.preceding_lower = _f->preceding_blocks.size();

    unsigned label = _f->ops.size();
    for ( goto_fixup fixup : _goto_block )
    {
        _f->preceding_blocks.append( fixup.block_index );
        ir_operand* operand = &_f->operands[ fixup.operand_index ];
        assert( operand->kind == IR_O_JUMP );
        operand->index = label;
    }
    _goto_block.clear();

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

void ir_build::end_loop( ir_block_index loop_header, goto_label* goto_loop )
{
    // Find loop header block.
    ir_block* block = &_f->blocks[ loop_header ];
    assert( block->kind == IR_BLOCK_UNSEALED );

    // One block we can just add in the preallocated slot.
    if ( goto_loop->size() )
    {
        goto_fixup fixup = goto_loop->front();
        assert( block->preceding_lower < block->preceding_upper );
        ir_block_index* preceding = &_f->preceding_blocks[ block->preceding_upper - 1 ];
        assert( *preceding == IR_INVALID_INDEX );
        *preceding = fixup.block_index;
    }

    // Insert other predecessor blocks.
    if ( goto_loop->size() > 1 )
    {
        size_t count = goto_loop->size() - 1;
        _f->preceding_blocks.insert( _f->preceding_blocks.begin() + block->preceding_upper, count, IR_INVALID_INDEX );

        for ( auto i = ++goto_loop->begin(); i != goto_loop->end(); ++i )
        {
            _f->preceding_blocks[ block->preceding_upper ] = i->block_index;
            block->preceding_upper += 1;
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
    for ( goto_fixup fixup : *goto_loop )
    {
        ir_operand* operand = &_f->operands[ fixup.operand_index ];
        assert( operand->kind == IR_O_JUMP );
        operand->index = label;
    }
    goto_loop->clear();

    // Seal loop.
    seal_loop( loop_header );
}

ir_operand ir_build::emit_jump( srcloc sloc, ir_opcode opcode, unsigned ocount, goto_label* goto_jump )
{
    if ( opcode == IR_JUMP && _block_index == IR_INVALID_INDEX )
    {
        /*
            Avoid emitting an empty block containing a single jump.  Instead
            move all jumps that targeted the next block to the goto stack.
        */
        goto_jump->splice( goto_jump->end(), _goto_block );
        return { IR_O_NONE };
    }

    _o.push_back( { IR_O_JUMP, IR_INVALID_INDEX } );
    ir_operand jump = emit( sloc, opcode, ocount + 1 );
    const ir_op& op = _f->ops[ jump.index ];
    goto_jump->push_back( { _block_index, op.oindex + ocount } );
    return jump;
}

ir_operand ir_build::emit_test( srcloc sloc, ir_opcode opcode, unsigned ocount, goto_label* goto_true, goto_label* goto_false )
{
    if ( goto_true == goto_false )
    {
        _o.pop_back();
        return emit_jump( sloc, IR_JUMP, 0, goto_true );
    }

    _o.push_back( { IR_O_JUMP, IR_INVALID_INDEX } );
    _o.push_back( { IR_O_JUMP, IR_INVALID_INDEX } );
    ir_operand test = emit( sloc, opcode, ocount + 2 );
    const ir_op& op = _f->ops[ test.index ];
    goto_true->push_back( { _block_index, op.oindex + ocount + 0 } );
    goto_false->push_back( { _block_index, op.oindex + ocount + 1 } );
    return test;
}

ir_operand ir_build::end_block( ir_operand jump )
{
    if ( jump.kind == IR_O_NONE )
    {
        assert( _block_index == IR_INVALID_INDEX );
        return jump;
    }

    // If any operands which reference op results survive the block, those
    // results must be upgraded to actual temporaries.
    for ( size_t i = 0; i < _o.size(); ++i )
    {
        ir_operand* operand = &_o[ i ];
        if ( operand->kind == IR_O_OP )
        {
            ir_op* op = &_f->ops[ operand->index ];
            if ( op->local() == IR_INVALID_LOCAL )
            {
                def( op->sloc, temporary(), *operand );
            }
            operand->kind = IR_O_TEMP;
            operand->index = op->local();
        }
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

unsigned ir_build::temporary()
{
    ast_local local = {};
    local.name = "$";
    local.kind = LOCAL_TEMPORARY;
    return _f->ast->locals.append( local );
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

