//
//  live_ir.h
//
//  Created by Edmund Kapusniak on 14/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef LIVE_IR_H
#define LIVE_IR_H

/*
    Perform liveness analysis.  After this process, each op in the IR has
    'mark' set to the number of uses (saturating to 255), and live_range set
    to the index of the op in the block where the op dies (or the closing
    jump, if it survives the block).

    Additionally, live ranges for each local are computed.  Locals are the
    only values that can cross block boundaries.

    See this paper:
        http://hal.archives-ouvertes.fr/docs/00/55/85/09/PDF/RR-7503.pdf
*/

#include "ir.h"

namespace kf
{

class live_ir
{
public:

    explicit live_ir( source* source );
    ~live_ir();

    void live( ir_function* function );

private:

    void live_linear_pass();
    bool has_side_effects( ir_op* op );

    void live_dataflow_pass();


    source* _source;
    ir_function* _f;

};

}

#endif

