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
    Hash table where keyvals are stored in a flat array, using open addressing.
    Each entry in the array is a 'slot'.  Each element is either in it's
    bucket's main slot (i.e. the one that it hashes to), or in a slot in a
    linked list from that slot.
*/

#include <assert.h>
#include <iterator>
#include <functional>
#include <algorithm>
#include <stdexcept>

namespace kf
{

template < typename K, typename V, typename Hash = std::hash< K >, typename KeyEqual = std::equal_to< K > >
class hash_table
{
public:

    struct keyval
    {
        std::pair< const K, V > kv;
        keyval* next;
    };

    template < typename constT >
    class basic_iterator
    {
    public:

        typedef std::forward_iterator_tag iterator_category;
        typedef std::remove_cv_t< constT > value_type;
        typedef ptrdiff_t difference_type;
        typedef constT* pointer;
        typedef constT& reference;

        basic_iterator( const basic_iterator< value_type >& i ) : _p( i._p ) {}

        bool operator == ( const basic_iterator& i ) const      { return _p == i._p; }
        bool operator != ( const basic_iterator& i ) const      { return _p != i._p; }

        basic_iterator& operator ++ ()                          { _p = next_slot( ++_p ); }
        basic_iterator operator ++ ( int )                      { basic_iterator i( *this ); operator ++ (); return *this; }

        constT& operator * () const                             { return _p->kv; }
        constT* operator -> () const                            { return &_p->kv; }

    private:

        friend class basic_hash_table;
        basic_iterator( constT* p )                             : _p( p ? next_slot( p ) : nullptr ) {}
        keyval* next_slot( keyval* p )                          { while ( ! p->next ) ++p; return p; }

        keyval* _p;
    };

public:

    typedef std::pair< const K, V > value_type;
    typedef std::pair< const K, V >& reference_type;
    typedef const std::pair< const K, V >& const_reference;
    typedef basic_iterator< std::pair< const K, V > > iterator;
    typedef basic_iterator< const std::pair< const K, V > > const_iterator;
    typedef ptrdiff_t difference_type;
    typedef size_t size_type;

    hash_table()                                        : _kv( nullptr ), _kvsize( 0 ), _length( 0 ) {}
    hash_table( hash_table&& t )                        : _kv( nullptr ), _kvsize( 0 ), _length( 0 ) { swap( t ); }
    hash_table( const hash_table& t )                   : _kv( nullptr ), _kvsize( 0 ), _length( 0 ) { for ( const auto& kv : t ) assign( kv.first, kv.second ); }
    hash_table& operator = ( hash_table&& t )           { destroy(); swap( t ); return *this; }
    hash_table& operator = ( const hash_table& t )      { clear(); for ( const auto& kv : t ) assign( kv.first, kv.second ); return *this; }
    ~hash_table()                                       { destroy(); }

    size_type size() const                              { return _length; }
    bool empty() const                                  { return _length = 0; }

    const_iterator cbegin() const                       { return const_iterator( _kv ); }
    const_iterator begin() const                        { return const_iterator( _kv ); }
    const_iterator cend() const                         { return const_iterator( _kv + _kvsize ); }
    const_iterator end() const                          { return const_iterator( _kv + _kvsize ); }

    bool contains( const K& key ) const                 { return lookup_key( key ) != _kv + _kvsize; }
    const_iterator find( const K& key ) const           { return const_iterator( lookup_key( key ) ); }
    const V& at( const K& key ) const                   { keyval* slot = lookup_key( key ); if ( slot != _kv + _kvsize ) return slot->kv.second; else throw std::out_of_range( "hash_table" ); }

    iterator begin()                                    { return iterator( _kv ); }
    iterator end()                                      { return iterator( _kv + _kvsize ); }

    iterator find( const K& key )                       { return iterator( lookup_key( key ) ); }
    V& at( const K& key )                               { keyval* slot = lookup_key( key ); if ( slot != _kv + _kvsize ) return slot->kv.second; else throw std::out_of_range( "hash_table" ); }

    template < typename M > iterator insert_or_assign( K&& key, M&& value );
    template < typename M > iterator insert_or_assign( const K& key, M&& value );

    iterator erase( const_iterator i );
    size_type erase( const K& key );
    void clear();

    void swap( hash_table& t );

protected:

    keyval* lookup_key( const K& key ) const;
    keyval* assign_key( const K& key, keyval* main_slot );
    keyval* insert_key( const K& key, size_t hash, keyval* main_slot );
    keyval* insert_key( const K& key, keyval* kv, size_t kvsize, keyval* main_slot );
    std::pair< bool, bool > erase_key( const K& key );
    void destroy();

    keyval* _kv;
    size_t _kvsize;
    size_t _length;
};

template < typename K, typename V, typename Hash, typename KeyEqual > template < typename M >
typename hash_table< K, V, Hash, KeyEqual >::iterator hash_table< K, V, Hash, KeyEqual >::insert_or_assign( K&& key, M&& value )
{
}

template < typename K, typename V, typename Hash, typename KeyEqual > template < typename M >
typename hash_table< K, V, Hash, KeyEqual >::iterator hash_table< K, V, Hash, KeyEqual >::insert_or_assign( const K& key, M&& value )
{
}

template < typename K, typename V, typename Hash, typename KeyEqual >
typename hash_table< K, V, Hash, KeyEqual >::iterator hash_table< K, V, Hash, KeyEqual >::erase( const_iterator i )
{
    return iterator( erase_key( i->first ).second ? i._p + 1 : i._p );
}

template < typename K, typename V, typename Hash, typename KeyEqual >
typename hash_table< K, V, Hash, KeyEqual >::size_type hash_table< K, V, Hash, KeyEqual >::erase( const K& key )
{
    return erase_key( key ).first ? 1 : 0;
}

template < typename K, typename V, typename Hash, typename KeyEqual >
void hash_table< K, V, Hash, KeyEqual >::clear()
{
    for ( size_t i = 0; i < _kvsize; ++i )
    {
        keyval* slot = _kv + i;
        if ( slot->next )
        {
            slot->kv.~pair();
            slot->next = nullptr;
        }
    }
    _length = 0;
}

template < typename K, typename V, typename Hash, typename KeyEqual >
void hash_table< K, V, Hash, KeyEqual >::swap( hash_table& t )
{
    std::swap( _kv, t._kv );
    std::swap( _kvsize, t._kvsize );
    std::swap( _length, t._length );
}

template < typename K, typename V, typename Hash, typename KeyEqual >
typename hash_table< K, V, Hash, KeyEqual >::keyval* hash_table< K, V, Hash, KeyEqual >::lookup_key( const K& key ) const
{
    if ( ! _kvsize )
    {
        return _kv;
    }

    Hash keyhash = Hash();
    KeyEqual keyequal = KeyEqual();

    keyval* slot = _kv + hashkey( key ) % _kvsize;
    if ( slot->next ) do
    {
        if ( keyequal( slot->kv.first, key ) )
        {
            return slot;
        }
        slot = slot->next;
    }
    while ( slot != (keyval*)-1 );

    return _kv + _kvsize;
}

template < typename K, typename V, typename Hash, typename KeyEqual >
typename hash_table< K, V, Hash, KeyEqual >::keyval* hash_table< K, V, Hash, KeyEqual >::assign_key( const K& key, keyval* main_slot )
{
    if ( ! _kvsize )
    {
        return nullptr;
    }

    KeyEqual keyequal = KeyEqual();

    keyval* slot = main_slot;
    if ( slot->next ) do
    {
        if ( keyequal( slot->kv.first, key ) )
        {
            return slot;
        }
        slot = slot->next;
    }
    while ( slot != (keyval*)-1 );

    return nullptr;
}

template < typename K, typename V, typename Hash, typename KeyEqual >
typename hash_table< K, V, Hash, KeyEqual >::keyval* hash_table< K, V, Hash, KeyEqual >::insert_key( const K& key, size_t hash, keyval* main_slot )
{
    Hash keyhash = Hash();
    KeyEqual keyequal = KeyEqual();

    // Load factor is 87.5%.
    if ( _length >= _kvsize - ( _kvsize / 8 ) )
    {
        // Reallocate.  Last element of kv is a sentinel value.
        size_t new_kvsize = std::max< size_t >( ( _kvsize + 1 ) * 2, 16 ) - 1;
        keyval* new_kv = calloc( new_kvsize + 1, sizeof( keyval ) );
        new_kv[ new_kvsize ].next = (keyval*)-1;

        // Re-insert all elements.
        for ( size_t i = 0; i < _kvsize; ++i )
        {
            const K& key = _kv[ i ].kv.first;
            keyval* slot = new_kv + keyhash( key ) % new_kvsize;
            new ( insert_key( key, new_kv, new_kvsize, slot )->kv ) std::pair< const K, V >( std::move( _kv[ i ].kv ) );
            _kv[ i ].kv.~pair();
        }

        // Update.
        free( _kv );
        _kv = new_kv;
        _kvsize = new_kvsize;

        // Recalculate main slot with new size.
        main_slot = _kv + hash % _kvsize;
    }

    // Insert.
    _length += 1;
    return insert_key( key, _kv, _kvsize, main_slot );
}

template < typename K, typename V, typename Hash, typename KeyEqual >
typename hash_table< K, V, Hash, KeyEqual >::keyval* hash_table< K, V, Hash, KeyEqual >::insert_key( const K& key, keyval* kv, size_t kvsize, keyval* main_slot )
{
    assert( kvsize );
    Hash keyhash = Hash();
    KeyEqual keyequal = KeyEqual();

    // Client should already have attempted to assign to existing key.
    if ( ! main_slot->next )
    {
        // Main position is empty, insert here.
        main_slot->next = (keyval*)-1;
        return main_slot;
    }

    // Key is not in the table, and the main position is occupied.
    size_t cuckoo_hash = keyhash( *main_slot );
    keyval* cuckoo_main_slot = kv + ( cuckoo_hash % kvsize );

    // Cuckoo's main slot must be occupied, because the cuckoo exists.
    assert( cuckoo_main_slot->next );

    // Find nearby free slot.
    keyval* free_slot = nullptr;
    for ( keyval* slot = cuckoo_main_slot + 1; slot < kv + kvsize; ++slot )
    {
        if ( ! slot->next )
        {
            free_slot = slot;
            break;
        }
    }

    if ( ! free_slot ) for ( keyval* slot = cuckoo_main_slot - 1; slot >= kv; --slot )
    {
        if ( ! slot->next )
        {
            free_slot = slot;
            break;
        }
    }
    assert( free_slot );

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
    keyval* prev_slot = cuckoo_main_slot;
    while ( prev_slot->next != main_slot )
    {
        prev_slot = prev_slot->next;
        assert( prev_slot != (keyval*)-1 );
    }

    // Move item from main_slot to free_slot.
    new ( &free_slot->kv ) std::pair< const K, V >( std::move( main_slot->kv ) );
    main_slot->kv.~pair();
    main_slot->next = (keyval*)-1;

    // Update bucket list.
    keyval* next_slot = main_slot->next;
    prev_slot->next = free_slot;
    free_slot->next = next_slot;

    return main_slot;
}

template < typename K, typename V, typename Hash, typename KeyEqual >
std::pair< bool, bool > hash_table< K, V, Hash, KeyEqual >::erase_key( const K& key )
{
    if ( ! _kvsize )
    {
        return std::make_pair( false, false );
    }

    Hash keyhash = Hash();
    KeyEqual keyequal = KeyEqual();

    keyval* main_slot = _kv + keyhash( key ) % _kvsize;
    keyval* next_slot = main_slot->next;
    if ( ! next_slot )
    {
        return std::make_pair( false, false );
    }

    if ( keyequal( main_slot->kv.first, key ) )
    {
        // Erase kv which is in main position.
        main_slot->kv.~pair();

        // Move next slot in linked list into main position.
        if ( next_slot != (keyval*)-1 )
        {
            new ( &main_slot->kv ) std::pair< const K, V >( std::move( next_slot->kv ) );
            next_slot->kv.~pair();
            main_slot->next = next_slot->next;
            main_slot = next_slot;
        }

        // Erase newly empty slot.
        main_slot->next = nullptr;
        return std::make_pair( true, next_slot < main_slot );
    }

    keyval* prev_slot = main_slot;
    while ( next_slot != (keyval*)-1 )
    {
        if ( keyequal( next_slot->kv.first, key ) )
        {
            // Unlink next_slot.
            prev_slot->next = next_slot->next;

            // Erase next_slot.
            next_slot->kv.~pair();
            next_slot->next = nullptr;
            return std::make_pair( true, false );
        }

        prev_slot = next_slot;
        next_slot = next_slot->next;
    }

    return std::make_pair( false, false );
}

template < typename K, typename V, typename Hash, typename KeyEqual >
void hash_table< K, V, Hash, KeyEqual >::destroy()
{
    clear();
    free( _kv );
    _kv = nullptr;
    _kvsize = 0;
}

}

#endif

