//
//  code.cpp
//
//  Created by Edmund Kapusniak on 12/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "code.h"
#include <stdio.h>
#include "escape_string.h"

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

const uint32_t* code_function::functions() const
{
    return (const uint32_t*)( selectors() + selector_count );
}

const code_debug_function* code_function::debug_function() const
{
    const code_debug_function* d = (const code_debug_function*)( functions() + function_count );
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
    [ OP_LDV        ] = "LDV %$r, #$Vc",
    [ OP_LDK        ] = "LDK %$r, #$Kc",
    [ OP_NEG        ] = "NEG %$r, %$a",
    [ OP_POS        ] = "POS %$r, %$a",
    [ OP_ADD        ] = "ADD %$r, %$a, %$b",
    [ OP_ADDN       ] = "ADDN %$r, %$a, #$Kb",
    [ OP_SUB        ] = "SUB %$r, %$a, %$b",
    [ OP_SUBN       ] = "SUBN %$r, %$a, #$Kb",
    [ OP_MUL        ] = "MUL %$r, %$a, %$b",
    [ OP_MULN       ] = "MULN %$r, %$a, #$Kb",
    [ OP_DIV        ] = "DIV %$r, %$a, %$b",
    [ OP_INTDIV     ] = "INTDIV %$r, %$a, %$b",
    [ OP_MOD        ] = "MOD %$r, %$a, %$b",
    [ OP_NOT        ] = "NOT %$r, %$a",
    [ OP_JMP        ] = "JMP $Jj",
    [ OP_JT         ] = "JT %$r, $Jj",
    [ OP_JF         ] = "JF %$r, $Jj",
    [ OP_JEQ        ] = "JEQ$Br, %$a, %$b, $*Jj",
    [ OP_JEQN       ] = "JEQ$BrN, %$a, #$Kb, $*Jj",
    [ OP_JEQS       ] = "JEQ$BrN, %$a, #$Kb, $*Jj",
    [ OP_JLT        ] = "JLT$Br, %$a, %$b, $*Jj",
    [ OP_JLTN       ] = "JLT$BrN, %$a, #$Kb, $*Jj",
    [ OP_JGTN       ] = "JGT$BrN, %$a, #$Kb, $*Jj",
    [ OP_JLE        ] = "JLE$Br, %$a, %$b, $*Jj",
    [ OP_JLEN       ] = "JLE$BrN, %$a, #$Kb, $*Jj",
    [ OP_JGEN       ] = "JGE$BrN, %$a, #$Kb, $*Jj",
    [ OP_GET_GLOBAL ] = "GET_GLOBAL %$r, #$Sc",
    [ OP_GET_KEY    ] = "GET_KEY %$r, %$a, #$Sb",
    [ OP_SET_KEY    ] = "SET_KEY %$r, %$a, #$Sb",
    [ OP_GET_INDEX  ] = "GET_INDEX %$r, %$a, %$b",
    [ OP_GET_INDEXI ] = "GET_INDEXI %$r, %$a, #$b",
    [ OP_SET_INDEX  ] = "SET_INDEX %$r, %$a, %$b",
    [ OP_SET_INDEXI ] = "SET_INDEXI %$r, %$a, #$b",
    [ OP_NEW_ENV    ] = "NEW_ENV %$r, #$c",
    [ OP_GET_VARENV ] = "GET_VARENV %$r, %$a, #$b",
    [ OP_SET_VARENV ] = "SET_VARENV %$r, %$a, #$b",
    [ OP_GET_OUTENV ] = "GET_OUTENV %$r, ^$a, #$b",
    [ OP_SET_OUTENV ] = "SET_OUTENV %$r, ^$a, #$b",
    [ OP_FUNCTION   ] = "FUNCTION %$r, #$c",
    [ OP_NEW_OBJECT ] = "NEW_OBJECT %$r, %$a",
    [ OP_NEW_ARRAY  ] = "NEW_ARRAY %$r, #$c",
    [ OP_NEW_TABLE  ] = "NEW_TABLE %$r, #$c",
    [ OP_APPEND     ] = "APPEND %$a, %$b",
    [ OP_CALL       ] = "CALL @$r:$b, @$r:$a",
    [ OP_CALLR      ] = "CALLR %$b, @$r:$a",
    [ OP_YCALL      ] = "YCALL @$r:$b, @$r:$a",
    [ OP_YIELD      ] = "YIELD @$r:$b, @$r:$a",
    [ OP_RETURN     ] = "RETURN @$r:$a",
    [ OP_VARARG     ] = "VARARG @$r:$b",
    [ OP_UNPACK     ] = "UNPACK @$r:$b, %$a",
    [ OP_EXTEND     ] = "EXTEND %$b, @$r:$a",
    [ OP_GENERATE   ] = "GENERATE %$r[2], %$a",
    [ OP_FOR_EACH   ] = "FOR_EACH @$r:$b, %$a[2], $*Jj",
    [ OP_FOR_STEP   ] = "FOR_STEP %$r, %$a[3], $*Jj",
    [ OP_CONCAT     ] = "CONCAT %$r, %$a, %$b",
    [ OP_CONCATS    ] = "CONCATS %$r, %$a, #$Kb",
    [ OP_RCONCATS   ] = "RCONCATS %$r, %$a, #$Kb",
    [ OP_BITNOT     ] = "BITNOT %$r, %$a",
    [ OP_LSHIFT     ] = "LSHIFT %$r, %$a, %$b",
    [ OP_RSHIFT     ] = "RSHIFT %$r, %$a, %$b",
    [ OP_ASHIFT     ] = "ASHIFT %$r, %$a, %$b",
    [ OP_BITAND     ] = "BITAND %$r, %$a, %$b",
    [ OP_BITXOR     ] = "BITXOR %$r, %$a, %$b",
    [ OP_BITOR      ] = "BITOR %$r, %$a, %$b",
    [ OP_LEN        ] = "LEN %$r, %$a",
    [ OP_IS         ] = "IS %$s, %$a, %$b",
    [ OP_SUPER      ] = "SUPER %$r, %$a",
    [ OP_THROW      ] = "THROW %$a",
    [ OP_F_METHOD   ] = "F_METHOD %$r, %$a",
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
    const uint32_t* functions = this->functions();
    const code_debug_function* debug = this->debug_function();
    const uint32_t* slocs = debug->slocs();

    if ( debug )
        printf( "FUNCTION %s:\n", debug_heap + debug->function_name );
    else
        printf( "FUNCTION:\n" );
    printf( "  %u OUTENV COUNT\n", outenv_count );
    printf( "  %u PARAM COUNT\n", param_count );
    printf( "  %u STACK SIZE\n", stack_size );
    if ( code_flags & CODE_VARARGS )
        printf( "  VARARGS\n" );
    if ( code_flags & CODE_GENERATOR )
        printf( "  GENERATOR\n" );

    if ( debug )
    {
        debug->debug_print( script );
    }

    printf( "  CONSTANTS\n" );
    for ( unsigned i = 0; i < constant_count; ++i )
    {
        const code_constant& k = constants[ i ];
        if ( k.text == ~(uint32_t)0 )
        {
            printf( "    %u : %f\n", i, k.n() );
        }
        else
        {
            std::string s = escape_string( std::string_view( heap + k.text, k.size ), 45 );
            printf( "    %u : %s\n", i, s.c_str() );
        }
    }

    printf( "  SELECTORS\n" );
    for ( unsigned i = 0; i < selector_count; ++i )
    {
        const code_selector& s = selectors[ i ];
        printf( "    %u : '%.*s'\n", i, s.size, heap + s.text );
    }

    printf( "  FUNCTIONS\n" );
    for ( unsigned i = 0; i < function_count; ++i )
    {
        uint32_t f = functions[ i ];
        printf( "    %u : %u\n", i, f );
    }

    for ( unsigned i = 0; i < op_count; ++i )
    {
        op cop = ops[ i ];
        printf( "[%3u]:%04X ", (unsigned)slocs[ i ], i );
        bool dual = false;

        for ( const char* p = OPCODE_PRINT[ cop.opcode ]; p[ 0 ]; ++p )
        {
            if ( p[ 0 ] == '$' )
            {
                op pop = cop;
                unsigned j = i;

                char okind = 'R';
                char field = p[ 1 ];
                if ( field == '*' )
                {
                    pop = ops[ i + 1 ];
                    j += 1;
                    p += 1;
                    field = p[ 1 ];
                    dual = true;
                }
                if ( field >= 'A' && field <= 'Z' )
                {
                    okind = field;
                    field = p[ 2 ];
                    p += 2;
                }
                else
                {
                    p += 1;
                }

                int v = 0;
                switch ( field )
                {
                case 'r': v = pop.r; break;
                case 'a': v = pop.a; break;
                case 'b': v = pop.b; break;
                case 'c': v = pop.c; break;
                case 'j': v = pop.j; break;
                }

                switch ( okind )
                {
                case 'R':
                {
                    printf( "%d", v );
                    break;
                }

                case 'V':
                {
                    if ( v == 0 )
                        printf( "null" );
                    else if ( v == 1 )
                        printf( "false" );
                    else if ( v == 3 )
                        printf( "true" );
                    else
                        printf( "!!%d", v );
                    break;
                }

                case 'B':
                {
                    printf( "%s", v ? "T" : "F" );
                    break;
                }

                case 'K':
                {
                    const code_constant& k = constants[ v ];
                    if ( k.text == ~(uint32_t)0 )
                    {
                        printf( "%f", k.n() );
                    }
                    else
                    {
                        std::string s = escape_string( std::string_view( heap + k.text, k.size ), 45 );
                        printf( "%s", s.c_str() );
                    }
                    break;
                }

                case 'S':
                {
                    const code_selector& s = selectors[ v ];
                    printf( "'%.*s'", (int)s.size, heap + s.text );
                    break;
                }

                case 'J':
                {
                    unsigned label = j + 1 + v;
                    printf( ":%04X", label );
                    break;
                }
                }
            }
            else
            {
                putchar( p[ 0 ] );
            }
        }

        if ( dual )
        {
            i += 1;
        }

        printf( "\n" );
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

