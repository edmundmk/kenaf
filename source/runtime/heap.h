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
      - Each allocated block has a 4-byte header just before it.

    Based on Doug Lea's dlmalloc.  http://gee.cs.oswego.edu/dl/html/malloc.html

    The allocator is not synchronized internally.  Access from multiple threads
    must be synchronized by the user.
*/

#include <stddef.h>

namespace kf
{

struct heap_state;

heap_state* heap_create();
void heap_destroy( heap_state* heap );

/*
    Malloc/free interface.
*/

void* heap_malloc( heap_state* heap, size_t size );
size_t heap_malloc_size( void* p );
void heap_free( heap_state* heap, void* p );

/*
    Sweeping.  Visits every allocated block in the heap.  Do not free any
    blocks while a sweep is active.  p is a pointer to the allocated block.
*/

void* heap_sweep( heap_state* heap, void* p, bool free_chunk );

/*
    Debugging.
*/

void debug_print( heap_state* heap );

}

#endif

