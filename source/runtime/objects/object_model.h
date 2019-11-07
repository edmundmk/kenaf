//
//  object_model.h
//
//  Created by Edmund Kapusniak on 12/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KF_OBJECT_MODEL_H
#define KF_OBJECT_MODEL_H

/*
    The basics of the object model for all garbage-collected objects.
*/

#include <stddef.h>
#include <assert.h>
#include <string.h>
#include "kenaf/runtime.h"
#include "../datatypes/atomic_load_store.h"

namespace kf
{

struct vm_context;
struct string_object;

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
};

/*
    Each object has a 4-byte header just before its address.  This stores
    the GC mark colour, type index, flags, and small native refcounts.

    The base object structure itself is empty.
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
          0004 XXXX XXXX XXXX   string pointer
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

const value boxed_null  = { 0 };
const value boxed_false = { 1 };
const value boxed_true  = { 2 };

inline bool box_is_null( value v )                  { return v.v == boxed_null.v; }
inline bool box_is_false( value v )                 { return v.v == boxed_false.v; }
inline bool box_is_true( value v )                  { return v.v == boxed_true.v; }
inline bool box_is_bool( value v )                  { return v.v >= 1 && v.v < 3; }
inline bool box_is_object( value v )                { return v.v >= 3 && v.v < UINT64_C( 0x0004'0000'0000'0000 ); }
inline bool box_is_string( value v )                { return v.v >= UINT64_C( 0x0004'0000'0000'0000 ) && v.v < UINT64_C( 0x0008'0000'0000'0000 ); }
inline bool box_is_number( value v )                { return v.v >= UINT64_C( 0x0008'0000'0000'0000 ); }
inline bool box_is_object_or_string( value v )      { return v.v >= 3 && v.v < UINT64_C( 0x0008'0000'0000'0000 ); }

inline value box_object( object* p )                { return { (uint64_t)p }; }
inline value box_string( string_object* s )         { return { (uint64_t)s | UINT64_C( 0x0004'0000'0000'0000 ) }; }
inline object* unbox_object( value v )              { return (object*)v.v; }
inline string_object* unbox_string( value v )       { return (string_object*)( v.v & UINT64_C( 0x0003'FFFF'FFFF'FFFF ) ); }
inline object* unbox_object_or_string( value v )    { return (object*)( v.v & UINT64_C( 0x0003'FFFF'FFFF'FFFF ) ); }

value box_object( string_object* p ) = delete; // Prohibit boxing strings as objects.

inline value box_number( double n )                 { uint64_t i; memcpy( &i, &n, sizeof( i ) ); return { ~i }; }
inline double unbox_number( value v )               { double n; uint64_t i = ~v.v; memcpy( &n, &i, sizeof( n ) ); return n; }

inline value box_index( size_t i )                  { return { ~(uint64_t)i }; }
inline size_t unbox_index( value v )                { return ~v.v; }

inline bool box_is_object_type( value v, type_code type )
{
    return box_is_object( v ) && header( unbox_object( v ) )->type == type;
}

/*
    Global GC state.
*/

enum gc_phase : uint8_t
{
    GC_PHASE_NONE,
    GC_PHASE_MARK,
    GC_PHASE_SWEEP,
};

enum gc_color : uint8_t
{
    GC_COLOR_NONE,
    GC_COLOR_ORANGE,
    GC_COLOR_PURPLE,
    GC_COLOR_MARKED,
};

struct gc_state
{
    gc_color old_color;     // overwriting references to this colour must mark.
    gc_color new_color;     // allocated objects must have this colour.
    gc_color dead_color;    // cannot resurrect weak references to this colour.
    gc_phase phase;         // current GC phase.
};

/*
    Object functions.
*/

void* object_new( vm_context* vm, type_code type, size_t size );
size_t object_size( vm_context* vm, object* object );

/*
    References that are read by the garbage collector must be atomic.  Writes
    to GC references must use a write barrier.
*/

template < typename T > using ref = atomic_p< T >;
template < typename T > T* read( const ref< T >& ref );
template < typename T > void winit( ref< T >& ref, T* value );
template < typename T > void write( vm_context* vm, ref< T >& ref, T* value );

using ref_value = atomic_u64;
value read( const ref_value& ref );
void winit( ref_value& ref, value value );
void write( vm_context* vm, ref_value& ref, value value );

template < typename T > inline T* read( const ref< T >& ref )
{
    return atomic_load( ref );
}

template < typename T > inline void winit( ref< T >& ref, T* value )
{
    assert( atomic_load( ref ) == nullptr );
    atomic_store( ref, value );
}

template < typename T > inline void write( vm_context* vm, ref< T >& ref, T* value )
{
    // TODO: pass previous unmarked value of reference to gc thread.
    atomic_store( ref, value );
}

inline value read( const ref_value& ref )
{
    return { atomic_load( ref ) };
}

inline void winit( ref_value& ref, value value )
{
    assert( atomic_load( ref ) == 0 );
    atomic_store( ref, value.v );
}

inline void write( vm_context* vm, ref_value& ref, value value )
{
    // TODO: pass previous unmarked value of reference to gc thread.
    atomic_store( ref, value.v );
}

inline object_header* header( object* object )
{
    return (object_header*)object - 1;
}

}

#endif

