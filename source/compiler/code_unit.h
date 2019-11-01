//
//  code_unit.h
//
//  Created by Edmund Kapusniak on 19/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KF_CODE_UNIT_H
#define KF_CODE_UNIT_H

/*
    How we build the code description.
*/

#include <memory>
#include <vector>
#include "../common/code.h"

namespace kf
{

struct code_unit;
struct code_function_unit;

struct code_script_deleter { void operator () ( void* p ) const { free( p ); } };
typedef std::unique_ptr< code_script, code_script_deleter > code_script_ptr;

struct code_unit
{
    code_unit();
    ~code_unit();

    code_script script;
    std::vector< std::unique_ptr< code_function_unit > > functions;
    std::vector< char > heap;
    std::vector< uint32_t > debug_newlines;
    std::vector< char > debug_heap;

    code_script_ptr pack() const;
};

struct code_function_unit
{
    code_function_unit();
    ~code_function_unit();

    code_function function;
    std::vector< op > ops;
    std::vector< code_constant > constants;
    std::vector< code_selector > selectors;
    std::vector< uint32_t > functions;
    code_debug_function debug;
    std::vector< uint32_t > debug_slocs;
    std::vector< code_debug_variable > debug_variables;
    std::vector< code_debug_var_span > debug_var_spans;
};

}

#endif

