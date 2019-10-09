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
const size_t HEAP_INITIAL_SIZE = 1024 * 1024;
const size_t HEAP_VM_GRANULARITY = 1024 * 1024;

/*
    Count leading and trailing zeroes.
*/

inline uint32_t clz( uint32_t x )
{
    return __builtin_clz( x );
}

inline uint32_t ctz( uint32_t x )
{
    return __builtin_ctz( x );
}

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
          & user data
            ...
        --- next chunk        / U / 1

    A small free block looks like this:

        --> u32 size of chunk / 0 / P
            u32 empty
          & chunk* next
            chunk* prev
            ...
            u32 size of chunk
        --- next chunk        / U / 0

    A large free block looks like this:

        --> u32 size of chunk / 0 / P
            u32 empty
          & chunk* next
            chunk* prev
            chunk* left
            chunk* right
            chunk* parent
            ...
            u32 size of chunk
        --- next chunk        / U / 0

    As in dlmalloc, the P bit indicates whether or not the previous chunk is
    allocated.  The U bit indicates whether or not the current chunk is
    allocated.

    Chunks are 8-byte aligned with a maximum size of just under 4GiB.
*/

struct heap_chunk_header
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

    // Used only for large free blocks.
    heap_chunk* parent;
    heap_chunk* child[ 2 ];
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

    Each large bin is a binary tree.  The nodes of the tree are lists of chunks
    with the same size.  Each tree is a 'not quite' prefix tree/bitwise trie
    keyed on the low bits of the chunk size.

    Let 'base' be the minimum size of chunk stored in this bin (the large bin
    size, above).  The sizes of chunks in each bin are in the interval
    [ base, base + range ), where range is a power of two.  For example:

        bin 4
            base    0b0100_0000_0000    range 9 bits
            last    0b0101_1111_1111
        bin 5
            base    0b0110_0000_0000    range 9 bits
            last    0b0111_1111_1111
        bin 6
            base    0b1000_0000_0000    range 10 bits
            last    0b1011_1111_1111

    Each node in the tree selects between its children using a bit from the
    range, with the root selecting using the high bit and subsequent levels
    using the next highest bit.  However, instead of the linked list for each
    chunk size being stored in leaf nodes, each leaf of the tree is stored in
    one of its ancestors (it doesn't matter which one, it will depend on
    insertion order).

    For example, bin 1 has a range of 7 bits, and a bitwise trie might look
    like (with some chains of parent nodes elided):

                                      [..]
                        .---------------'---------------.
                      [0..]                           [1..]
                .-------'-------.               .-------'-------.
             [00..]          [01..]             .            [11..]
           .----'----.          '----.       1010110       .----'
           .         .               .                     .
        0001111    0010110        0110110               1100111

    But our tree might instead look like this:

                                   [..] 1010110
                            .-----------'----------.
                      [0..] 0001111          [1..] 1100111
                    .-------'-------.
             [00..] 0010110  [01..] 0110110

*/

const size_t HEAP_SMALLBIN_COUNT = 32;
const size_t HEAP_LARGEBIN_COUNT = 32;
const size_t HEAP_LARGEBIN_SIZE = 256;

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
        size_t log2size = sizeof( uint32_t ) * CHAR_BIT - 1 * clz( (uint32_t)size );
        size_t index = ( log2size - 8 ) * 2;
        size_t ihalf = size >> ( ( log2size - 1 ) & 1 );
        return index + ihalf;
    }
    else
    {
        return 31;
    }
}

inline uint32_t heap_largebin_prefix( size_t size, size_t index )
{
    // Shift bits of size so most significant bit is the top bit of the range
    // for the bin at index.
    uint32_t range_bits = 7 + index / 2;
    return (uint32_t)size << ( 32 - range_bits );
}

/*
    Main heap data structure, at the end of the initial segment.  Note that
    smallbin_anchors are the sentinel nodes in doubly-linked lists of blocks.
    However, we only store the next and prev pointers.
*/

const size_t HEAP_MAX_CHUNK_SIZE = 0xFFFFFFE0;
const size_t HEAP_CHUNK_ALIGNMENT = 8;

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
        assert( size <= HEAP_MAX_CHUNK_SIZE );
        free_chunk->header = { true, false, (uint32_t)size, HEAP_FREE_WORD };
        heap_chunk_this_footer( free_chunk )->size = size;
        victim_size = size;
        victim = free_chunk;
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
    // Chunk size is larger due to overhead, and must be aligned.
    size = size + sizeof( heap_chunk_header );
    size = ( size + HEAP_CHUNK_ALIGNMENT - 1 ) & ~( HEAP_CHUNK_ALIGNMENT - 1 );
    if ( size > HEAP_MAX_CHUNK_SIZE )
    {
        throw std::bad_alloc();
    }

    // Lock.
    std::lock_guard< std::mutex > lock( mutex );

    heap_chunk* chunk = nullptr;
    if ( size < HEAP_LARGEBIN_SIZE )
    {
        // Small chunk.
        size_t index = heap_smallbin_index( size );
        uint32_t bin_map = smallbin_map >> index;

        if ( bin_map & 0xF )
        {
            /*
                Use the entirety of a chunk in the smallbin of a size where
                the remaining size after we split is too small to hold a free
                chunk. Free chunks on 64-bit systems take up 32 bytes/4 bins.
            */
            unsigned waste = ctz( bin_map );
            size += waste * 8;
            index += waste;

            assert( map & 1u << index );
            heap_chunk* anchor = smallbin_anchor( index );
            chunk = anchor->next;

            heap_chunk* next = chunk->next;
            anchor->next = next;
            next->prev = anchor;
        }
        else
        {
            if ( victim_size < size )
            {
                insert_chunk( victim_size, victim );

                if ( bin_map )
                {
                    // Replace victim with first non-empty smallbin.
                }
                else if ( largebin_map )
                {
                    // Replace victim with smallest non-empty largebin.
                }
                else
                {
                    // Make new VM allocation to create a new victim.
                }
            }

            // Split victim chunk.

        }
    }
    else
    {
        // Large chunk.

        // Find best fitting binned chunk.

        if ( 32 < victim_size )
        {
            // Use best fitting chunk.
        }
        else
        {
            // Use victim chunk.
        }

        // Split target chunk.
    }


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
        assert( next->header.u );
        next->header.p = false;
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
        smallbin_map |= 1u << index;
    }
    else
    {
        size_t index = heap_largebin_index( size );
        uint32_t prefix = heap_largebin_prefix( size, index );

        // Set tree node properties.
        chunk->child[ 0 ] = nullptr;
        chunk->child[ 1 ] = nullptr;

        // Link into tree.
        assert( index < HEAP_LARGEBIN_COUNT );
        heap_chunk* parent = chunk;
        heap_chunk** link = &largebins[ index ];
        heap_chunk* node = *link;
        while ( true )
        {
            if ( ! node )
            {
                // Link new node into tree.
                *link = chunk;
                chunk->next = chunk;
                chunk->prev = chunk;
                chunk->parent = parent;
                break;
            }

            if ( node->header.size == size )
            {
                // Link new node into the linked list at this tree node.
                heap_chunk* next = node->next;
                node->next = chunk;
                next->prev = chunk;
                chunk->next = next;
                chunk->prev = node;
                chunk->parent = nullptr;
                break;
            }

            parent = node;
            link = &node->child[ prefix >> 31 ];
            prefix >>= 1;
            node = *link;
        }

        // Mark largebin map, as this largebin is not empty.
        largebin_map |= 1u << index;
    }
}

inline heap_chunk* heap_tree_rightwards( heap_chunk* chunk )
{
    heap_chunk* right = chunk->child[ 1 ];
    return right ? right : chunk->child[ 0 ];
}

void heap_state::remove_chunk( size_t size, heap_chunk* chunk )
{
    heap_chunk* prev = chunk->prev;
    heap_chunk* next = chunk->next;

    if ( size < HEAP_LARGEBIN_SIZE )
    {
        // Unlink from list.
        prev->next = next;
        next->prev = prev;

        // Check if this bin is empty.
        if ( prev == next )
        {
            size_t index = heap_smallbin_index( size );
            assert( prev == smallbin_anchor( index ) );
            smallbin_map &= ~( 1u << index );
        }
    }
    else
    {
        heap_chunk* replace = nullptr;

        if ( prev != next )
        {
            // Chunk is part of a list.  Unlink it, and replace with next.
            prev->next = next;
            next->prev = prev;
            replace = next;
        }
        else
        {
            // Choose rightmost leaf as replacment.
            replace = heap_tree_rightwards( chunk );
            while ( heap_chunk* right = heap_tree_rightwards( replace ) )
            {
                replace = right;
            }
        }

        // If this chunk was a tree node, then replace it.
        heap_chunk* parent = chunk->parent;
        if ( parent )
        {
            if ( parent->child[ 0 ] == chunk ) parent->child[ 0 ] = replace;
            if ( parent->child[ 1 ] == chunk ) parent->child[ 1 ] = replace;
            replace->parent = parent;
        }
    }
}

void heap_state::free_segment( heap_segment* segment )
{
    // Unlink from list.
    heap_segment* s = initial_segment;
    heap_segment** pback = &initial_segment;
    while ( s )
    {
        if ( s == segment ) break;
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
    heap_chunk* chunk = heap_chunk_head( p );
    assert( chunk->header.u );
    return chunk->header.size - sizeof( heap_chunk_header );
}

}

