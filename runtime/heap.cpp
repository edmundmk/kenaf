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
#include <limits.h>
#include <assert.h>
#include <mutex>
#include <new>

#include <sys/mman.h>

namespace kf
{

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

const size_t HEAP_INITIAL_SIZE = 1024 * 1024;
const size_t HEAP_VM_GRANULARITY = 1024 * 1024;

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
    An allocated chunk looks like this:

        --> u32 size of chunk / 1 / P
            u32 word
          & ...
        --- next chunk        / U / 1

    A free chunk which is too small to link into a bin (i.e. smaller than a
    header + three pointers) looks like this:

        --> u32 size of chunk / 0 / P
            ...
            u32 size of chunk
        --> next chunk        / U / 0

    For the very tiniest chunks, this is the same size as a header on its own.
    These tiny free chunks are not binned, and the memory is unrecoverable
    until adjacent allocations are merged.

    A small free chunk looks like this:

        --> u32 size of chunk / 0 / P
            u32 ...
          & chunk* next
            chunk* prev
            ...
            u32 size of chunk
        --- next chunk        / U / 0

    A large free chunk looks like this:

        --> u32 size of chunk / 0 / P
            u32 ...
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

const size_t HEAP_CHUNK_ALIGNMENT = 8;
const size_t HEAP_MIN_BINNED_SIZE = 8 + sizeof( void* ) * 3;

const uint32_t HEAP_WORD_INTERNAL = 0xDEFDEF;
const uint32_t HEAP_WORD_FREE = 0xFEEFEE;

struct heap_chunk_header
{
    uint32_t p_u_size;
    uint32_t word;

    heap_chunk_header( bool p, bool u, size_t size, uint32_t word );

    size_t size() const;
    bool p() const;
    bool u() const;
    void set_p();
    void clear_p();
};

inline heap_chunk_header::heap_chunk_header( bool p, bool u, size_t size, uint32_t word )
    :   p_u_size( (uint32_t)size |( u ? 2 : 0 )|( p ? 1 : 0 ) )
    ,   word( word )
{
    assert( ( size & 3 ) == 0 );
    assert( this->size() == size );
}

inline size_t heap_chunk_header::size() const
{
    return p_u_size & ~3;
}

inline bool heap_chunk_header::p() const
{
    return p_u_size & 1;
}

inline bool heap_chunk_header::u() const
{
    return p_u_size & 2;
}

inline void heap_chunk_header::set_p()
{
    p_u_size |= 1;
}

inline void heap_chunk_header::clear_p()
{
    p_u_size &= ~(uint32_t)1;
}

struct heap_chunk
{
    heap_chunk_header header;

    // Used for free chunks.
    heap_chunk* next;
    heap_chunk* prev;

    // Used for large free chunks.
    heap_chunk* parent;
    heap_chunk* child[ 2 ];
    size_t index;
};

struct heap_chunk_footer
{
    uint32_t size;
};

/*
    Navigating chunks in memory.
*/

inline heap_chunk* heap_chunk_head( void* p )
{
    return (heap_chunk*)( (heap_chunk_header*)p - 1 );
}

inline void* heap_chunk_data( heap_chunk* p )
{
    return &p->header + 1;
}

inline heap_chunk_footer* heap_chunk_prev_footer( heap_chunk* chunk )
{
    assert( ! chunk->header.p() );
    return (heap_chunk_footer*)chunk - 1;
}

inline heap_chunk* heap_chunk_prev( heap_chunk* chunk )
{
    return (heap_chunk*)( (char*)chunk - heap_chunk_prev_footer( chunk )->size );
}

inline heap_chunk* heap_chunk_next( heap_chunk* chunk, size_t size )
{
    return (heap_chunk*)( (char*)chunk + size );
}

/*
    Setting chunk headers and footers.
*/

inline void heap_chunk_set_free( heap_chunk* chunk, size_t size )
{
    assert( size >= 8 );
    chunk->header = { true, false, size, HEAP_WORD_FREE };
    heap_chunk_footer* footer = (heap_chunk_footer*)( (char*)chunk + size ) - 1;
    footer->size = size;
}

inline void heap_chunk_set_allocated( heap_chunk* chunk, size_t size )
{
    chunk->header = { true, true, size, 0 };
}

inline void heap_chunk_set_segment( heap_chunk* chunk )
{
    chunk->header = { false, true, 0, HEAP_WORD_INTERNAL };
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

const size_t HEAP_MAX_CHUNK_SIZE = 0xFFFFFFE0;

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

inline bool heap_segment_can_merge( heap_segment* prev, heap_segment* next )
{
    if ( prev + 1 != next->base )
        return false;

    size_t total_size = heap_segment_size( prev ) + heap_segment_size( next );
    return total_size - sizeof( heap_segment ) <= HEAP_MAX_CHUNK_SIZE;
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

    Large bins are trees stored in a specific way that minimises insertion
    cost while allowing us to quickly locate best fit chunks.
*/

const size_t HEAP_SMALLBIN_COUNT = 32;
const size_t HEAP_LARGEBIN_COUNT = 32;
const size_t HEAP_LARGE_SIZE = 256;

inline size_t heap_smallbin_index( size_t size )
{
    assert( size < HEAP_LARGE_SIZE );
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
        size_t log2size = sizeof( uint32_t ) * CHAR_BIT - 1 - clz( (uint32_t)size );
        size_t index = ( log2size - 8 ) * 2;
        size_t ihalf = ( size >> ( log2size - 1 ) ) & 1;
        return index + ihalf;
    }
    else
    {
        return 31;
    }
}

/*
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

    Since tree operations are complex, they are split out here so they can be
    tested indepedently.
*/

struct heap_largebin
{
    heap_chunk* root;

    void insert( size_t index, size_t size, heap_chunk* chunk );
    bool remove( size_t index, heap_chunk* chunk );
    heap_chunk* smallest( size_t index );
    heap_chunk* best_fit( size_t index, size_t size );

    uint32_t trie_prefix( size_t index, size_t size );
    heap_chunk* leftwards( heap_chunk* chunk );
    heap_chunk* rightwards( heap_chunk* chunk );

    void debug_print( size_t index );
    void debug_print( size_t index, int level, uint32_t prefix, heap_chunk* chunk );
};

void heap_largebin::insert( size_t index, size_t size, heap_chunk* chunk )
{
    assert( heap_largebin_index( size ) == index );
    uint32_t prefix = trie_prefix( index, size );

    // Set tree node properties.
    chunk->child[ 0 ] = nullptr;
    chunk->child[ 1 ] = nullptr;
    chunk->index = index;

    // Link into tree.
    heap_chunk* parent = chunk;
    heap_chunk** link = &root;
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

        if ( node->header.size() == size )
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
        prefix <<= 1;
        node = *link;
    }
}

bool heap_largebin::remove( size_t index, heap_chunk* chunk )
{
    assert( chunk->index == index );
    assert( heap_largebin_index( chunk->header.size() ) == index );

    heap_chunk* replace = nullptr;
    heap_chunk* prev = chunk->prev;
    heap_chunk* next = chunk->next;
    heap_chunk* parent = chunk->parent;

    if ( next != chunk )
    {
        // Chunk is part of a list.  Unlink it.
        prev->next = next;
        next->prev = prev;

        // If original chunk wasn't a tree node, then we're done.
        if ( ! parent )
        {
            return true;
        }

        // Otherwise replace the node with the next one in the list.
        replace = next;
    }
    else
    {
        assert( parent );
        replace = rightwards( chunk );

        if ( replace )
        {
            // Search for rightmost leaf.
            heap_chunk* replace_parent = chunk;
            while ( heap_chunk* right = rightwards( replace ) )
            {
                replace_parent = replace;
                replace = right;
            }

            // Unlink replacement node from its current position in the tree.
            assert( ! replace->child[ 0 ] && ! replace->child[ 1 ] );
            assert( replace->parent == replace_parent );
            if ( replace_parent->child[ 0 ] == replace ) replace_parent->child[ 0 ] = nullptr;
            if ( replace_parent->child[ 1 ] == replace ) replace_parent->child[ 1 ] = nullptr;
        }
    }

    if ( parent != chunk )
    {
        // Replacing a non-root node.
        if ( parent->child[ 0 ] == chunk ) parent->child[ 0 ] = replace;
        if ( parent->child[ 1 ] == chunk ) parent->child[ 1 ] = replace;
        if ( replace ) replace->parent = parent;
        return true;
    }
    else
    {
        // Replacing the root node of the tree.
        assert( root == chunk );
        root = replace;
        if ( replace )
        {
            // Mark as root by linking it back to itself.
            replace->parent = replace;
            return true;
        }
        else
        {
            // Bin is now empty.
            return false;
        }
    }
}

heap_chunk* heap_largebin::smallest( size_t index )
{
    assert( root );

    heap_chunk* chunk = root;
    size_t chunk_size = chunk->header.size();
    assert( heap_largebin_index( chunk_size ) == index );

    heap_chunk* tree = leftwards( chunk );
    while ( tree )
    {
        size_t tree_size = tree->header.size();
        if ( tree_size < chunk_size )
        {
            chunk = tree->next;
            chunk_size = tree_size;
        }

        tree = leftwards( tree );
    }

    return chunk;
}

heap_chunk* heap_largebin::best_fit( size_t index, size_t size )
{
    assert( heap_largebin_index( size ) == index );
    assert( root );

    heap_chunk* chunk = nullptr;
    size_t chunk_size = SIZE_MAX;

    // Search down tree limited by the size we're looking for.
    heap_chunk* right_tree = nullptr;
    heap_chunk* tree = root;
    uint32_t prefix = trie_prefix( index, size );
    while ( true )
    {
        size_t tree_size = tree->header.size();
        if ( size <= tree_size && tree_size < chunk_size )
        {
            chunk = tree->next;
            chunk_size = tree_size;
            if ( chunk_size == size )
            {
                break;
            }
        }

        heap_chunk* right = tree->child[ 1 ];
        tree = tree->child[ prefix >> 31 ];
        prefix <<= 1;

        if ( right && right != tree )
        {
            right_tree = right;
        }

        if ( ! tree )
        {
            tree = right_tree;
            break;
        }
    }

    // Might not have found anything.
    if ( ! tree && ! chunk )
    {
        return nullptr;
    }

    // Go down the left hand side to find smallest chunk.
    while ( tree )
    {
        size_t tree_size = tree->header.size();
        if ( tree_size < chunk_size )
        {
            chunk = tree->next;
            chunk_size = tree_size;
        }

        tree = leftwards( tree );
    }

    return chunk;
}

uint32_t heap_largebin::trie_prefix( size_t index, size_t size )
{
    // Shift bits of size so most significant bit is the top bit of our range.
    assert( heap_largebin_index( size ) == index );
    uint32_t range_bits = 7 + index / 2;
    return (uint32_t)size << ( 32 - range_bits );
}

inline heap_chunk* heap_largebin::leftwards( heap_chunk* chunk )
{
    heap_chunk* left = chunk->child[ 0 ];
    return left ? left : chunk->child[ 1 ];
}

inline heap_chunk* heap_largebin::rightwards( heap_chunk* chunk )
{
    heap_chunk* right = chunk->child[ 1 ];
    return right ? right : chunk->child[ 0 ];
}

void heap_largebin::debug_print( size_t index )
{
    printf( "LARGEBIN %zu: %p\n", index, root );
    if ( root )
    {
        debug_print( index, 0, 0, root );
    }
}

void heap_largebin::debug_print( size_t index, int level, uint32_t prefix, heap_chunk* chunk )
{
    printf( "%*s[", ( level + 1 ) * 2, "" );
    for ( int i = 0; i < level; ++i )
    {
        printf( "%c", ( prefix << i ) >> 31 ? '1' : '0' );
    }
    printf( "..] " );

    size_t range_bits = 7 + index / 2;
    uint32_t size_bits = chunk->header.size() << ( 32 - range_bits );
    for ( size_t i = 0; i < range_bits; ++i )
    {
        printf( "%c", size_bits >> 31 ? '1' : '0' );
        size_bits <<= 1;
    }

    printf( " -> %p/%s/%s:%zu i:%zu p:%p l:%p r:%p\n", chunk, chunk->header.u() ? "U" : "-", chunk->header.p() ? "P" : "-", chunk->header.size(), chunk->index, chunk->parent, chunk->child[ 0 ], chunk->child[ 1 ] );

    heap_chunk* c = chunk;
    do
    {
        printf( "%*s%p/%s/%s:%zu i:%zu @:%p <-> %p\n", ( level + 3 ) * 2, "", c, c->header.u() ? "U" : "-", c->header.p() ? "P" : "-", c->header.size(), c->index, c->prev, c->next );
        c = c->next;
    }
    while ( c != chunk );

    if ( c->child[ 0 ] )
    {
        debug_print( index, level + 1, prefix, c->child[ 0 ] );
    }
    if ( c->child[ 1 ] )
    {
        debug_print( index, level + 1, prefix | ( 1u << ( 31 - level ) ), c->child[ 1 ] );
    }
}

/*
    Main heap data structure, at the end of the initial segment.  Note that
    smallbin_anchors are the sentinel nodes in doubly-linked lists of chunks.
    However, we only store the next and prev pointers.
*/

struct heap_state
{
    explicit heap_state( size_t segment_size );
    ~heap_state();

    heap_chunk_header header;
    uint32_t smallbin_map;
    uint32_t largebin_map;
    uint32_t victim_size;
    uint32_t segment_size;
    heap_segment* segments;
    heap_chunk* victim;
    heap_chunk* smallbin_anchors[ HEAP_SMALLBIN_COUNT * 2 ];
    heap_largebin largebins[ HEAP_LARGEBIN_COUNT ];
    std::mutex mutex;

    void* malloc( size_t size );
    void free( void* p );

    heap_chunk* smallbin_anchor( size_t i );
    void insert_chunk( size_t size, heap_chunk* chunk );
    void remove_chunk( size_t size, heap_chunk* chunk );
    heap_chunk* remove_small_chunk( size_t size, heap_chunk* chunk );
    heap_chunk* remove_large_chunk( size_t index, heap_chunk* chunk );

    heap_chunk* alloc_segment( size_t size );
    void free_segment( heap_segment* segment );
    void unlink_segment( heap_segment* segment );

    void debug_print();
};

heap_state::heap_state( size_t segment_size )
    :   header{ true, true, sizeof( heap_state ), 0 }
    ,   smallbin_map( 0 )
    ,   largebin_map( 0 )
    ,   victim_size( 0 )
    ,   segment_size( segment_size )
    ,   segments( nullptr )
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
            free_chunk
            heap_segment
    */
    assert( segment_size >= sizeof( heap_state ) + sizeof( heap_segment ) );
    heap_chunk* free_chunk = (heap_chunk*)( this + 1 );
    heap_chunk* segment_chunk = (heap_chunk*)( (char*)this + segment_size - sizeof( heap_segment ) );

    if ( free_chunk < segment_chunk )
    {
        size_t size = segment_size - sizeof( heap_state ) - sizeof( heap_segment );
        assert( size <= HEAP_MAX_CHUNK_SIZE );
        heap_chunk_set_free( free_chunk, size );
        victim = free_chunk;
        victim_size = size;
    }

    heap_chunk_set_segment( segment_chunk );

    // Link in the initial segment.
    segments = (heap_segment*)segment_chunk;
    segments->base = this;
    segments->next = nullptr;
}

heap_state::~heap_state()
{
    heap_segment* s = segments;
    while ( s )
    {
        heap_segment* next = s->next;
        if ( s->base != this )
        {
            heap_vmfree( s->base, heap_segment_size( s ) );
        }
        s = next;
    }
}

void* heap_state::malloc( size_t size )
{
    printf( "MALLOC\n" );

    // Chunk size is larger due to overhead, and must be aligned.
    size = size + sizeof( heap_chunk_header );
    size = size + ( HEAP_CHUNK_ALIGNMENT - 1 ) & ~( HEAP_CHUNK_ALIGNMENT - 1 );
    if ( size > HEAP_MAX_CHUNK_SIZE )
    {
        throw std::bad_alloc();
    }

    // Lock.
    std::lock_guard< std::mutex > lock( mutex );

    heap_chunk* chunk = nullptr;
    size_t chunk_size = 0;

    if ( size < HEAP_LARGE_SIZE )
    {
        // Small chunk.

        /*
            Use the entirety of a chunk in a smallbin of the correct size.
        */
        size_t index = heap_smallbin_index( size );
        uint32_t bin_map = smallbin_map & ~( ( 1u << index ) - 1 );
        if ( bin_map & 1u << index )
        {
            heap_chunk* anchor = smallbin_anchor( index );
            chunk = remove_small_chunk( index, anchor->next );
            assert( chunk != anchor );

            heap_chunk_set_allocated( chunk, size );
            heap_chunk_next( chunk, size )->header.set_p();
            return heap_chunk_data( chunk );
        }

        /*
            Locate a chunk to split.
        */
        if ( size <= victim_size )
        {
            // Use existing victim chunk.
            chunk = victim;
            chunk_size = victim_size;
        }
        else if ( bin_map )
        {
            // Use smallest chunk in smallbins that can satisfy the request.
            index = ctz( bin_map );
            chunk_size = ( index + 1 ) * 8;

            assert( index < HEAP_SMALLBIN_COUNT );
            assert( smallbin_map & 1u << index );

            heap_chunk* anchor = smallbin_anchor( index );
            chunk = remove_small_chunk( index, anchor->next );
            assert( chunk != anchor );
        }
        else if ( largebin_map )
        {
            // Pick smallest chunk in the first non-empty largebin.
            size_t index = ctz( largebin_map );
            assert( index < HEAP_LARGEBIN_COUNT );
            chunk = remove_large_chunk( index, largebins[ index ].smallest( index ) );
            chunk_size = chunk->header.size();
        }
        else
        {
            // Allocate new VM segment.
            chunk = alloc_segment( size );
            chunk_size = chunk->header.size();
        }
    }
    else
    {
        // Large chunk.

        /*
            Search for best fit in binned large chunks.
        */
        size_t index = heap_largebin_index( size );
        uint32_t bin_map = largebin_map & ~( ( 1u << index ) - 1 );
        if ( bin_map )
        {
            if ( bin_map & 1u << index )
            {
                // Search bin of appropriate size for smallest chunk that fits.
                chunk = largebins[ index ].best_fit( index, size );
                bin_map &= ~( 1u << index );
                printf( "BEST FIT: %p\n", chunk );
            }

            if ( ! chunk && bin_map )
            {
                // No chunks in that bin, or all chunks were too small, find
                // smallest chunk in a larger bin (if one exists).
                index = ctz( bin_map );
                chunk = largebins[ index ].smallest( index );
                printf( "SMALLEST: %p\n", chunk );
            }
        }

        if ( chunk )
        {
            chunk_size = chunk->header.size();
            assert( size <= chunk_size );

            if ( victim_size >= chunk_size || size > victim_size )
            {
                // Binned chunk will be split.
                remove_large_chunk( chunk->index, chunk );
                printf( "  CONFIRM\n" );
            }
            else
            {
                // Victim is a better fit.
                chunk = victim;
                chunk_size = victim_size;
                printf( "  USE VICTIM INSTEAD: %p\n", chunk );
            }
        }
        else if ( size <= victim_size )
        {
            // Use existing victim chunk.
            chunk = victim;
            chunk_size = victim_size;
            printf( "  USE VICTIM: %p\n", chunk );
        }
        else
        {
            // Neither large chunks nor victim fit, allocate new VM segment.
            chunk = alloc_segment( size );
            chunk_size = chunk->header.size();
            printf( "  ALLOC NEW VM: %p\n", chunk );
        }
    }

    assert( chunk );
    assert( chunk->header.p() );
    assert( ! chunk->header.u() );
    assert( size <= chunk_size );

    heap_chunk* split_chunk = nullptr;
    size_t split_chunk_size = 0;

    if ( chunk_size - size >= HEAP_MIN_BINNED_SIZE )
    {
        // Allocate.
        heap_chunk_set_allocated( chunk, size );

        // Set up split chunk in remaining space.
        split_chunk = heap_chunk_next( chunk, size );
        split_chunk_size = chunk_size - size;
        heap_chunk_set_free( split_chunk, split_chunk_size );
    }
    else
    {
        // Splitting the chunk will leave us with a free chunk that we cannot
        // link into a bin, so just use the entire chunk.
        heap_chunk_set_allocated( chunk, chunk_size );
        heap_chunk_next( chunk, chunk_size )->header.set_p();
    }

    if ( chunk == victim || size < HEAP_LARGE_SIZE )
    {
        // Update victim chunk.
        victim = split_chunk;
        victim_size = split_chunk_size;
    }
    else
    {
        // Add split chunk to bin.
        insert_chunk( split_chunk_size, split_chunk );
    }

    return heap_chunk_data( chunk );
}

void heap_state::free( void* p )
{
    printf( "FREE\n" );

    // free( nullptr ) is valid.
    if ( ! p )
    {
        return;
    }

    // Lock.
    std::lock_guard< std::mutex > lock( mutex );

    // We don't have much context, but assert that the chunk is allocated.
    heap_chunk* chunk = heap_chunk_head( p );
    assert( chunk->header.u() );
    size_t size = chunk->header.size();

    // Attempt to merge with previous chunk.
    if ( ! chunk->header.p() )
    {
        heap_chunk* prev = heap_chunk_prev( chunk );
        assert( prev->header.p() );
        assert( ! prev->header.u() );
        if ( prev != victim )
        {
            size_t prev_size = prev->header.size();
            remove_chunk( prev_size, prev );
            size += prev_size;
        }
        else
        {
            size += victim_size;
        }
        chunk = prev;
    }

    // Attempt to merge with following chunk.
    heap_chunk* next = heap_chunk_next( chunk, size );
    if ( ! next->header.u() )
    {
        assert( next->header.p() );
        if ( next != victim )
        {
            size_t next_size = next->header.size();
            remove_chunk( next_size, next );
            size += next_size;
        }
        else
        {
            victim = chunk;
            size += victim_size;
        }
        next = heap_chunk_next( chunk, size );
    }

    if ( next->header.size() != 0 || chunk != ( (heap_segment*)next )->base )
    {
        // This chunk is free.
        heap_chunk_set_free( chunk, size );
        assert( next->header.u() );
        next->header.clear_p();
        if ( chunk != victim )
        {
            insert_chunk( size, chunk );
        }
        else
        {
            victim_size = size;
        }
    }
    else
    {
        printf( "===================    FREE SEGMENT    ====================\n" );
        // This chunk spans the entire segment, so free the segment.
        free_segment( (heap_segment*)next );
        if ( chunk == victim )
        {
            victim = nullptr;
            victim_size = 0;
        }
    }
}

heap_chunk* heap_state::smallbin_anchor( size_t i )
{
    assert( i < HEAP_SMALLBIN_COUNT );
    return heap_chunk_head( &smallbin_anchors[ i * 2 ] );
}

void heap_state::insert_chunk( size_t size, heap_chunk* chunk )
{
    assert( chunk != victim );

    if ( size < HEAP_MIN_BINNED_SIZE )
    {
        /*
            Chunk is too small to add to a free bin at all, since the required
            pointers won't fit.  It'll be wasted until it can be merged with
            an adjacent chunk.
        */
    }
    else if ( size < HEAP_LARGE_SIZE )
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
        // Insert into largebin.
        size_t index = heap_largebin_index( size );
        largebins[ index ].insert( index, size, chunk );

        // Mark largebin map, as this largebin is not empty.
        largebin_map |= 1u << index;
    }
}

void heap_state::remove_chunk( size_t size, heap_chunk* chunk )
{
    assert( chunk != victim );

    if ( size < HEAP_MIN_BINNED_SIZE || chunk == victim )
    {
        // Isn't in any bin.
    }
    else if ( size < HEAP_LARGE_SIZE )
    {
        remove_small_chunk( heap_smallbin_index( size ), chunk );
    }
    else
    {
        remove_large_chunk( chunk->index, chunk );
    }
}

heap_chunk* heap_state::remove_small_chunk( size_t index, heap_chunk* chunk )
{
    heap_chunk* prev = chunk->prev;
    heap_chunk* next = chunk->next;

    // Unlink from list.
    prev->next = next;
    next->prev = prev;

    // Check if this bin is now empty.
    if ( next == prev )
    {
        assert( prev == smallbin_anchor( index ) );
        smallbin_map &= ~( 1u << index );
    }

    return chunk;
}

heap_chunk* heap_state::remove_large_chunk( size_t index, heap_chunk* chunk )
{
    // Remove from largebin.
    bool nonempty = largebins[ index ].remove( index, chunk );

    // Clear largebin map if the bin is empty.
    if ( ! nonempty )
    {
        largebin_map &= ~( 1u << index );
    }

    return chunk;
}

heap_chunk* heap_state::alloc_segment( size_t size )
{
    // Add space for segment header, and align to VM allocation granularity.
    size += sizeof( heap_segment );
    size = size + ( HEAP_VM_GRANULARITY - 1 ) & ~( HEAP_VM_GRANULARITY - 1 );

    // Make VM allocation.
    void* vmalloc = heap_vmalloc( size );

    // Add segment.
    heap_chunk* segment_chunk = (heap_chunk*)( (char*)vmalloc + size - sizeof( heap_segment ) );
    heap_chunk_set_segment( segment_chunk );
    heap_segment* segment = (heap_segment*)segment_chunk;
    segment->base = vmalloc;

    // Add to segment list in memory address order.
    heap_segment* prev = nullptr;
    if ( std::less< void* >()( vmalloc, segments->base ) )
    {
        segment->next = segments;
        segments = segment;
    }
    else
    {
        prev = segments;
        while ( true )
        {
            heap_segment* next = prev->next;
            if ( ! next || std::less< void* >()( vmalloc, next->base ) )
                break;
            prev = next;
        }

        assert( prev );
        segment->next = prev->next;
        prev->next = segment;
    }

    // Create free chunk.
    size_t free_size = size - sizeof( heap_segment );
    heap_chunk* free_chunk = (heap_chunk*)segment->base;

    // Attempt to merge with previous segment.
    if ( prev && heap_segment_can_merge( prev, segment ) )
    {
        // Remove segment.
        unlink_segment( prev );
        segment->base = prev->base;
        if ( segment->base == this )
        {
            segment_size = heap_segment_size( segment );
        }

        // Merge free space.
        heap_chunk* prev_chunk = (heap_chunk*)prev;
        assert( prev_chunk->header.u() );
        assert( prev_chunk->header.size() == 0 );
        free_size += sizeof( heap_segment );
        free_chunk = prev_chunk;

        if ( ! free_chunk->header.p() )
        {
            prev_chunk = heap_chunk_prev( free_chunk );
            assert( prev_chunk->header.p() );
            assert( ! prev_chunk->header.u() );
            if ( prev_chunk != victim )
            {
                size_t prev_chunk_size = prev_chunk->header.size();
                remove_chunk( prev_chunk_size, prev_chunk );
                free_size += prev_chunk_size;
            }
            else
            {
                free_size += victim_size;
                victim = nullptr;
                victim_size = 0;
            }
            free_chunk = prev_chunk;
        }
    }

    // And with next segment.
    heap_segment* next = segment->next;
    if ( next && heap_segment_can_merge( segment, next ) && next->base != this )
    {
        // Merge free space.
        free_size += sizeof( heap_segment );

        heap_chunk* next_chunk = (heap_chunk*)( segment + 1 );
        assert( next_chunk->header.p() );
        if ( ! next_chunk->header.u() )
        {
            if ( next_chunk != victim )
            {
                size_t next_chunk_size = next_chunk->header.size();
                remove_chunk( next_chunk_size, next_chunk );
                free_size += next_chunk_size;
            }
            else
            {
                free_size += victim_size;
                victim = nullptr;
                victim_size = 0;
            }
        }
        else
        {
            next_chunk->header.clear_p();
        }

        // Remove segment.
        next->base = segment->base;
        unlink_segment( segment );
        segment = next;
    }

    // Construct free chunk in space.
    heap_chunk_set_free( free_chunk, free_size );
    return free_chunk;
}

void heap_state::free_segment( heap_segment* segment )
{
    assert( segment->base != this );
    unlink_segment( segment );
    heap_vmfree( segment->base, heap_segment_size( segment ) );
}

void heap_state::unlink_segment( heap_segment* segment )
{
    heap_segment** link = &segments;
    while ( heap_segment* s = *link )
    {
        if ( s == segment ) break;
        link = &s->next;
    }

    assert( *link == segment );
    *link = segment->next;
}

void heap_state::debug_print()
{
    std::lock_guard< std::mutex > lock( mutex );

    printf( "HEAP %p:\n", this );
    printf( "  smallbin_map: %08X\n", smallbin_map );

    for ( size_t index = 0; index < HEAP_SMALLBIN_COUNT; ++index )
    {
        if ( smallbin_map & 1u << index )
        {
            heap_chunk* anchor = smallbin_anchor( index );
            printf( "    %zu:%p <-> %p\n", index, anchor->prev, anchor->next );
        }
    }

    printf( "  largebin_map: %08X\n", largebin_map );

    for ( size_t index = 0; index < HEAP_LARGEBIN_COUNT; ++index )
    {
        if ( largebin_map & 1u << index )
        {
            largebins[ index ].debug_print( index );
        }
    }

    printf( "  victim: %p:%zu\n", victim, (size_t)victim_size );

    for ( heap_segment* s = segments; s; s = s->next )
    {
        printf( "SEGMENT %p %p:%zu:\n", s, s->base, heap_segment_size( s ) );
        heap_chunk* c = (heap_chunk*)s->base;
        while ( true )
        {
            printf( "  %p/%s/%s:%zu", c, c->header.u() ? "U" : "-", c->header.p() ? "P" : "-", c->header.size() );
            if ( ! c->header.u() )
            {
                if ( c == victim )
                {
                    printf( " VICTIM" );
                }
                else if ( c->header.size() < HEAP_MIN_BINNED_SIZE )
                {
                    printf( " UNBINNED" );
                }
                else if ( c->header.size() < HEAP_LARGE_SIZE )
                {
                    printf( " @:%p <-> %p", c->prev, c->next );
                }
                else
                {
                    printf( " i:%zu u:%p l:%p r:%p @:%p <-> %p", c->index, c->parent, c->child[ 0 ], c->child[ 1 ], c->prev, c->next );
                }
                heap_chunk_footer* footer = (heap_chunk_footer*)( (char*)c + c->header.size() ) - 1;
                printf( " f:%zu\n", (size_t)footer->size );
            }
            else
            {
                printf( "\n" );
            }
            if ( c == (heap_chunk*)s )
            {
                break;
            }
            c = heap_chunk_next( c, c->header.size() );
        }
    }
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
    size_t vmsize = _state->segment_size;
    _state->~heap_state();
    heap_vmfree( _state, vmsize );
}

void* heap::malloc( size_t size )
{
    return _state->malloc( size );
}

void heap::free( void* p )
{
    _state->free( p );
}

void heap::debug_print() const
{
    _state->debug_print();
}

/*
    Get size of allocation.
*/

size_t heap_malloc_size( void* p )
{
    heap_chunk* chunk = heap_chunk_head( p );
    assert( chunk->header.u() );
    return chunk->header.size() - sizeof( heap_chunk_header );
}

}

/*
    Testing of heap.
*/

#ifdef HEAP_TEST

#include <vector>
#include <map>

bool visit_largebin( kf::heap_state* heap, std::map< kf::heap_chunk*, size_t >* bins, size_t index, kf::heap_chunk* tree )
{
    bool ok = true;

    kf::heap_chunk* chunk = tree;
    do
    {
        if ( chunk->index != index )
        {
            printf( "******** LARGEBIN CHUNK %p HAS INCORRECT INDEX\n", chunk );
            ok = false;
        }
        bins->emplace( chunk, 32 + index );
    }
    while ( chunk != tree );

    if ( tree->child[ 0 ] ) if ( ! visit_largebin( heap, bins, index, tree->child[ 0 ] ) ) ok = false;
    if ( tree->child[ 1 ] ) if ( ! visit_largebin( heap, bins, index, tree->child[ 1 ] ) ) ok = false;

    return ok;
}

bool visit_bins( kf::heap_state* heap, std::map< kf::heap_chunk*, size_t >* bins )
{
    bool ok = true;

    for ( size_t smi = 0; smi < kf::HEAP_SMALLBIN_COUNT; ++smi )
    {
        if ( !( heap->smallbin_map & 1u << smi ) )
            continue;
        kf::heap_chunk* anchor = heap->smallbin_anchor( smi );
        kf::heap_chunk* chunk = anchor->next;
        do
        {
            if ( smi != kf::heap_smallbin_index( chunk->header.size() ) )
            {
                printf( "******** SMALLBIN CHUNK %p HAS INCORRECT INDEX\n", chunk );
                ok = false;
            }

            bins->emplace( chunk, smi );
            chunk = chunk->next;
        }
        while ( chunk != anchor );
    }

    for ( size_t lgi = 0; lgi < kf::HEAP_LARGEBIN_COUNT; ++lgi )
    {
        if ( !( heap->largebin_map & 1u << lgi ) )
            continue;
        if ( ! visit_largebin( heap, bins, lgi, heap->largebins[ lgi ].root ) )
            ok = false;
    }

    return ok;
}

bool check_bins( kf::heap_state* heap )
{
    bool ok = true;

    std::map< kf::heap_chunk*, size_t > bins;
    if ( ! visit_bins( heap, &bins ) )
        ok = false;

    for ( kf::heap_segment* s = heap->segments; s; s = s->next )
    {
        kf::heap_chunk* c = (kf::heap_chunk*)s->base;

        bool was_u = true;
        while ( true )
        {
            if ( was_u != c->header.p() )
            {
                printf( "!!!!!!!! P/U MISMATCH: %p\n", c );
                ok = false;
            }
            was_u = c->header.u();

            if ( ! c->header.u() )
            {
                if ( c == heap->victim )
                {
                    if ( bins.find( c ) != bins.end() )
                    {
                        printf( "******** VICTIM CHUNK IS IN BIN\n" );
                        ok = false;
                    }
                    if ( c->header.size() != heap->victim_size )
                    {
                        printf( "******** VICTIM SIZE MISMATCH\n" );
                        ok = false;
                    }
                }
                else if ( c->header.size() < kf::HEAP_MIN_BINNED_SIZE )
                {
                    if ( bins.find( c ) != bins.end() )
                    {
                        printf( "******** UNBINNABLE CHUNK IS IN BIN\n" );
                        ok = false;
                    }
                }
                else if ( c->header.size() < kf::HEAP_LARGE_SIZE )
                {
                    bins.erase( c );
                }
                else
                {
                    bins.erase( c );
                }
            }

            if ( c == (kf::heap_chunk*)s )
                break;
            c = heap_chunk_next( c, c->header.size() );
        }
    }

    if ( ! bins.empty() )
    {
        printf( "******** BINS HAVE DEAD CHUNKS\n" );
        for ( const auto& binned_chunk : bins )
        {
            printf( "    %p : %zu\n", binned_chunk.first, binned_chunk.second );
        }
        ok = false;
    }

    return ok;
}

int main( int argc, char* argv[] )
{
    kf::heap heap;

    srand( clock() );
    struct alloc { void* p; size_t size; int b; };
    std::vector< alloc > allocs;
    int b = 0;

    // Check allocations.
    for ( size_t i = 0; i < 100; ++i )
    {
        printf( "-------- ALLOC\n" );

        size_t alloc_count = rand() % 100;
        for ( size_t j = 0; j < alloc_count; ++j )
        {
            size_t alloc_size = rand() % 512;
            if ( alloc_size >= 256 )
            {
                alloc_size = rand() % ( 16 * 1024 * 1024 );
            }
            void* p = heap.malloc( alloc_size );
            memset( p, b, alloc_size );
            allocs.push_back( { p, alloc_size, b } );
            b = ( b + 1 ) % 256;

            printf( "    -------- %p\n", kf::heap_chunk_head( p ) );
            heap.debug_print();
            bool ok = check_bins( *(kf::heap_state**)&heap );
            assert( ok );
        }

        printf( "-------- FREE\n" );

        size_t free_count = rand() % allocs.size();
        for ( size_t j = 0; j < free_count; ++j )
        {
            size_t alloc_index = rand() % allocs.size();
            alloc a = allocs.at( alloc_index );

            for ( size_t j = 0; j < a.size; ++j )
            {
                if ( *( (char*)a.p + j ) != (char)a.b )
                {
                    printf( "******** BLOCK AT %p:%zu CORRUPT SHOULD BE %02X\n", a.p, a.size, a.b );
                    break;
                }
            }

            printf( "-------- FREE %p\n", kf::heap_chunk_head( a.p ) );
            heap.free( a.p );
            allocs.erase( allocs.begin() + alloc_index );

            heap.debug_print();
            bool ok = check_bins( *(kf::heap_state**)&heap );
            assert( ok );
        }
    }

    for ( const alloc& a : allocs )
    {
        for ( size_t j = 0; j < a.size; ++j )
        {
            if ( *( (char*)a.p + j ) != (char)a.b )
            {
                printf( "******** BLOCK AT %p:%zu CORRUPT SHOULD BE %02X\n", a.p, a.size, a.b );
                break;
            }
        }

        printf( "-------- FREE %p\n", kf::heap_chunk_head( a.p ) );
        heap.free( a.p );

        heap.debug_print();
        bool ok = check_bins( *(kf::heap_state**)&heap );
        assert( ok );
    }

    allocs.clear();

    return EXIT_SUCCESS;
}

#endif

