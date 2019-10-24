//
//  hash_table.h
//
//  Created by Edmund Kapusniak on 24/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KF_HASH_TABLE_H
#define KF_HASH_TABLE_H

/*
    Hash table where keyvals are stored in a flat array.  Each keyval in a
    bucket is linked into a list, using other positions in the array.
*/

#include <assert.h>
#include <iterator>
#include <type_traits>
#include <functional>

namespace kf
{

template < typename K, typename T, typename KeyVal, typename HashKeyValue >
class basic_hash_table : private HashKeyValue
{
public:

    template < typename constT >
    class basic_iterator
    {
    public:

        typedef std::forward_iterator_tag iterator_category;
        typedef std::remove_cv_t< constT > value_type;
        typedef ptrdiff_t difference_type;
        typedef constT* pointer;
        typedef constT& reference;

        basic_iterator( const basic_iterator< value_type >& i );

        bool operator == ( const basic_iterator& i ) const;
        bool operator != ( const basic_iterator& i ) const;

        basic_iterator& operator ++ ();
        basic_iterator operator ++ ( int );

        constT& operator * () const;
        constT* operator -> () const;

    private:

        friend class basic_hash_table;
        basic_iterator( constT* p );
        KeyVal* _p;
    };

public:

    typedef T value_type;
    typedef T& reference_type;
    typedef const T& const_reference;
    typedef basic_iterator< T > iterator;
    typedef basic_iterator< const T > const_iterator;
    typedef ptrdiff_t difference_type;
    typedef size_t size_type;

    basic_hash_table();
    basic_hash_table( basic_hash_table&& t );
    basic_hash_table( const basic_hash_table& t );
    basic_hash_table& operator = ( basic_hash_table&& t );
    basic_hash_table& operator = ( const basic_hash_table& t );
    ~basic_hash_table();

    size_type size() const;
    size_type max_size() const;
    bool empty() const;

    const_iterator cbegin() const;
    const_iterator begin() const;
    const_iterator cend() const;
    const_iterator end() const;

    bool contains( const K& key ) const;
    const_iterator find( const K& key ) const;

    iterator begin();
    iterator end();

    iterator find( const K& key );

    iterator erase( const_iterator i );
    iterator erase( const K& key );

    void clear();

    void swap( basic_hash_table& t );

protected:

    KeyVal* _kv;
    size_t _capacity;
    size_t _occupancy;

};

template < typename K, typename V >
struct hash_table_keyval
{
    std::pair< const K, V > kv;
    hash_table_keyval* next;
};

template < typename K, typename V, typename Hash, typename KeyEqual >
struct hash_table_hashkv : private Hash, private KeyEqual
{
    size_t hash( const K& key ) const;
    size_t hash( const hash_table_keyval< K, V >& keyval ) const;
    bool equal( const hash_table_keyval< K, V >& keyval, const K& key ) const;
    void move( hash_table_keyval< K, V >* slot, hash_table_keyval< K, V >&& from ) const;
    void destroy( hash_table_keyval< K, V >* slot ) const;
};

template < typename K, typename V, typename Hash = std::hash< K >, typename KeyEqual = std::equal_to< K > >
class hash_table : public basic_hash_table< K, std::pair< const K, V >, hash_table_keyval< K, V >, hash_table_hashkv< K, V, Hash, KeyEqual > >
{
public:

    using basic_hash_table< K, std::pair< const K, V >, hash_table_keyval< K, V >, hash_table_hashkv< K, V, Hash, KeyEqual > >::basic_hash_table;
    using typename basic_hash_table< K, std::pair< const K, V >, hash_table_keyval< K, V >, hash_table_hashkv< K, V, Hash, KeyEqual > >::iterator;

    const V& at( const K& key ) const;
    V& at( const K& key );

    iterator assign( K&& key, V&& value );
    iterator assign( const K& key, V&& value );

};

template < typename K >
struct hash_set_keyval
{
    K key;
    hash_set_keyval* next;
};

template < typename K, typename Hash, typename KeyEqual >
struct hash_set_hashkv : private Hash, KeyEqual
{
    size_t hash( const K& key ) const;
    bool equal( const hash_set_keyval< K >& keyval, const K& key ) const;
    void move( hash_set_keyval< K >* slot, K&& from ) const;
    void destroy( hash_set_keyval< K >* slot ) const;
};

template < typename K, typename Hash = std::hash< K >, typename KeyEqual = std::equal_to< K > >
class hash_set : public basic_hash_table< K, K, hash_set_keyval< K >, hash_set_hashkv< K, Hash, KeyEqual > >
{
public:

    using basic_hash_table< K, K, hash_set_keyval< K >, hash_set_hashkv< K, Hash, KeyEqual > >::basic_hash_table;
    using typename basic_hash_table< K, K, hash_set_keyval< K >, hash_set_hashkv< K, Hash, KeyEqual > >::iterator;

    iterator insert( K&& key );
    iterator insert( const K& key );

};

template < typename HashKeyValue, typename T, typename K >
const T* hash_table_lookup( const HashKeyValue& hashkv, const T* kv, size_t kvsize, const K& key, size_t hash )
{
    assert( kvsize );
    T* slot = kv + ( hash & kvsize - 1 );
    if ( ! slot->next )
    {
        return nullptr;
    }

    do
    {
        if ( hashkv.equal( *slot, key ) )
        {
            return slot;
        }
        slot = slot->next;
    }
    while ( slot != (T*)-1 );

    return nullptr;
}

template < typename HashKeyValue, typename T, typename K >
T* hash_table_erase( const HashKeyValue& hashkv, T* kv, size_t kvsize, const K& key, size_t hash )
{
    assert( kvsize );
    T* main_slot = kv + ( hash & kvsize - 1u );
    T* next_slot = main_slot->next;
    if ( ! next_slot )
    {
        return nullptr;
    }

    if ( hashkv.equal( *main_slot, key ) )
    {
        // Erase kv which is in main position.
        hashkv.destroy( main_slot );

        // Move next slot in linked list into main position.
        if ( next_slot != (T*)-1 )
        {
            hashkv.move( main_slot, std::move( *next_slot ) );
            main_slot->next = next_slot->next;
            hashkv.destroy( next_slot );
            main_slot = next_slot;
        }

        // Erase newly empty slot.
        main_slot->next = nullptr;
        return main_slot;
    }

    T* prev_slot = main_slot;
    while ( next_slot != (T*)-1 )
    {
        if ( hashkv.equal( *next_slot, key ) )
        {
            // Unlink next_slot.
            prev_slot->next = next_slot->next;

            // Erase next_slot.
            hashkv.destroy( next_slot );
            next_slot->next = nullptr;
            return next_slot;
        }

        prev_slot = next_slot;
        next_slot = next_slot->next;
    }

    return nullptr;
}

template < typename HashKeyValue, typename T, typename K >
T* hash_table_assign( const HashKeyValue& hashkv, T* kv, size_t kvsize, const K& key, size_t hash )
{
    assert( kvsize );
    T* slot = kv + ( hash & kvsize - 1u );
    if ( ! slot->next )
    {
        return nullptr;
    }

    do
    {
        if ( hashkv.equal( *slot, key ) )
        {
            return slot;
        }
        slot = slot->next;
    }
    while ( slot != (T*)-1 );

    return nullptr;
}

template < typename HashKeyValue, typename T, typename K >
T* hash_table_insert( const HashKeyValue& hashkv, T* kv, size_t kvsize, const K& key, size_t hash )
{
    assert( kvsize );

    // Client should already have attempted to assign to existing key.
    T* main_slot = kv + ( hash & kvsize - 1u );
    if ( ! main_slot->next )
    {
        // Main position is empty, insert here.
        main_slot->next = (T*)-1;
        return main_slot;
    }

    // Key is not in the table, and the main position is occupied.
    size_t cuckoo_hash = hashkv.hash( *main_slot );
    T* cuckoo_main_slot = kv + ( cuckoo_hash & kvsize - 1u );

    // Cuckoo's main slot must be occupied, because the cuckoo exists.
    assert( cuckoo_main_slot->next );

    // Find nearby free slot.
    T* free_slot = nullptr;
    for ( T* slot = cuckoo_main_slot + 1; slot < kv + kvsize; ++slot )
    {
        if ( ! slot->next )
        {
            free_slot = slot;
            break;
        }
    }

    if ( ! free_slot ) for ( T* slot = cuckoo_main_slot - 1; slot >= kv; --slot )
    {
        if ( ! slot->next )
        {
            free_slot = slot;
            break;
        }
    }

    // Hash collision if both the occupying cuckoo and the key hash to the
    // same bucket.  Link the free slot into the list starting at main.
    if ( cuckoo_main_slot == main_slot )
    {
        free_slot->next = main_slot->next;
        main_slot->next = free_slot;
        return free_slot;
    }

    // Otherwise, the occupying element is a member of another bucket.  Find
    // previous slot in that bucket's linked list.
    T* prev_slot = cuckoo_main_slot;
    while ( prev_slot->next != main_slot )
    {
        prev_slot = prev_slot->next;
        assert( prev_slot != (T*)-1 );
    }

    // Move item from main_slot to free_slot and update bucket list.
    T* next_slot = main_slot->next;
    hashkv.move( free_slot, std::move( *main_slot ) );
    prev_slot->next = free_slot;
    free_slot->next = next_slot;

    // Erase main_slot, as it's where we'll put the new element.
    hashkv.destroy( main_slot );
    main_slot->next = (T*)-1;
    return main_slot;
}

}

#endif

