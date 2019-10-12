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

namespace kf
{

const char* code_script::heap() const
{
    return (const char*)( this + 1 );
}

const code_function* code_script::functions() const
{
    const code_function* f = (const code_function*)( heap() + heap_size );
    return f->code_size ? f : nullptr;
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

const uint32_t* code_debug_function::newlines() const
{
    return (const uint32_t*)( slocs() + sloc_count );
}

const code_debug_variable* code_debug_function::variables() const
{
    return (const code_debug_variable*)( newlines() + newline_count );
}

const code_debug_var_span* code_debug_function::var_spans() const
{
    return (const code_debug_var_span*)( variables() + variable_count );
}

const char* code_debug_function::heap() const
{
    return (const char*)( var_spans() + var_span_count );
}

const char* const OPCODE_PRINT[] =
{
    [ OP_MOV        ] = "MOV %r, %a",
    [ OP_SWP        ] = "SWP %r, %a",
    [ OP_NULL       ] = "NULL %r",
    [ OP_BOOL       ] = "BOOL %r, %Bc",
    [ OP_LOADK      ] = "LOADK %r, [%Kc]",
    [ OP_LOADI      ] = "LOADI %r, [%j]",
    [ OP_LENGTH     ] = "LENGTH %r, %a",
    [ OP_NEG        ] = "NEG %r, %a",
    [ OP_POS        ] = "POS %r, %a",
    [ OP_BITNOT     ] = "BITNOT %r, %a",
    [ OP_NOT        ] = "NOT %r, %a",
    [ OP_ADD        ] = "ADD %r, %a, %b",
    [ OP_ADDK       ] = "ADDK %r, %a, [%Kb]",
    [ OP_ADDI       ] = "ADDI %r, %a, [%i]",
    [ OP_SUB        ] = "SUB %r, %a, %b",
    [ OP_SUBK       ] = "SUBK %r, %a, [%Kb]",
    [ OP_SUBI       ] = "SUBI %r, %a, [%Kb]",
    [ OP_MUL        ] = "MUL %r, %a, %b",
    [ OP_MULK       ] = "MULK %r, %a, [%Kb]",
    [ OP_MULI       ] = "MULI %r, %a, [%i]",
    [ OP_CONCAT     ] = "CONCAT %r, %a, %b",
    [ OP_CONCATK    ] = "CONCATK %r, %a, [%Kb]",
    [ OP_RCONCATK   ] = "RCONCATK %r, %a, [%Kb]",
    [ OP_DIV        ] = "DIV %r, %a, %b",
    [ OP_INTDIV     ] = "INTDIV %r, %a, %b",
    [ OP_MOD        ] = "MOD %r, %a, %b",
    [ OP_LSHIFT     ] = "LSHIFT %r, %a, %b",
    [ OP_RSHIFT     ] = "RSHIFT %r, %a, %b",
    [ OP_ASHIFT     ] = "ASHIFT %r, %a, %b",
    [ OP_BITAND     ] = "BITAND %r, %a, %b",
    [ OP_BITXOR     ] = "BITXOR %r, %a, %b",
    [ OP_BITOR      ] = "BITOR %r, %a, %b",
    [ OP_EQ         ] = "EQ %r, %a, %b",
    [ OP_NE         ] = "NE %r, %a, %b",
    [ OP_LT         ] = "LT %r, %a, %b",
    [ OP_LE         ] = "LE %r, %a, %b",
    [ OP_IS         ] = "IS %s, %a, %b",
    [ OP_JUMP       ] = "JUMP %Jj",
    [ OP_JCLOSE     ] = "JCLOSE !%r, %Jj",
    [ OP_JT         ] = "JT %r, %Jj",
    [ OP_JF         ] = "JF %r, %Jj",
    [ OP_JEQ        ] = "JEQ %a, %b",
    [ OP_JEQK       ] = "JEQK %a, [%Kb]",
    [ OP_JNE        ] = "JNE %a, %b",
    [ OP_JNEK       ] = "JNEK %a, [%Kb]",
    [ OP_JLT        ] = "JLT %a, %b",
    [ OP_JLTK       ] = "JLTK %a, [%Kb]",
    [ OP_JGTK       ] = "JGTK %a, [%Kb]",
    [ OP_JLE        ] = "JLE %a, %b",
    [ OP_JLEK       ] = "JLEK %a, [%Kb]",
    [ OP_JGEK       ] = "JGEK %a, [%Kb]",
    [ OP_GET_GLOBAL ] = "GET_GLOBAL [%Kc]",
    [ OP_GET_UPVAL  ] = "GET_UPVAL %r, ^%a",
    [ OP_SET_UPVAL  ] = "SET_UPVAL %r, ^%a",
    [ OP_GET_KEY    ] = "GET_KEY %r, %a, ?%b",
    [ OP_SET_KEY    ] = "SET_KEY %r, %a, ?%b",
    [ OP_GET_INDEX  ] = "GET_INDEX %r, %a, %b",
    [ OP_GET_INDEXK ] = "GET_INDEXK %r, %a, [%Kb]",
    [ OP_GET_INDEXI ] = "GET_INDEXI %r, %a, [%b]",
    [ OP_SET_INDEX  ] = "SET_INDEX %r, %a, %b",
    [ OP_SET_INDEXK ] = "SET_INDEXK %r, %a, [%Kb]",
    [ OP_SET_INDEXI ] = "SET_INDEXI %r, %a, [%b]",
    [ OP_NEW_OBJECT ] = "NEW_OBJECT %r, %a",
    [ OP_NEW_ARRAY  ] = "NEW_ARRAY %r, [%c]",
    [ OP_NEW_TABLE  ] = "NEW_TABLE %r, [%c]",
    [ OP_APPEND     ] = "APPEND %r, %a",
    [ OP_CALL       ] = "CALL %r, >%a, %b",
    [ OP_CALLX      ] = "CALLX %r, >%a, >%b",
    [ OP_YCALL      ] = "YCALL %r, >%a, %b",
    [ OP_YCALLX     ] = "YCALLX %r, >%a, >%b",
    [ OP_YIELD      ] = "YIELD %r, >%a, >%b",
    [ OP_EXTEND     ] = "EXTEND %r, >%a, %b",
    [ OP_RETURN     ] = "RETURN %r, >%a",
    [ OP_VARARG     ] = "VARARG %r, >%b",
    [ OP_UNPACK     ] = "UNPACK %r, %a, >%b",
    [ OP_GENERATE   ] = "GENERATE %r, %a, %b",
    [ OP_FOR_EACH   ] = "FOR_EACH %r, %a, >%b, %q",
    [ OP_FOR_STEP   ] = "FOR_STEP %r, %a, %b, %q",
    [ OP_SUPER      ] = "SUPER %r, %a",
    [ OP_THROW      ] = "THROW %r",
    [ OP_FUNCTION   ] = "FUNCTION %r, %Fc",
    [ OP_UPVAL      ] = "UPVAL !%r, %a",
    [ OP_UCOPY      ] = "UCOPY ^%a",
};

}

