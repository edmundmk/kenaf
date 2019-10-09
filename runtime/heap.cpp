//
//  heap.cpp
//
//  Created by Edmund Kapusniak on 02/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "heap.h"
#include <stdint.h>
#include <assert.h>
#include <mutex>
#include <new>

#include <sys/mman.h>

namespace kf
{

struct heap_state;
struct heap_segment;
struct heap_chunk;

const unsigned HEAP_INVALID_WORD = 0xDEFDEF;
const unsigned HEAP_FREE_WORD = 0xFEEFEE;

const size_t HEAP_MAX_SEGMENT = 0xFFFFFFE8;
const size_t HEAP_INITIAL_SIZE = 1024 * 1024;

/*
    Allocate and free virtual memory from the system.
*/

void* heap_vmalloc( size_t size )
{
    void* p = mmap( nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 );
    if ( p == MAP_FAILED )
    {
        throw std::bad_alloc();
    }
    return p;
}

void heap_vmfree( void* p, size_t size )
{
    munmap( p, size );
}

/*
    An allocated block looks like this:

        --> u32 size of chunk / 1 / P
            u32 word
        --> user data
            ...
        --- next chunk        / U / 1

    A small free block looks like this:

        --> u32 size of chunk / 0 / P
            u32 empty
        --> chunk* next
            chunk* prev
            ...
            u32 size of chunk
        --- next chunk        / U / 0

    A large free block looks like this:

        --> u32 size of chunk / 0 / P
            u32 empty
        --> chunk* next
            chunk* prev
            chunk* left
            chunk* right
            chunk* parent
            u32 bin_index
            ...
            u32 size of chunk
        --- next chunk        / U / 0

    As in dlmalloc, the P bit indicates whether or not the previous chunk is
    allocated.  The U bit indicates whether or not the current chunk is
    allocated.

    Blocks are 8-byte aligned with a minimum size of 32 bytes, and a maximum
    size of just under 4GiB.
*/

struct alignas( 8 ) heap_chunk_header
{
    uint32_t p : 1;
    uint32_t u : 1;
    uint32_t size : 30;
    uint32_t word;
};

struct heap_chunk
{
    heap_chunk_header header;

    // Used only for free blocks.
    heap_chunk* next;
    heap_chunk* prev;

    // Used only for large blocks.
    heap_chunk* left;
    heap_chunk* right;
    heap_chunk* parent;
    uint32_t bin_index;
};

struct heap_chunk_footer
{
    uint32_t size;
};

inline heap_chunk* heap_chunk_head( void* p )
{
    return (heap_chunk*)( (heap_chunk_header*)p - 1 );
}

inline void* heap_chunk_data( heap_chunk* p )
{
    return p + 1;
}

inline heap_chunk_footer* heap_chunk_this_footer( heap_chunk* chunk )
{
    assert( ! chunk->header.u );
    return (heap_chunk_footer*)( (char*)chunk + chunk->header.size ) - 1;
}

inline heap_chunk_footer* heap_chunk_prev_footer( heap_chunk* chunk )
{
    assert( ! chunk->header.p );
    return (heap_chunk_footer*)chunk - 1;
}

inline heap_chunk* heap_chunk_prev( heap_chunk* chunk )
{
    return (heap_chunk*)( (char*)chunk - heap_chunk_prev_footer( chunk )->size );
}

inline heap_chunk* heap_chunk_next( heap_chunk* chunk )
{
    return (heap_chunk*)( (char*)chunk + chunk->header.size );
}

/*
    An entry in a linked list of memory segments allocated from the system.

    The initial segment looks like this:

        --- heap_state
        --- heap_chunk   : u32 size / U / 1
            ...
        --- heap_segment : 0 / 1 / P

    Subsequently allocated segments look like this:

        --- heap_chunk    : u32 size / U / 1
            ...
        --- heap_segment : 0 / 1 / P

    The heap_segment structure is always placed at the end of the segment, and
    is treated as an allocated chunk. This means we can guarantee that valid
    chunks are always followed by an allocated chunk.

    The heap_segment chunk has a size of zero, to indicate that it is the end
    of the segment.
*/

struct heap_segment
{
    heap_chunk_header header;
    void* base;
    heap_segment* next;
};

inline size_t heap_segment_size( heap_segment* segment )
{
    return (char*)( segment + 1 ) - (char*)segment->base;
}

/*
    Small bin sizes, == ( index + 1 ) * 8
        [ 0 ] -> 8
        [ 1 ] -> 16
        [ 2 ] -> 24
        [ 3 ] -> 32
        [ 4 ] -> 40
        ...
        [ 30 ] -> 240
        [ 31 ] -> 248

    Each small bin is a list of free chunks of exactly the given size.

    Large bin sizes, >= ( 256 << index/2 ) + index%2 ? 128 << index/2 : 0
        [ 0 ] -> >= 256
        [ 1 ] -> >= 384
        [ 2 ] -> >= 512
        [ 3 ] -> >= 768
        [ 4 ] -> >= 1024
        ...
        [ 30 ] -> >= 8MiB
        [ 31 ] -> >= 12MiB

    Each large bin is a binary tree keyed by .  The nodes of the tree are lists of chunks with
    the same size.
*/

const size_t HEAP_SMALLBIN_COUNT = 32;
const size_t HEAP_LARGEBIN_COUNT = 32;
const size_t HEAP_LARGEBIN_SIZE = 256;

inline unsigned clz( unsigned x )
{
    return __builtin_clz( x );
}

inline size_t heap_smallbin_index( size_t size )
{
    assert( size < HEAP_LARGEBIN_SIZE );
    return size / 8;
}

inline size_t heap_largebin_index( size_t size )
{
    if ( size < 256 )
    {
        return 0;
    }
    else if ( size < ( 12 << 20 ) )
    {
        unsigned log2size = sizeof( unsigned ) * CHAR_BIT - 1 * clz( (unsigned)size );
        size_t index = ( log2size - 8 ) * 2;
        size_t ihalf = size >> ( ( log2size - 1 ) & 1 );
        return index + ihalf;
    }
    else
    {
        return 31;
    }
}

/*
    Main heap data structure, at the end of the initial segment.  Note that
    smallbin_anchors are the sentinel nodes in doubly-linked lists of blocks.
    However, we only store the next and prev pointers.
*/

struct heap_state
{
    explicit heap_state( size_t segment_size );
    ~heap_state();

    uint32_t smallbin_map;
    uint32_t largebin_map;
    uint32_t victim_size;
    heap_segment* initial_segment;
    heap_chunk* victim;
    heap_chunk* smallbin_anchors[ HEAP_SMALLBIN_COUNT * 2 ];
    heap_chunk* largebins[ HEAP_LARGEBIN_COUNT ];
    std::mutex mutex;

    void* malloc( size_t size );
    void free( void* p );

    heap_chunk* smallbin_anchor( size_t i );
    void insert_chunk( size_t size, heap_chunk* chunk );
    void remove_chunk( size_t size, heap_chunk* chunk );
    void free_segment( heap_segment* segment );
};

heap_state::heap_state( size_t segment_size )
    :   smallbin_map( 0 )
    ,   largebin_map( 0 )
    ,   victim_size( 0 )
    ,   victim( nullptr )
    ,   smallbin_anchors{}
    ,   largebins{}
{
    // Initialize anchors.
    for ( size_t i = 0; i < HEAP_SMALLBIN_COUNT; ++i )
    {
        heap_chunk* anchor = smallbin_anchor( i );
        anchor->next = anchor;
        anchor->prev = anchor;
    }

    /*
        Initial segment looks like this:

            heap_state
            heap_chunk free_chunk
            heap_segment
    */
    assert( segment_size >= sizeof( heap_state ) + sizeof( heap_segment ) );
    heap_chunk* free_chunk = (heap_chunk*)( this + 1 );
    heap_chunk* segment_chunk = (heap_chunk*)( (char*)this + segment_size - sizeof( heap_segment ) );

    if ( free_chunk < segment_chunk )
    {
        size_t size = segment_size - sizeof( heap_state ) - sizeof( heap_segment );
        assert( size < HEAP_MAX_SEGMENT );
        free_chunk->header = { true, false, (uint32_t)size, HEAP_FREE_WORD };
        heap_chunk_this_footer( free_chunk )->size = size;
        insert_chunk( size, free_chunk );
    }

    segment_chunk->header = { false, true, 0, HEAP_INVALID_WORD };
    initial_segment = (heap_segment*)segment_chunk;
    initial_segment->base = this;
    initial_segment->next = nullptr;
}

heap_state::~heap_state()
{
    heap_segment* s = initial_segment;
    while ( s )
    {
        heap_segment* next = s->next;
        heap_vmfree( s->base, heap_segment_size( s ) );
        s = next;
    }
}

void* heap_state::malloc( size_t size )
{
    std::lock_guard< std::mutex > lock( mutex );
    // TODO.
    return nullptr;
}

void heap_state::free( void* p )
{
    // free( nullptr ) is valid.
    if ( ! p )
    {
        return;
    }

    // Lock.
    std::lock_guard< std::mutex > lock( mutex );

    // We don't have much context, but assert that the chunk is allocated.
    heap_chunk* chunk = heap_chunk_head( p );
    assert( chunk->header.u );
    uint32_t size = chunk->header.size;

    // Attempt to merge with previous chunk.
    if ( ! chunk->header.p )
    {
        heap_chunk* prev = heap_chunk_prev( chunk );
        assert( prev->header.p );
        assert( ! prev->header.u );
        uint32_t prev_size = prev->header.size;
        remove_chunk( prev_size, prev );
        size += prev_size;
        chunk = prev;
    }

    // Attempt to merge with following chunk.
    heap_chunk* next = (heap_chunk*)( (char*)chunk + size );
    if ( ! next->header.u )
    {
        assert( next->header.p );
        size_t next_size = next->header.size;
        remove_chunk( next_size, next );
        size += next_size;
        next = (heap_chunk*)( (char*)chunk + size );;
    }

    if ( next->header.size != 0 || chunk != ( (heap_segment*)next )->base )
    {
        // This block is free.
        chunk->header = { false, true, size, HEAP_FREE_WORD };
        heap_chunk_this_footer( chunk )->size = size;
        insert_chunk( size, chunk );
    }
    else
    {
        // This chunk spans the entire segment, so free the segment.
        free_segment( (heap_segment*)next );
    }
}

heap_chunk* heap_state::smallbin_anchor( size_t i )
{
    assert( i < HEAP_SMALLBIN_COUNT );
    return heap_chunk_head( &smallbin_anchors[ i * 2 ] );
}

void heap_state::insert_chunk( size_t size, heap_chunk* chunk )
{
    if ( size < HEAP_LARGEBIN_SIZE )
    {
        size_t index = heap_smallbin_index( size );

        // Insert at head of smallbin list of this size.
        heap_chunk* prev = smallbin_anchor( index );
        heap_chunk* next = prev->next;
        prev->next = chunk;
        next->prev = chunk;
        chunk->next = next;
        chunk->prev = prev;

        // Mark smallbin map, as this smallbin is not empty.
        largebin_map |= 1u << index;
    }
    else
    {
        size_t index = heap_largebin_index( size );

        //

    }
}

void heap_state::remove_chunk( size_t size, heap_chunk* chunk )
{
}

void heap_state::free_segment( heap_segment* segment )
{
    // Unlink from list.
    heap_segment* s = initial_segment;
    heap_segment** pback = &initial_segment;
    while ( s )
    {
        if ( s == segment )
            break;
        pback = &s->next;
        s = s->next;
    }

    assert( s );
    assert( *pback != initial_segment );
    *pback = s->next;

    // Free memory.
    heap_vmfree( segment->base, heap_segment_size( segment ) );
}

/*
    Heap interface.
*/

heap::heap()
{
    _state = new ( heap_vmalloc( HEAP_INITIAL_SIZE ) ) heap_state( HEAP_INITIAL_SIZE );
}

heap::~heap()
{
    _state->~heap_state();
}

void* heap::malloc( size_t size )
{
    return _state->malloc( size );
}

void heap::free( void* p )
{
    _state->free( p );
}

/*
    Get size of allocated block.
*/

size_t heap_malloc_size( void* p )
{
    return heap_chunk_head( p )->header.size;
}

}
