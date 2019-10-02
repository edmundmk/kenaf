//
//  icode.cpp
//
//  Created by Edmund Kapusniak on 02/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "icode.h"
#include <string.h>

namespace kf
{

icode_function::icode_function()
    :   ast( nullptr )
{
}

icode_function::~icode_function()
{
}

void icode_function::debug_print()
{
}

icode_oplist::icode_oplist()
    :   _ops( nullptr )
    ,   _body_size( 0 )
    ,   _head_size( 0 )
    ,   _watermark( 0 )
    ,   _capacity( 0 )
{
}

icode_oplist::~icode_oplist()
{
    free( _ops );
}

void icode_oplist::clear()
{
    _head_size = 0;
    _body_size = 0;
    _watermark = ( _capacity / 4 ) * 3;
}

unsigned icode_oplist::push_head( const icode_op& op )
{
    if ( _watermark + _head_size >= _capacity )
    {
        grow( false, true );
    }
    _ops[ _watermark + _head_size ] = op;
    return _head_size++;
}

unsigned icode_oplist::push_body( const icode_op& op )
{
    if ( _body_size >= _watermark )
    {
        grow( true, false );
    }
    _ops[ _body_size ] = op;
    return _body_size++;
}

void icode_oplist::grow( bool grow_body, bool grow_head )
{
    // Calculate updated sizes.
    size_t body_capacity = _watermark;
    size_t head_capacity = _capacity - _watermark;
    body_capacity = std::max< size_t >( body_capacity + ( grow_body ? body_capacity / 2 : 0 ), 8 );
    head_capacity = std::max< size_t >( head_capacity + ( grow_head ? head_capacity / 2 : 0 ), 8 );

    // Reallocate.
    _capacity = body_capacity + head_capacity;
    _ops = (icode_op*)realloc( _ops, _capacity );

    // Move head ops.
    memmove( _ops + _watermark, _ops + body_capacity, _head_size );
    _watermark = body_capacity;
}

icode_block::icode_block()
    :   loop_kind( IR_LOOP_NONE )
    ,   test_kind( IR_TEST_NONE )
    ,   block_index( IR_INVALID_INDEX )
    ,   function( nullptr )
    ,   loop( nullptr )
    ,   if_true( nullptr )
    ,   if_false( nullptr )
{
}

icode_block::~icode_block()
{
}

void icode_block::debug_print()
{
}

}

