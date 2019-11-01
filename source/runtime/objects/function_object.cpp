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
#include <vector>
#include <algorithm>

namespace kf
{

script_object* script_new( vm_context* vm, code_script* code )
{
    const char* name = code->debug_heap() + code->debug_script_name;
    script_object* script = new ( object_new( vm, SCRIPT_OBJECT, sizeof( script_object ) + sizeof( uint32_t ) * code->debug_newline_count + strlen( name ) ) ) script_object();
    script->name_size = strlen( name );
    script->newline_count = code->debug_newline_count;
    memcpy( (char*)( script->newlines + script->newline_count ), name, script->name_size );
    memcpy( script->newlines, code->debug_newlines(), sizeof( uint32_t ) * code->debug_newline_count );
    return script;
}

std::string_view script_name( vm_context* vm, script_object* script )
{
    const char* text = (const char*)( script->newlines + script->newline_count );
    return std::string_view( text, script->name_size );
}

static code_script* validate_code( void* data, size_t size )
{
    if ( size < sizeof( code_script ) )
        return nullptr;

    code_script* code = (code_script*)data;
    if ( code->magic != CODE_MAGIC )
        return nullptr;

    if ( size != code->code_size )
        return nullptr;

    return code;
}

program_object* program_new( vm_context* vm, void* data, size_t size )
{
    // Get code.
    code_script* code = validate_code( data, size );
    if ( ! code )
    {
        throw std::runtime_error( "invalid code" );
    }

    const char* heap = code->heap();
    const char* debug_heap = code->debug_heap();

    // Create script object.
    script_object* script = script_new( vm, code );

    // Create a program object for each function.
    std::vector< program_object* > programs;
    programs.reserve( code->function_count );

    for ( const code_function* cf = code->functions(); cf != nullptr; cf = cf->next() )
    {
        const code_debug_function* df = cf->debug_function();

        // Get name.
        const char* name = debug_heap + df->function_name;

        // Calculate size of program required.
        size_t op_count = ( cf->op_count + 1 ) & ~(size_t)1;
        size_t size = sizeof( program_object );
        size += sizeof( op ) * op_count;
        size += sizeof( ref_value ) * cf->constant_count;
        size += sizeof( key_selector ) * cf->selector_count;
        size += sizeof( ref< program_object > ) * cf->function_count;
        size += sizeof( uint32_t ) * cf->op_count;
        size += strlen( name );

        // Construct program.
        program_object* program = new ( object_new( vm, PROGRAM_OBJECT, size ) ) program_object();

        winit( program->script, script );
        program->name_size = strlen( name );
        program->op_count = cf->op_count;
        program->constant_count = cf->constant_count;
        program->selector_count = cf->selector_count;
        program->function_count = cf->function_count;
        program->outenv_count = cf->outenv_count;
        program->param_count = cf->param_count;
        program->stack_size = cf->stack_size;
        program->code_flags = cf->code_flags;

        program->constants = (ref_value*)( program->ops + program->op_count );
        program->selectors = (key_selector*)( program->constants + program->constant_count );
        program->functions = (ref< program_object >*)( program->selectors + program->selector_count );

        memcpy( program->ops, cf->ops(), sizeof( op ) * program->op_count );

        const code_constant* constants = cf->constants();
        for ( size_t i = 0; i < program->constant_count; ++i )
        {
            const code_constant& k = constants[ i ];
            if ( k.text == ~(uint32_t)0 )
            {
                winit( program->constants[ i ], box_number( k.n() ) );
            }
            else
            {
                winit( program->constants[ i ], box_string( string_new( vm, heap + k.text, k.size ) ) );
            }
        }

        const code_selector* selectors = cf->selectors();
        for ( size_t i = 0; i < program->selector_count; ++i )
        {
            const code_selector& s = selectors[ i ];
            key_selector* ksel = program->selectors + i;
            winit( ksel->key, string_key( vm, heap + s.text, s.size ) );
        }

        uint32_t* slocs = (uint32_t*)( program->functions + program->function_count );
        memcpy( slocs, df->slocs(), program->op_count );

        char* name_text = (char*)( slocs + program->op_count );
        memcpy( name_text, name, program->name_size );

        programs.push_back( program );
    }

    // Fixup program references.
    size_t index = 0;
    for ( const code_function* cf = code->functions(); cf != nullptr; cf = cf->next() )
    {
        program_object* program = programs.at( index++ );
        const uint32_t* functions = cf->functions();
        for ( size_t i = 0; i < program->function_count; ++i )
        {
            winit( program->functions[ i ], programs.at( functions[ i ] ) );
        }
    }

    // Return root program.
    assert( programs.size() );
    return programs.front();
}

std::string_view program_name( vm_context* vm, program_object* program )
{
    const char* text = (const char*)( (uint32_t*)( program->functions + program->function_count ) + program->op_count );
    return std::string_view( text, program->name_size );
}

source_location program_source_location( vm_context* vm, program_object* program, unsigned ip )
{
    // Get sloc.
    ip = std::min( ip, program->op_count - 1u );
    uint32_t sloc = ( (uint32_t*)( program->functions + program->function_count ) )[ ip ];

    // Search script for newline.
    script_object* script = read( program->script );
    auto i = std::upper_bound( script->newlines, script->newlines + script->newline_count, sloc );
    assert( i > script->newlines );
    return { (unsigned)( i - script->newlines ), (unsigned)( sloc - *( i - 1 ) + 1 ) };
}

function_object* function_new( vm_context* vm, program_object* program )
{
    function_object* function = new ( object_new( vm, FUNCTION_OBJECT, sizeof( function_object ) + sizeof( ref< vslots_object > ) * program->outenv_count ) ) function_object();
    winit( function->program, program );
    return function;
}

}

