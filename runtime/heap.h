//
//  heap.h
//
//  Created by Edmund Kapusniak on 02/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef HEAP_H
#define HEAP_H

/*
    A memory allocator with the following interface:

      - New blocks can be allocated.
      - A heap walker can walk through each block, optionally freeing blocks.

    Based on Doug Lea's dlmalloc.  http://gee.cs.oswego.edu/dl/html/malloc.html
*/

#include <stddef.h>

namespace kf
{

struct heap_state;

class heap
{
public:

    heap();
    ~heap();

    void* malloc( size_t size );
    void free( void* p );

private:

    heap_state* _state;

};

size_t heap_malloc_size( void* p );

}

#endif

