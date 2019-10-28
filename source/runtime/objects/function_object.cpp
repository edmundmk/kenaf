//
//  function_object.cpp
//
//  Created by Edmund Kapusniak on 27/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "function_object.h"

namespace kf
{

script_object* script_new( vm_context* vm, code_script* code )
{
    const char* name = code->debug_heap() + code->debug_script_name;
    script_object* script = (script_object*)object_new( vm, SCRIPT_OBJECT, sizeof( script_object ) + sizeof( uint32_t ) * code->debug_newline_count + strlen( name ) );
    script->name_text = (const char*)( script->newlines + code->debug_newline_count );
    script->name_size = strlen( name );
    script->newline_count = code->debug_newline_count;
    memcpy( (char*)script->name_text, name, script->name_size );
    memcpy( script->newlines, code->debug_newlines(), sizeof( uint32_t ) * code->debug_newline_count );
    return script;
}

program_object* program_new( vm_context* vm, void* data, size_t size )
{
}

function_object* function_new( vm_context* vm, program_object* program )
{
}

cothread_object* cothread_new( vm_context* vm )
{
}

}

