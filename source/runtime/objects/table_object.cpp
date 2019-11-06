//
//  table_object.cpp
//
//  Created by Edmund Kapusniak on 26/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "table_object.h"
#include "kenaf/errors.h"
#include "string_object.h"
#include <limits.h>
#include <functional>

namespace kf
{

/*
    Round up to next highest power of two.
*/

static inline size_t ceilpow2( size_t x )
{
    if ( x > 1 )
        return 1 << ( sizeof( size_t ) * CHAR_BIT - __builtin_clz( x - 1 ) );
    else
        return 1;
}

/*
    When used as keys, -0.0 becomes 0.0.
*/

static inline value key_value( value value )
{
    if ( ! box_is_number( value ) || value.v != box_number( -0.0 ).v )
    {
        return value;
    }
    else
    {
        return box_number( +0.0 );
    }

}

static inline size_t key_hash( value key )
{
    if ( ! box_is_string( key ) )
    {
        return std::hash< uint64_t >()( key.v );
    }
    else
    {
        string_object* string = unbox_string( key );
        return std::hash< std::string_view >()( std::string_view( string->text, string->size ) );
    }
}

static inline bool key_equal( value a, value b )
{
    if ( a.v == b.v )
    {
        return true;
    }

    if ( box_is_string( a ) && box_is_string( b ) )
    {
        string_object* sa = unbox_string( a );
        string_object* sb = unbox_string( b );
        return sa->size == sb->size && memcmp( sa->text, sb->text, sa->size ) == 0;
    }

    return false;
}

/*
    Key/value slot list.
*/

kvslots_object* kvslots_new( vm_context* vm, size_t count )
{
    kvslots_object* kvslots = new ( object_new( vm, KVSLOTS_OBJECT, sizeof( kvslots_object ) + ( count + 1 ) * sizeof( kvslot ) ) ) kvslots_object();
    kvslots->count = count;
    kvslots->slots[ count ].next = (kvslot*)-1;
    return kvslots;
}

/*
    Table functions.
*/

table_object* table_new( vm_context* vm, size_t capacity )
{
    table_object* table = new ( object_new( vm, TABLE_OBJECT, sizeof( table_object ) ) ) table_object();
    if ( capacity != 0 )
    {
        capacity += capacity / 4;
        capacity = ceilpow2( std::max< size_t >( capacity, 16 ) ) - 1;
        winit( table->kvslots, kvslots_new( vm, capacity ) );
    }
    return table;
}

value table_getindex( vm_context* vm, table_object* table, value key )
{
    value v;
    if ( table_tryindex( vm, table, key, &v ) )
    {
        return v;
    }
    else
    {
        throw index_error( "missing key in table" );
    }
}

bool table_tryindex( vm_context* vm, table_object* table, value key, value* out_value )
{
    kvslots_object* kvslots = read( table->kvslots );
    if ( kvslots->count )
    {
        key = key_value( key );
        kvslot* slot = kvslots->slots + key_hash( key ) % kvslots->count;
        if ( slot->next ) do
        {
            if ( key_equal( read( slot->k ), key ) )
            {
                if ( out_value )
                {
                    *out_value = read( slot->v );
                }
                return true;
            }
            slot = slot->next;
        }
        while ( slot != (kvslot*)-1 );
    }
    return false;
}

static kvslot* table_insert( vm_context* vm, kvslots_object* kvslots, size_t kvcount, value key, kvslot* main_slot )
{
    // Insert in main slot if it's empty.
    if ( main_slot->next )
    {
        main_slot->next = (kvslot*)-1;
        return main_slot;
    }

    // Identify cuckoo.
    size_t cuckoo_hash = key_hash( read( main_slot->k ) );
    kvslot* cuckoo_main_slot = kvslots->slots + cuckoo_hash % kvcount;
    assert( cuckoo_main_slot->next );

    // Find nearby free slot.
    kvslot* free_slot = nullptr;
    for ( kvslot* slot = cuckoo_main_slot + 1; slot < kvslots->slots + kvcount; ++slot )
    {
        if ( ! slot->next )
        {
            free_slot = slot;
            break;
        }
    }

    if ( ! free_slot ) for ( kvslot* slot = cuckoo_main_slot - 1; slot >= kvslots->slots; --slot )
    {
        if ( ! slot->next )
        {
            free_slot = slot;
            break;
        }
    }

    // Check for hash collision if cuckoo and new entry hash to same bucket.
    if ( cuckoo_main_slot == main_slot )
    {
        free_slot->next = main_slot->next;
        main_slot->next = free_slot;
        return free_slot;
    }

    // Find previous slot in cuckoo's bucket's linked list.
    kvslot* prev_slot = cuckoo_main_slot;
    while ( prev_slot->next != main_slot )
    {
        prev_slot = prev_slot->next;
        assert( prev_slot != (kvslot*)-1 );
    }

    // Move item from main_slot to free_slot.
    write( vm, free_slot->k, read( main_slot->k ) );
    write( vm, free_slot->v, read( main_slot->v ) );

    // Update bucket list for amended cuckoo bucket.
    prev_slot->next = free_slot;
    free_slot->next = main_slot->next;
    main_slot->next = (kvslot*)-1;

    return main_slot;
}

void table_setindex( vm_context* vm, table_object* table, value key, value val )
{
    key = key_value( key );
    size_t hash = key_hash( key );
    kvslot* main_slot;

    kvslots_object* kvslots = read( table->kvslots );
    size_t kvcount = kvslots ? kvslots->count : 0;

    if ( kvcount )
    {
        // Check if the key already exists in the table.
        kvslot* slot = main_slot = kvslots->slots + hash % kvcount;
        if ( slot->next ) do
        {
            if ( key_equal( read( slot->k ), key ) )
            {
                write( vm, slot->v, val );
                return;
            }
            slot = slot->next;
        }
        while ( slot != (kvslot*)-1 );
    }

    if ( table->length >= kvcount - ( kvcount / 8 ) )
    {
        // Reallocate kvslots with a larger count.
        size_t new_kvcount = std::max< size_t >( ( kvcount + 1 ) * 2, 16 ) - 1;
        kvslots_object* new_kvslots = kvslots_new( vm, new_kvcount );

        // Re-insert all elements.
        for ( size_t i = 0; i < kvcount; ++i )
        {
            kvslot* kval = kvslots->slots + i;
            if ( kval->next )
            {
                value key = read( kval->k );
                kvslot* slot = new_kvslots->slots + key_hash( key ) % new_kvcount;
                slot = table_insert( vm, new_kvslots, new_kvcount, key, slot );
                write( vm, slot->k, key );
                write( vm, slot->v, val );
            }
        }

        // Update.
        write( vm, table->kvslots, new_kvslots );
        kvslots = new_kvslots;
        kvcount = new_kvcount;

        // Recalculate main slot in reallocated slots list.
        main_slot = kvslots->slots + hash % kvcount;
    }

    // Insert.
    table->length += 1;
    kvslot* slot = table_insert( vm, kvslots, kvcount, key, main_slot );
    write( vm, slot->k, key );
    write( vm, slot->v, val );
}

void table_delindex( vm_context* vm, table_object* table, value key )
{
    kvslots_object* kvslots = read( table->kvslots );
    if ( ! kvslots || ! table->length )
    {
        return;
    }

    key = key_value( key );
    size_t kvcount = kvslots->count;
    kvslot* main_slot = kvslots->slots + key_hash( key ) % kvcount;
    kvslot* next_slot = main_slot->next;
    if ( ! next_slot )
    {
        return;
    }

    if ( key_equal( read( main_slot->k ), key ) )
    {
        // Move next slot in linked list into main position.
        if ( next_slot != (kvslot*)-1 )
        {
            write( vm, main_slot->k, read( next_slot->k ) );
            write( vm, main_slot->v, read( next_slot->v ) );
            main_slot->next = next_slot->next;
            main_slot = next_slot;
        }

        // Erase newly empty slot.
        write( vm, main_slot->k, { 0 } );
        write( vm, main_slot->v, { 0 } );
        main_slot->next = nullptr;
        table->length -= 1;
        return;
    }

    kvslot* prev_slot = main_slot;
    while ( next_slot != (kvslot*)-1 )
    {
        if ( key_equal( read( next_slot->k ), key ) )
        {
            // Unlink and erase next_slot.
            write( vm, next_slot->k, { 0 } );
            write( vm, next_slot->v, { 0 } );
            prev_slot->next = next_slot->next;
            next_slot->next = nullptr;
            table->length -= 1;
            return;
        }

        prev_slot = next_slot;
        next_slot = next_slot->next;
    }
}

void table_clear( vm_context* vm, table_object* table )
{
    kvslots_object* kvslots = read( table->kvslots );
    if ( ! kvslots )
    {
        return;
    }

    size_t kvcount = kvslots->count;
    for ( size_t i = 0; i < kvcount; ++i )
    {
        kvslot* kval = kvslots->slots + i;
        if ( kval->next )
        {
            write( vm, kval->k, { 0 } );
            write( vm, kval->v, { 0 } );
            kval->next = nullptr;
        }
    }

    table->length = 0;
}

size_t table_iterate( vm_context* vm, table_object* table )
{
    kvslots_object* kvslots = read( table->kvslots );
    size_t i = 0;
    while ( ! kvslots->slots[ i ].next )
    {
        ++i;
    }
    return i;
}

bool table_next( vm_context* vm, table_object* table, size_t* i, table_keyval* keyval )
{
    kvslots_object* kvslots = read( table->kvslots );
    if ( *i < kvslots->count )
    {
        // Get key/value from this slot.
        kvslot* kvslot = kvslots->slots + *i;
        assert( kvslot->next );
        keyval->k = read( kvslot->k );
        keyval->v = read( kvslot->v );

        // Find next non-empty slot.  kvslots objects always end with sentinel.
        ++*i;
        while ( ! kvslots->slots[ *i ].next )
        {
            ++*i;
        }

        return true;
    }
    else
    {
        return false;
    }
}

}

