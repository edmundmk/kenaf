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
    Functions, cothreads, and supporting objects.
*/

#include <vector>
#include "object_model.h"
#include "lookup_object.h"
#include "../../common/code.h"

namespace kf
{

/*
    Objects.
*/

struct code_object : public object
{
    uint16_t op_count;
    uint16_t constant_count;
    uint16_t selector_count;
    uint16_t function_count;
    uint8_t outenv_count;
    uint8_t param_count;
    uint8_t stack_size;
    uint8_t code_flags;

    op* ops() const;
    ref_value* constants() const;
    selector* selectors() const;
    ref< code_object >* functions() const;
};

struct function_object : public object
{
    ref< code_object > code;
    ref< vslots_object > outenvs[];
};

struct call_frame
{
};

struct cothread_object : public object
{
    std::vector< value > stack;
    std::vector< call_frame > call_frames;
};

/*
    Functions.
*/

code_object* code_new( vm_context* vm, void* code, size_t size );

}

#endif

