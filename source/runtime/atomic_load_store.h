//
//  atomic_load_store.h
//
//  Created by Edmund Kapusniak on 12/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KF_ATOMIC_LOAD_STORE_H
#define KF_ATOMIC_LOAD_STORE_H

/*
    Our concurrent garbage collector requires:

      - Relaxed atomic loads and stores of the colour byte in the GC header.
      - Relaxed atomic loads and stores of references to garbage collected
        objects, both bare pointers and 64-bit boxed values.
      - Consume ordering/address dependency for access to the GC header from a
        loaded reference.

    The consume ordering enables the following safe pattern:

        mutator thread:
            allocate object
            initialize object
            atomic_produce_fence()
            atomic_store( r, reference )

        collector thread:
            reference = atomic_consume( r )
            value = atomic_load( reference->offset )

    On x86-64 the compiler doesn't need to emit any memory barriers at all.  On
    ARM64 the ideal produce fence is 'dmb ishst', but any stronger fence will
    also do.
*/

#include <stdint.h>

/*
    Check for architectures where we know that our consume pattern can be
    implemented using relaxed loads.  In reality, this is every current
    architecture, but it's good practise to be conservative.
*/

#if defined( __x86_64__ ) || defined( __arm64__ )
#define ATOMIC_CONSUME_RELAXED 1
#endif

#ifdef __GNUC__

namespace kf
{

template < typename T > struct atomic_p { T* v; };
struct atomic_u8 { uint8_t v; };
struct atomic_u64 { uint64_t v; };

// Load/store.

template < typename T > inline T* __attribute__(( always_inline )) atomic_load( const atomic_p< T >& p ) { return __atomic_load_n( &p.v, __ATOMIC_RELAXED ); }
inline uint8_t __attribute__(( always_inline )) atomic_load( const atomic_u8& u ) { return __atomic_load_n( &u.v, __ATOMIC_RELAXED ); }
inline uint64_t __attribute__(( always_inline )) atomic_load( const atomic_u64& u ) { return __atomic_load_n( &u.v, __ATOMIC_RELAXED ); }

template < typename T > inline void __attribute__(( always_inline )) atomic_store( atomic_p< T >& p, T* v ) { __atomic_store_n( &p.v, v, __ATOMIC_RELAXED ); }
inline void __attribute__(( always_inline )) atomic_store( atomic_u8& u, uint8_t v ) { __atomic_store_n( &u.v, v, __ATOMIC_RELAXED ); }
inline void __attribute__(( always_inline )) atomic_store( atomic_u64& u, uint64_t v ) { __atomic_store_n( &u.v, v, __ATOMIC_RELAXED ); }

// Release/consume.

#ifdef ATOMIC_CONSUME_RELAXED
#define ATOMIC_CONSUME __ATOMIC_RELAXED
#else
#define ATOMIC_CONSUME __ATOMIC_CONSUME
#endif

inline void __attribute__(( always_inline )) atomic_produce_fence() { __atomic_thread_fence( __ATOMIC_RELEASE ); }

template < typename T > inline T* __attribute__(( always_inline )) atomic_consume( const atomic_p< T >& p ) { return __atomic_load_n( &p.v, ATOMIC_CONSUME ); }
inline uint64_t __attribute__(( always_inline )) atomic_consume( const atomic_u64& p ) { return __atomic_load_n( &p.v, ATOMIC_CONSUME ); }

#undef ATOMIC_CONSUME

}

#else

#include <atomic>

namespace kf
{

template < typename T > using atomic_p = std::atomic< T* >;
using atomic_u8 = std::atomic< uint8_t >;
using atomic_u64 = std::atomic< uint64_t >;

// Load/store.

template < typename T > inline T* atomic_load( const atomic_p< T >& p ) { return p.load( std::memory_order_relaxed ); }
inline uint8_t atomic_load( const atomic_u8& u ) { return u.load( std::memory_order_relaxed ); }
inline uint64_t atomic_load( const atomic_u64& u ) { return u.load( std::memory_order_relaxed ); }

template < typename T > inline void atomic_store( atomic_p< T >& p, T* v ) { p.store( v, std::memory_order_relaxed ); }
inline void atomic_store( atomic_u8& u, uint8_t v ) { u.store( v, std::memory_order_relaxed ); }
inline void atomic_store( atomic_u64& u, uint64_t v ) { u.store( v, std::memory_order_relaxed ); }

// Release/consume.

#ifdef ATOMIC_CONSUME_RELAXED
#define MEMORY_ORDER_CONSUME std::memory_order_relaxed
#else
#define MEMORY_ORDER_CONSUME std::memory_order_consume
#endif

inline void atomic_produce_fence() { std::atomic_thread_fence( std::memory_order_release ); }

template < typename T > inline T* atomic_consume( const atomic_p< T >& p ) { return p.load( MEMORY_ORDER_CONSUME ); }
inline uint64_t atomic_consume( const atomic_u64& p ) { return p.load( MEMORY_ORDER_CONSUME ); }

#undef MEMORY_ORDER_CONSUME

}

#endif

#endif

