//
//  ir_regmap.h
//
//  Created by Edmund Kapusniak on 19/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef IR_REGMAP_H
#define IR_REGMAP_H

/*
    This data structure is used by the register allocation pass, and stores
    allocated live ranges for each register.  Currently we store a sorted list
    of allocated intervals for each register.
*/

#include <vector>

namespace kf
{

struct ir_value_range
{
    unsigned local_index;   // local index.
    unsigned lower;         // instruction where value becomes live (def or block start)
    unsigned upper;         // instruction where value dies
};

class ir_regmap
{
public:

    ir_regmap();
    ~ir_regmap();

    bool check( unsigned r, const ir_value_range* ranges, size_t rcount ) const;
    unsigned lowest( const ir_value_range* ranges, size_t rcount ) const;
    unsigned top( unsigned index ) const;

    void allocate( unsigned r, const ir_value_range* ranges, size_t rcount );
    void clear();

    void debug_print() const;

private:

    struct reg_range
    {
        unsigned index : 31;
        unsigned alloc;
    };

    typedef std::vector< reg_range > reg_range_list;
    std::vector< reg_range_list > _rr;

};

}

#endif

