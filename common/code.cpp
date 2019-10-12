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

}

