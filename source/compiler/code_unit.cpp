//
//  code_unit.cpp
//
//  Created by Edmund Kapusniak on 19/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "code_unit.h"

namespace kf
{

code_unit::code_unit()
{
}

code_unit::~code_unit()
{
}

code_function_unit::code_function_unit()
{
}

code_function_unit::~code_function_unit()
{
}

const code_script* code_unit::pack() const
{
    size_t code_size = 0;
    for ( const code_function_unit& funit : functions )
    {
        code_size += sizeof( code_function );
        code_size += sizeof( op ) * funit.ops.size();
        code_size += sizeof( code_constant ) * funit.constants.size();
        code_size += sizeof( code_selector ) * funit.selectors.size();
        code_size += sizeof( code_debug_function );
        code_size += sizeof( uint32_t ) * funit.debug_slocs.size();
        code_size += sizeof( code_debug_variable ) * funit.debug_variables.size();
        code_size += sizeof( code_debug_var_span ) * funit.debug_var_spans.size();
    }
    code_size += sizeof( uint32_t );
    uint32_t function_size = code_size;

    code_size += sizeof( code_script );
    code_size += heap.size();
    code_size += sizeof( uint32_t ) * debug_newlines.size();
    code_size += debug_heap.size();

    code_script* s = (code_script*)malloc( code_size );
    s->magic = CODE_MAGIC;
    s->code_size = code_size;
    s->function_size = function_size;
    s->function_count = functions.size();
    s->heap_size = heap.size();
    s->debug_script_name = script.debug_script_name;
    s->debug_newline_count = debug_newlines.size();
    s->debug_heap_size = debug_heap.size();

    code_function* f = (code_function*)( s + 1 );
    for ( const code_function_unit& funit : functions )
    {
        uint32_t code_size = sizeof( code_function );
        code_size += sizeof( op ) * funit.ops.size();
        code_size += sizeof( code_constant ) * funit.constants.size();
        code_size += sizeof( code_selector ) * funit.selectors.size();

        uint32_t debug_size = sizeof( code_debug_function );
        debug_size += sizeof( uint32_t ) * funit.debug_slocs.size();
        debug_size += sizeof( code_debug_variable ) * funit.debug_variables.size();
        debug_size += sizeof( code_debug_var_span ) * funit.debug_var_spans.size();

        f->code_size = code_size + debug_size;
        f->op_count = funit.ops.size();
        f->constant_count = funit.constants.size();
        f->selector_count = funit.selectors.size();
        f->outenv_count = funit.function.outenv_count;
        f->param_count = funit.function.param_count;
        f->stack_size = funit.function.stack_size;
        f->flags = funit.function.flags;

        memcpy( (op*)f->ops(), funit.ops.data(), sizeof( op ) * funit.ops.size() );
        memcpy( (code_constant*)f->constants(), funit.constants.data(), sizeof( code_constant ) * funit.constants.size() );
        memcpy( (code_selector*)f->selectors(), funit.selectors.data(), sizeof( code_selector ) * funit.selectors.size() );

        code_debug_function* d = (code_debug_function*)( f->selectors() + f->selector_count );
        d->code_size = debug_size;
        d->function_name = funit.debug.function_name;
        d->sloc_count = funit.debug_slocs.size();
        d->variable_count = funit.debug_variables.size();
        d->var_span_count = funit.debug_var_spans.size();

        memcpy( (uint32_t*)d->slocs(), funit.debug_slocs.data(), sizeof( uint32_t ) * funit.debug_slocs.size() );
        memcpy( (code_debug_variable*)d->variables(), funit.debug_variables.data(), sizeof( code_debug_variable ) * funit.debug_variables.size() );
        memcpy( (code_debug_var_span*)d->var_spans(), funit.debug_var_spans.data(), sizeof( code_debug_var_span ) * funit.debug_var_spans.size() );

        f = (code_function*)( (const char*)f + f->code_size );
    }
    f->code_size = 0;

    memcpy( (char*)s->heap(), heap.data(), heap.size() );
    memcpy( (uint32_t*)s->debug_newlines(), debug_newlines.data(), sizeof( uint32_t ) * debug_newlines.size() );
    memcpy( (char*)s->debug_heap(), debug_heap.data(), debug_heap.size() );

    return s;
}

}

