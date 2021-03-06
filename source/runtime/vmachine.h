//
//  vmachine.h
//
//  Created by Edmund Kapusniak on 12/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KF_VMACHINE_H
#define KF_VMACHINE_H

/*
    The virtual machine's object model and global environment.
*/

#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <vector>
#include <mutex>
#include "datatypes/hash_table.h"
#include "datatypes/segment_list.h"
#include "kenaf/runtime.h"
#include "atomic_load_store.h"
#include "hashkeys.h"

namespace kf
{

struct heap_state;
struct layout_object;
struct lookup_object;
struct string_object;
struct u64val_object;
struct cothread_object;
struct collector;

/*
    Each object type has a unique type index to identify it.
*/

enum type_code : uint8_t
{
    LOOKUP_OBJECT,
    STRING_OBJECT,
    ARRAY_OBJECT,
    TABLE_OBJECT,
    FUNCTION_OBJECT,
    NATIVE_FUNCTION_OBJECT,
    COTHREAD_OBJECT,
    U64VAL_OBJECT,
    NUMBER_OBJECT,
    BOOL_OBJECT,
    NULL_OBJECT,
    LAYOUT_OBJECT,
    VSLOTS_OBJECT,
    KVSLOTS_OBJECT,
    PROGRAM_OBJECT,
    SCRIPT_OBJECT,
    TYPE_COUNT,
};

/*
    Object flags.
*/

enum
{
    FLAG_KEY    = 1 << 0, // String object is a key.
    FLAG_SEALED = 1 << 1, // Lookup object is sealed.
    FLAG_DIRECT = 1 << 2, // Function is a direct constructor.
};

/*
    Each object has a 4-byte header just before its address.  This stores
    the GC mark colour, type index, flags, and small native refcounts.
*/

struct object_header
{
    atomic_u8 color;
    type_code type;
    uint8_t flags;
    uint8_t refcount;
};

struct object
{
    /* EMPTY */
};

object_header* header( object* object );

/*
    Values are 64-bit 'nun-boxed' pointers/doubles.  Inverting the bits of
    a double puts negative NaNs at the bottom of the encoding space.  Both
    x86-64 and ARM64 have a 48-bit virtual address space.

          0000 0000 0000 0000   null
          0000 0000 0000 0001   false
          0000 0000 0000 0003   true
          0000 XXXX XXXX XXXX   object pointer
          0002 XXXX XXXX XXXX   string pointer
          0004 XXXX XXXX XXXX   boxed u64val
          0008 0000 0000 0000   minimum number
          000F FFFF FFFF FFFF   -infinity
          7FFF FFFF FFFF FFFF   -0
          8007 FFFF FFFF FFFE   qNaN
          800F FFFF FFFF FFFE   sNaN
          800F FFFF FFFF FFFF   +infinity
          FFFF FFFF FFFF FFFF   +0

    String objects are tagged using a high bit in the boxed bit pattern,
    because the VM frequently needs to check if an object is a string when
    doing comparisons.

    For-each loops over arrays, tables, and strings store an index value
    directly on the value stack.  To differentiate indexes from pointers, the
    index value is stored as the not of the index.  This overlaps with the
    encoding of numbers, but the compiler knows not to use a register
    containing an index as an operand to an instruction that requires a number.
*/

const uint64_t BOX_OBJPTR = UINT64_C( 0x0001'FFFF'FFFF'FFFF );
const uint64_t BOX_STRING = UINT64_C( 0x0002'0000'0000'0000 );
const uint64_t BOX_U64VAL = UINT64_C( 0x0004'0000'0000'0000 );
const uint64_t BOX_NUMBER = UINT64_C( 0x0008'0000'0000'0000 );

inline bool box_is_null( value v )                  { return v.v == null_value.v; }
inline bool box_is_false( value v )                 { return v.v == false_value.v; }
inline bool box_is_true( value v )                  { return v.v == true_value.v; }
inline bool box_is_bool( value v )                  { return v.v >= 1 && v.v < 3; }
inline bool box_is_object( value v )                { return v.v >= 3 && v.v < BOX_STRING; }
inline bool box_is_string( value v )                { return v.v >= BOX_STRING && v.v < BOX_U64VAL; }
inline bool box_is_u64val( value v )                { return v.v >= BOX_U64VAL && v.v < BOX_NUMBER; }
inline bool box_is_number( value v )                { return v.v >= BOX_NUMBER; }
inline bool box_is_object_or_string( value v )      { return v.v >= 3 && v.v < BOX_U64VAL; }

inline value box_object( object* p )                { return { (uint64_t)p }; }
inline value box_string( string_object* s )         { return { (uint64_t)s | BOX_STRING }; }
inline object* unbox_object( value v )              { return (object*)v.v; }
inline string_object* unbox_string( value v )       { return (string_object*)( v.v & BOX_OBJPTR ); }
inline object* unbox_object_or_string( value v )    { return (object*)( v.v & BOX_OBJPTR ); }

inline value box_u64val( uint64_t u )               { return { u | BOX_U64VAL }; }
inline uint64_t unbox_u64val( value v )             { return v.v & BOX_OBJPTR; }

value box_object( string_object* p ) = delete;      // Prohibit boxing strings as objects.

inline value box_number( double n )                 { uint64_t i; memcpy( &i, &n, sizeof( i ) ); return { ~i }; }
inline double unbox_number( value v )               { double n; uint64_t i = ~v.v; memcpy( &n, &i, sizeof( n ) ); return n; }

inline value box_index( size_t i )                  { return { ~(uint64_t)i }; }
inline size_t unbox_index( value v )                { return ~v.v; }

inline bool box_is_object_type( value v, type_code type )
{
    return box_is_object( v ) && header( unbox_object( v ) )->type == type;
}

/*
    References visible to the garbage collector must be atomic.
*/

template < typename T > using ref = atomic_p< T >;
using ref_value = atomic_u64;

/*
    Selectors.
*/

struct selector
{
    uint32_t cookie;
    uint32_t sindex;
    ref_value* slot;
};

struct key_selector
{
    ref< string_object > key;
    selector sel;
};

/*
    Global GC state.
*/

enum gc_phase
{
    GC_PHASE_NONE,
    GC_PHASE_MARK,
    GC_PHASE_SWEEP,
};

enum gc_color : uint8_t
{
    GC_COLOR_NONE,
    GC_COLOR_PURPLE,
    GC_COLOR_ORANGE,
    GC_COLOR_MARKED,
};

/*
    Execution environment structure.
*/

struct vcontext
{
    vcontext();
    ~vcontext();

    // Current cothread, and cothread execution stack.
    cothread_object* cothread;
    std::vector< cothread_object* > cothread_stack;

    // Global object.
    lookup_object* global_object;

    // Context values.
    std::vector< value > values;

    // Linked list of contexts.
    vcontext* next;
    vcontext* prev;
};

struct vmachine
{
    vmachine();
    ~vmachine();

    // Basic GC heap state.
    gc_color old_color;     // overwriting references to this colour must mark.
    gc_color new_color;     // allocated objects must have this colour.
    gc_phase phase;         // gc phase.
    unsigned countdown;     // GC allocation countdown.

    // Context state.
    vcontext* c;

    // Object model support.
    lookup_object* prototypes[ TYPE_COUNT ];
    string_object* self_key;
    selector self_sel;

    // Lookup object tables.
    hash_table< string_hashkey, string_object* > keys;
    hash_table< lookup_object*, layout_object* > instance_layouts;
    hash_table< layout_hashkey, layout_object* > splitkey_layouts;
    uint32_t next_cookie;

    // Unique u64vals.
    hash_table< uint64_t, u64val_object* > u64vals;

    // Runtime values.
    std::vector< value > values;

    // List of root objects.
    hash_table< object*, size_t > roots;
    vcontext* context_list;

    // Mutator mark stack.
    segment_list< object* > mark_list;

    // GC state.
    std::mutex mark_mutex;  // Serialize marking of cothread stacks.
    std::mutex heap_mutex;  // Serialize access to heap during sweeping.
    heap_state* heap;       // GC heap.
    collector* gc;          // GC thread.
};

void link_vcontext( vmachine* vm, vcontext* vc );
void unlink_vcontext( vmachine* vm, vcontext* vc );

/*
    Object functions.
*/

void* object_new( vmachine* vm, type_code type, size_t size );
size_t object_size( vmachine* vm, object* object );
void object_retain( vmachine* vm, object* object );
void object_release( vmachine* vm, object* object );

/*
    Writes to GC references must use a write barrier.
*/

template < typename T > T* read( const ref< T >& ref );
template < typename T > void winit( ref< T >& ref, T* value );
template < typename T > void write( vmachine* vm, ref< T >& ref, T* value );

value read( const ref_value& ref );
void winit( ref_value& ref, value v );
void write( vmachine* vm, ref_value& ref, value v );

void write_barrier( vmachine* vm, value oldv );
void write_barrier( vmachine* vm, object* old );
void write_barrier( vmachine* vm, string_object* old );

cothread_object* mark_cothread( vmachine* vm, cothread_object* cothread );

/*
    Object model functions.
*/

void setup_object_model( vmachine* vm );
lookup_object* value_keyerof( vmachine* vm, value v );
lookup_object* value_superof( vmachine* vm, value v );

/*

*/

inline object_header* header( object* object )
{
    return (object_header*)object - 1;
}

template < typename T > inline T* read( const ref< T >& ref )
{
    return atomic_load( ref );
}

template < typename T > inline void winit( ref< T >& ref, T* v )
{
    assert( atomic_load( ref ) == nullptr );
    atomic_store( ref, v );
}

template < typename T > inline void write( vmachine* vm, ref< T >& ref, T* v )
{
    if ( vm->old_color )
    {
        T* old = atomic_load( ref );
        if ( old && atomic_load( header( old )->color ) == vm->old_color )
        {
            write_barrier( vm, old );
        }
    }

    atomic_store( ref, v );
}

inline value read( const ref_value& ref )
{
    return { atomic_load( ref ) };
}

inline void winit( ref_value& ref, value v )
{
    assert( atomic_load( ref ) == 0 );
    atomic_store( ref, v.v );
}

inline void write( vmachine* vm, ref_value& ref, value v )
{
    if ( vm->old_color )
    {
        value oldv = { atomic_load( ref ) };
        if ( box_is_object_or_string( oldv ) && atomic_load( header( unbox_object_or_string( oldv ) )->color ) == vm->old_color )
        {
            write_barrier( vm, oldv );
        }
    }

    atomic_store( ref, v.v );
}

}

#endif

