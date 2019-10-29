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
#include "../datatypes/atomic_load_store.h"

namespace kf
{

struct vm_context;
struct string_object;

/*
    Base class of all objects.  It's empty - data is stored in the header.
*/

struct object {};

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
*/

struct value { uint64_t v; };

const value null_value  = { 0 };
const value false_value = { 1 };
const value true_value  = { 2 };

inline bool is_null( value v )                  { return v.v == null_value.v; }
inline bool is_false( value v )                 { return v.v == false_value.v; }
inline bool is_true( value v )                  { return v.v == true_value.v; }
inline bool is_bool( value v )                  { return v.v >= 1 && v.v < 3; }
inline bool is_object( value v )                { return v.v >= 3 && v.v < UINT64_C( 0x0004'0000'0000'0000 ); }
inline bool is_string( value v )                { return v.v >= UINT64_C( 0x0004'0000'0000'0000 ) && v.v < UINT64_C( 0x0008'0000'0000'0000 ); }
inline bool is_object_or_string( value v )      { return v.v >= 3 && v.v < UINT64_C( 0x0008'0000'0000'0000 ); }
inline bool is_number( value v )                { return v.v >= UINT64_C( 0x0008'0000'0000'0000 ); }

inline value object_value( object* p )          { return { (uint64_t)p }; }
inline value string_value( string_object* s )   { return { (uint64_t)s | UINT64_C( 0x0004'0000'0000'0000 ) }; }
inline object* as_object( value v )             { return (object*)v.v; }
inline object* as_object_or_string( value v )   { return (object*)( v.v & UINT64_C( 0x0003'FFFF'FFFF'FFFF ) ); }
inline string_object* as_string( value v )      { return (string_object*)as_object_or_string( v ); }

inline value number_value( double n )           { uint64_t i; memcpy( &i, &n, sizeof( i ) ); return { ~i }; }
inline double as_number( value v )              { double n; uint64_t i = ~v.v; memcpy( &n, &i, sizeof( n ) ); return n; }

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
    COTHREAD_OBJECT,
    NUMBER_OBJECT,
    BOOL_OBJECT,
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
*/

struct object_header
{
    atomic_u8 color;
    type_code type;
    uint8_t flags;
    uint8_t refcount;
};

object_header* header( object* object );

/*
    Object functions.
*/

void* object_new( vm_context* vm, type_code type, size_t size );
size_t object_size( vm_context* vm, object* object );

/*
    Inline functions.
*/

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

