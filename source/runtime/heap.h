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

heap_state* heap_create();
void heap_destroy( heap_state* heap );

void* heap_malloc( heap_state* heap, size_t size );
size_t heap_malloc_size( void* p );
void heap_free( heap_state* heap, void* p );

void debug_print( heap_state* heap );

}

#endif

