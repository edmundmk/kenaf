//
//  code.cpp
//
//  Created by Edmund Kapusniak on 12/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "kenaf/code.h"
#include <stdio.h>

namespace kf
{

const code_function* code_script::functions() const
{
    const code_function* f = (const code_function*)( this + 1 );
    return f->code_size ? f : nullptr;
}

const char* code_script::heap() const
{
    return (const char*)( this + 1 ) + function_size;
}

const uint32_t* code_script::debug_newlines() const
{
    return (const uint32_t*)( heap() + heap_size );
}

const char* code_script::debug_heap() const
{
    return (const char*)( debug_newlines() + debug_newline_count );
}

const op* code_function::ops() const
{
    return (const op*)( this + 1 );
}

const code_constant* code_function::constants() const
{
    return (const code_constant*)( ops() + op_count );
}

const code_selector* code_function::selectors() const
{
    return (const code_selector*)( constants() + constant_count );
}

const code_debug_function* code_function::debug_function() const
{
    const code_debug_function* d = (const code_debug_function*)( selectors() + selector_count );
    return d->code_size ? d : nullptr;
}

const code_function* code_function::next() const
{
    const code_function* f = (const code_function*)( (const char*)this + code_size );
    return f->code_size ? f : nullptr;
}

const uint32_t* code_debug_function::slocs() const
{
    return (const uint32_t*)( this + 1 );
}

const code_debug_variable* code_debug_function::variables() const
{
    return (const code_debug_variable*)( slocs() + sloc_count );
}

const code_debug_var_span* code_debug_function::var_spans() const
{
    return (const code_debug_var_span*)( variables() + variable_count );
}

const char* const OPCODE_PRINT[] =
{
    [ OP_MOV        ] = "MOV %$r, %$a",
    [ OP_SWP        ] = "SWP %$r, %$a",
    [ OP_NULL       ] = "NULL %$r",
    [ OP_BOOL       ] = "BOOL %$r, %$Bc",
    [ OP_LDK        ] = "LDK %$r, #$Kc",
    [ OP_LEN        ] = "LEN %$r, %$a",
    [ OP_NEG        ] = "NEG %$r, %$a",
    [ OP_POS        ] = "POS %$r, %$a",
    [ OP_BITNOT     ] = "BITNOT %$r, %$a",
    [ OP_NOT        ] = "NOT %$r, %$a",
    [ OP_ADD        ] = "ADD %$r, %$a, %$b",
    [ OP_ADDK       ] = "ADDK %$r, %$a, #$Kb",
    [ OP_ADDI       ] = "ADDI %$r, %$a, #$i",
    [ OP_SUB        ] = "SUB %$r, %$a, %$b",
    [ OP_SUBK       ] = "SUBK %$r, %$a, #$Kb",
    [ OP_SUBI       ] = "SUBI %$r, %$a, #$Kb",
    [ OP_MUL        ] = "MUL %$r, %$a, %$b",
    [ OP_MULK       ] = "MULK %$r, %$a, #$Kb",
    [ OP_MULI       ] = "MULI %$r, %$a, #$i",
    [ OP_CONCAT     ] = "CONCAT %$r, %$a, %$b",
    [ OP_CONCATK    ] = "CONCATK %$r, %$a, #$Kb",
    [ OP_RCONCATK   ] = "RCONCATK %$r, %$a, #$Kb",
    [ OP_DIV        ] = "DIV %$r, %$a, %$b",
    [ OP_INTDIV     ] = "INTDIV %$r, %$a, %$b",
    [ OP_MOD        ] = "MOD %$r, %$a, %$b",
    [ OP_LSHIFT     ] = "LSHIFT %$r, %$a, %$b",
    [ OP_RSHIFT     ] = "RSHIFT %$r, %$a, %$b",
    [ OP_ASHIFT     ] = "ASHIFT %$r, %$a, %$b",
    [ OP_BITAND     ] = "BITAND %$r, %$a, %$b",
    [ OP_BITXOR     ] = "BITXOR %$r, %$a, %$b",
    [ OP_BITOR      ] = "BITOR %$r, %$a, %$b",
    [ OP_EQ         ] = "EQ %$r, %$a, %$b",
    [ OP_NE         ] = "NE %$r, %$a, %$b",
    [ OP_LT         ] = "LT %$r, %$a, %$b",
    [ OP_LE         ] = "LE %$r, %$a, %$b",
    [ OP_IS         ] = "IS %$s, %$a, %$b",
    [ OP_JUMP       ] = "JUMP %$Jj",
    [ OP_JT         ] = "JT %$r, %$Jj",
    [ OP_JF         ] = "JF %$r, %$Jj",
    [ OP_JEQ        ] = "JEQ %$a, %$b",
    [ OP_JEQK       ] = "JEQK %$a, #$Kb",
    [ OP_JNE        ] = "JNE %$a, %$b",
    [ OP_JNEK       ] = "JNEK %$a, #$Kb",
    [ OP_JLT        ] = "JLT %$a, %$b",
    [ OP_JLTK       ] = "JLTK %$a, #$Kb",
    [ OP_JGTK       ] = "JGTK %$a, #$Kb",
    [ OP_JLE        ] = "JLE %$a, %$b",
    [ OP_JLEK       ] = "JLEK %$a, #$Kb",
    [ OP_JGEK       ] = "JGEK %$a, #$Kb",
    [ OP_GET_GLOBAL ] = "GET_GLOBAL %$r, #$Sb",
    [ OP_GET_KEY    ] = "GET_KEY %$r, %$a, #$Sb",
    [ OP_SET_KEY    ] = "SET_KEY %$r, %$a, #$Sb",
    [ OP_GET_INDEX  ] = "GET_INDEX %$r, %$a, %$b",
    [ OP_GET_INDEXK ] = "GET_INDEXK %$r, %$a, #$Kb",
    [ OP_GET_INDEXI ] = "GET_INDEXI %$r, %$a, #$b",
    [ OP_SET_INDEX  ] = "SET_INDEX %$r, %$a, %$b",
    [ OP_SET_INDEXK ] = "SET_INDEXK %$r, %$a, #$Kb",
    [ OP_SET_INDEXI ] = "SET_INDEXI %$r, %$a, #$b",
    [ OP_NEW_ENV    ] = "NEW_ENV %$r, #$c",
    [ OP_GET_VARENV ] = "GET_VARENV %$r, %$a, #$b",
    [ OP_SET_VARENV ] = "SET_VARENV %$r, %$a, #$b",
    [ OP_GET_OUTENV ] = "GET_OUTENV %$r, ^$a, #$b",
    [ OP_SET_OUTENV ] = "SET_OUTENV %$r, ^$a, #$b",
    [ OP_NEW_OBJECT ] = "NEW_OBJECT %$r, %$a",
    [ OP_NEW_ARRAY  ] = "NEW_ARRAY %$r, #$c",
    [ OP_NEW_TABLE  ] = "NEW_TABLE %$r, #$c",
    [ OP_APPEND     ] = "APPEND %$r, %$a",
    [ OP_CALL       ] = "CALL %$r, @$a, @$b",
    [ OP_CALLR      ] = "CALLR %$r, @$a, %$b",
    [ OP_YCALL      ] = "YCALL %$r, @$a, @$b",
    [ OP_YCALLR     ] = "YCALLR %$r, @$a, %$b",
    [ OP_YIELD      ] = "YIELD %$r, @$a, @$b",
    [ OP_EXTEND     ] = "EXTEND %$r, @$a, %$b",
    [ OP_RETURN     ] = "RETURN %$r, @$a",
    [ OP_VARARG     ] = "VARARG %$r, @$b",
    [ OP_UNPACK     ] = "UNPACK %$r, %$a, @$b",
    [ OP_GENERATE   ] = "GENERATE %$r, %$a, %$b",
    [ OP_FOR_EACH   ] = "FOR_EACH %$r, %$a, @$b, %$q",
    [ OP_FOR_STEP   ] = "FOR_STEP %$r, %$a, %$b, %$q",
    [ OP_SUPER      ] = "SUPER %$r, %$a",
    [ OP_THROW      ] = "THROW %$r",
    [ OP_FUNCTION   ] = "FUNCTION %$r, #$c",
    [ OP_F_VARENV   ] = "F_VARENV %$r, #$a, %$b",
    [ OP_F_OUTENV   ] = "F_OUTENV %$r, #$a, ^$b",
};

void code_script::debug_print() const
{
    const char* debug_heap = this->debug_heap();
    printf( "SCRIPT %s\n", debug_heap + debug_script_name );
    for ( const code_function* function = functions(); function; function = function->next() )
    {
        function->debug_print( this );
    }
}

void code_function::debug_print( const code_script* script ) const
{
    const char* heap = script->heap();
    const char* debug_heap = script->debug_heap();
    const op* ops = this->ops();
    const code_constant* constants = this->constants();
    const code_selector* selectors = this->selectors();
    const code_debug_function* debug = this->debug_function();

    if ( debug )
        printf( "FUNCTION %s:\n", debug_heap + debug->function_name );
    else
        printf( "FUNCTION:\n" );
    printf( "  %u OUTENV COUNT\n", outenv_count );
    printf( "  %u PARAM COUNT\n", param_count );
    printf( "  %u STACK SIZE\n", stack_size );
    if ( flags & CODE_FLAGS_VARARGS )
        printf( "  VARARGS\n" );
    if ( flags & CODE_FLAGS_GENERATOR )
        printf( "  GENERATOR\n" );

    if ( debug )
    {
        debug->debug_print( script );
    }

    printf( "  CONSTANTS\n" );
    for ( unsigned i = 0; i < constant_count; ++i )
    {
        const code_constant& k = constants[ i ];
        if ( k.is_number() )
            printf( "    %u : %f\n", i, k.as_number() );
        else
            printf( "    %u : \"%s\"\n", i, heap + k.as_offset() );
    }

    printf( "  SELECTORS\n" );
    for ( unsigned i = 0; i < selector_count; ++i )
    {
        const code_selector& s = selectors[ i ];
        printf( "    %u : '%s'\n", i, heap + s.key );
    }

    for ( unsigned i = 0; i < op_count; ++i )
    {
        const op& op = ops[ i ];
        printf( ":%04X ", i );
        for ( const char* p = OPCODE_PRINT[ op.opcode ]; p[ 0 ]; ++p )
        {
            if ( p[ 0 ] == '$' )
            {
                char okind = 'R';
                char field = p[ 1 ];
                if ( field >= 'A' && field <= 'Z' )
                {
                    okind = field;
                    field = p[ 2 ];
                }

                int v = 0;
                switch ( field )
                {
                case 'r': v = op.r; break;
                case 'a': v = op.a; break;
                case 'b': v = op.b; break;
                case 'c': v = op.c; break;
                case 'i': v = op.i; break;
                case 'j': v = op.j; break;
                }

                switch ( okind )
                {
                case 'R':
                {
                    printf( "%d", v );
                    break;
                }

                case 'B':
                {
                    printf( "%s", v ? "true" : "false" );
                    break;
                }

                case 'K':
                {
                    const code_constant& k = constants[ v ];
                    if ( k.is_number() )
                        printf( "%f", k.as_number() );
                    else
                        printf( "\"%s\"", heap + k.as_offset() );
                    break;
                }

                case 'S':
                {
                    const code_selector& s = selectors[ v ];
                    printf( "'%s'", heap + s.key );
                    break;
                }
                }
            }
            else
            {
                putchar( p[ 0 ] );
            }
        }
    }
}

void code_debug_function::debug_print( const code_script* script ) const
{
    const char* debug_heap = script->debug_heap();
    const code_debug_variable* variables = this->variables();
    const code_debug_var_span* var_spans = this->var_spans();

    printf( "  VARIABLES:\n" );
    for ( unsigned i = 0; i < variable_count; ++i )
    {
        const code_debug_variable& v = variables[ i ];
        printf( "    %u : [%u] %s\n", i, v.r, debug_heap + v.variable_name );
    }

    printf( "  VAR SPANS:\n" );
    for ( unsigned i = 0; i < var_span_count; ++i )
    {
        const code_debug_var_span& s = var_spans[ i ];
        printf( "    %u : %u :%04X:%04X\n", i, s.variable_index, s.lower, s.upper );
    }
}

}
