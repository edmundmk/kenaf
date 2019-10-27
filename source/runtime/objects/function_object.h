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

*/

#include "object_model.h"
#include "lookup_object.h"

namespace kf
{

/*

*/

struct code_object : public object
{

    ref< code_object > code[];
};

struct function_object : public object
{
    ref< code_object > code;
    ref< vslots_object > outenvs[];
};

struct cothread_object : public object
{

};

/*
    Functions.
*/

code_object* code_new( vm_context* vm, void* code, size_t size );

}

#endif

