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
#include "../vmachine.h"
#include "lookup_object.h"
#include "../../common/code.h"

namespace kf
{

/*
    Source location.
*/

struct source_location
{
    unsigned line;
    unsigned column;
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
    ref< lookup_object > omethod;
    ref< vslots_object > outenvs[];
};

struct native_function_object : public object
{
    native_function native;
    void* cookie;
    unsigned param_count;
    unsigned code_flags;
    unsigned name_size;
    char name_text[];
};

/*
    Functions.
*/

script_object* script_new( vmachine* vm, const code_script* code );
std::string_view script_name( vmachine* vm, script_object* script );
program_object* program_new( vmachine* vm, const void* data, size_t size );
std::string_view program_name( vmachine* vm, program_object* program );
source_location program_source_location( vmachine* vm, program_object* program, unsigned ip );
function_object* function_new( vmachine* vm, program_object* program );
native_function_object* native_function_new( vmachine* vm, std::string_view name, native_function native, void* cookie, unsigned param_count, unsigned code_flags );
std::string_view native_function_name( vmachine* vm, native_function_object* function );

}

#endif

