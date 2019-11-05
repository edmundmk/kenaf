//
//  ir_foldk.h
//
//  Created by Edmund Kapusniak on 15/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KF_IR_FOLDK_H
#define KF_IR_FOLDK_H

/*
    Builds final constant tables and inlines constant operands into
    instruction forms that support constant operands.
*/

#include "ir.h"
#include <unordered_map>

namespace kf
{

class ir_foldk
{
public:

    ir_foldk( source* source );
    ~ir_foldk();

    bool foldk( ir_function* function );

private:

    void inline_operands();
    void alloc_constants();

    ir_operand b_immediate( ir_operand operand );

    ir_operand insert_number( ir_operand operand );
    ir_operand insert_string( ir_operand operand );
    ir_operand insert_selector( ir_operand operand );
    ir_operand insert_function( ir_operand operand );

    source* _source;
    ir_function* _f;
    std::vector< ir_constant > _constants;
    std::vector< ir_selector > _selectors;
    std::unordered_map< uint64_t, unsigned > _number_map;
    std::unordered_map< std::string_view, unsigned > _string_map;
    std::unordered_map< std::string_view, unsigned > _selector_map;

};

}

#endif

