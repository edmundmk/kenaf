//
//  function_object.h
//
//  Created by Edmund Kapusniak on 27/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KF_FUNCTION_OBJECT_H
#define KF_FUNCTION_OBJECT_H

/*
    Functions and supporting objects.
*/

#include <string_view>
#include "object_model.h"
#include "lookup_object.h"
#include "../../common/code.h"

namespace kf
{

/*
    Utility.
*/

struct source_location
{
    unsigned line;
    unsigned column;
};

struct key_selector
{
    ref< string_object > key;
    selector sel;
};

/*
    Objects.
*/

struct script_object : public object
{
    uint32_t name_size;
    uint32_t newline_count;
    uint32_t newlines[];
};

struct program_object : public object
{
    ref_value* constants;
    key_selector* selectors;
    ref< program_object >* functions;
    ref< script_object > script;
    uint32_t name_size;
    uint16_t op_count;
    uint16_t constant_count;
    uint16_t selector_count;
    uint16_t function_count;
    uint8_t outenv_count;
    uint8_t param_count;
    uint8_t stack_size;
    uint8_t code_flags;
    op ops[];
};

struct function_object : public object
{
    ref< program_object > program;
    ref< vslots_object > outenvs[];
};

/*
    Functions.
*/

script_object* script_new( vm_context* vm, code_script* code );
std::string_view script_name( vm_context* vm, script_object* script );

program_object* program_new( vm_context* vm, void* code, size_t size );
std::string_view program_name( vm_context* vm, program_object* program );
source_location program_source_location( vm_context* vm, program_object* program, unsigned ip );

function_object* function_new( vm_context* vm, program_object* program );

}

#endif

