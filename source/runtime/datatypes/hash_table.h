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

template < typename HashKeyValue, typename T, typename K >
T* hash_table_lookup( const HashKeyValue& hashkv, T* kv, size_t kvsize, const K& key )
{
    assert( kvsize );
    T* slot = kv + ( hashkv.hash( key ) % kvsize );
    if ( slot->next ) do
    {
        if ( hashkv.equal( *slot, key ) )
        {
            return slot;
        }
        slot = slot->next;
    }
    while ( slot != (T*)-1 );

    return kv + kvsize;
}

template < typename HashKeyValue, typename T, typename K >
std::pair< bool, bool > hash_table_erase( const HashKeyValue& hashkv, T* kv, size_t kvsize, const K& key )
{
    assert( kvsize );
    T* main_slot = kv + ( hashkv.hash( key ) % kvsize );
    T* next_slot = main_slot->next;
    if ( ! next_slot )
    {
        return std::make_pair( false, false );
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
        return std::make_pair( true, next_slot < main_slot );
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
            return std::make_pair( true, false );
        }

        prev_slot = next_slot;
        next_slot = next_slot->next;
    }

    return std::make_pair( false, false );
}

template < typename HashKeyValue, typename T, typename K >
T* hash_table_assign( const HashKeyValue& hashkv, T* kv, size_t kvsize, const K& key, T* slot )
{
    assert( kvsize );
    if ( slot->next ) do
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
T* hash_table_insert( const HashKeyValue& hashkv, T* kv, size_t kvsize, const K& key, T* main_slot )
{
    assert( kvsize );

    // Client should already have attempted to assign to existing key.
    if ( ! main_slot->next )
    {
        // Main position is empty, insert here.
        main_slot->next = (T*)-1;
        return main_slot;
    }

    // Key is not in the table, and the main position is occupied.
    size_t cuckoo_hash = hashkv.hash( *main_slot );
    T* cuckoo_main_slot = kv + ( cuckoo_hash % kvsize );

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

template < typename HashKeyValue, typename T >
bool hash_table_grow( const HashKeyValue& hashkv, T*& kv, size_t& kvsize, size_t length )
{
    // Load factor is 87.5%.
    if ( length < kvsize - ( kvsize / 8 ) )
    {
        return false;
    }

    // Reallocate.  Last element of kv is a sentinel value.
    size_t new_kvsize = std::max< size_t >( ( kvsize + 1 ) * 2, 16 ) - 1;
    T* new_kv = calloc( new_kvsize + 1u, sizeof( T ) );
    new_kv[ new_kvsize ].next = (T*)-1;

    // Re-insert all elements.
    for ( size_t i = 0; i < kvsize; ++i )
    {
        T* slot = new_kv + ( hashkv.hash( kv[ i ] ) % new_kvsize );
        hashkv.move( hash_table_insert( hashkv(), new_kv, new_kvsize, hashkv.key( kv[ i ] ), slot ), kv[ i ] );
        hashkv.destroy( kv + i );
    }

    // Update.
    free( kv );
    kv = new_kv;
    kvsize = new_kvsize;
    return true;
}

template < typename HashKeyValue, typename T >
void hash_table_clear( const HashKeyValue& hashkv, T* kv, size_t kvsize )
{
    for ( size_t i = 0; i < kvsize; ++i )
    {
        if ( kv[ i ].next )
        {
            hashkv.destroy( kv + i );
            kv[ i ].next = nullptr;
        }
    }
    assert( kv[ kvsize ].next == (T*)-1 );
}

template < typename K, typename V, typename Hash = std::hash< K >, typename KeyEqual = std::equal_to< K > >
class hash_table
{
public:

    struct keyval
    {
        std::pair< const K, V > kv;
        keyval* next;
    };

    struct hashkv : public Hash, public KeyEqual
    {
        const K& key( const keyval& keyval ) const              { return keyval.kv.first; }
        size_t hash( const K& key ) const                       { return Hash::operator () ( key ); }
        size_t hash( const keyval& keyval ) const               { return Hash::operator () ( keyval.kv.first ); }
        bool equal( const keyval& keyval, const K& key ) const  { return KeyEqual::operator () ( keyval.kv.first, key ); }
        void move( keyval* slot, keyval&& from ) const          { new ( &slot->kv ) keyval( std::move( from.kv ) ); }
        void destroy( keyval* slot ) const                      { slot->kv.~pair< const K, V >(); }
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

    hash_table()                                    : _kv( nullptr ), _kvsize( 0 ), _length( 0 ) {}
    hash_table( hash_table&& t )                    : _kv( nullptr ), _kvsize( 0 ), _length( 0 ) { swap( t ); }
    hash_table( const hash_table& t )               : _kv( nullptr ), _kvsize( 0 ), _length( 0 ) { for ( const auto& kv : t ) assign( kv.first, kv.second ); }
    hash_table& operator = ( hash_table&& t )       { clear(); free( _kv ); _kvsize = 0; swap( t ); return *this; }
    hash_table& operator = ( const hash_table& t )  { clear(); for ( const auto& kv : t ) assign( kv.first, kv.second ); return *this; }
    ~hash_table()                                   { clear(); free( _kv ); }

    size_type size() const                          { return _length; }
    bool empty() const                              { return _length = 0; }

    const_iterator cbegin() const                   { return const_iterator( _kv ); }
    const_iterator begin() const                    { return const_iterator( _kv ); }
    const_iterator cend() const                     { return const_iterator( _kv + _kvsize ); }
    const_iterator end() const                      { return const_iterator( _kv + _kvsize ); }

    bool contains( const K& key ) const             { return lookup( key ) != _kv + _kvsize; }
    const_iterator find( const K& key ) const       { return const_iterator( lookup( key ) ); }
    const V& at( const K& key ) const               { keyval* slot = lookup( key ); if ( slot != _kv + _kvsize ) return slot->kv.second; else throw std::out_of_range( "hash_table" ); }

    iterator begin()                                { return iterator( _kv ); }
    iterator end()                                  { return iterator( _kv + _kvsize ); }

    iterator find( const K& key )                   { return iterator( lookup( key ) ); }
    V& at( const K& key )                           { keyval* slot = lookup( key ); if ( slot != _kv + _kvsize ) return slot->kv.second; else throw std::out_of_range( "hash_table" ); }

    template < typename M > iterator assign( K&& key, M&& value );
    template < typename M > iterator assign( const K& key, M&& value );

    iterator erase( const_iterator i );
    size_type erase( const K& key );
    void clear();

    void swap( hash_table& t )                      { std::swap( _kv, t._kv ); std::swap( _kvsize, t._kvsize ); std::swap( _length, t._length ); }

protected:

    keyval* lookup( const K& key ) const            { return hash_table_lookup( hashkv(), _kv, _kvsize, key, hashkv().hash( key ) ); }
    keyval* insert( const K& key, size_t hash, keyval* main_slot );

    keyval* _kv;
    size_t _kvsize;
    size_t _length;
};

template < typename K, typename Hash = std::hash< K >, typename KeyEqual = std::equal_to< K > >
class hash_set
{
public:

    struct keyval
    {
        K key;
        keyval* next;
    };

    struct hashkv : public Hash, public KeyEqual
    {
        const K& key( const keyval& keyval ) const              { return keyval.key; }
        size_t hash( const K& key ) const                       { return Hash::operator () ( key ); }
        size_t hash( const keyval& keyval ) const               { return Hash::operator () ( keyval.key ); }
        bool equal( const keyval& keyval, const K& key ) const  { return KeyEqual::operator () ( keyval.key, key ); }
        void move( keyval* slot, keyval&& from ) const          { new ( &slot->key ) keyval( std::move( from.key ) ); }
        void destroy( keyval* slot ) const                      { slot->key.~K(); }
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
        bool operator != ( const basic_iterator& i ) const      { return _p == i._p; }

        basic_iterator& operator ++ ()                          { _p = next_slot( ++_p ); }
        basic_iterator operator ++ ( int )                      { basic_iterator i( *this ); operator ++ (); return *this; }

        constT& operator * () const                             { return _p->key; }
        constT* operator -> () const                            { return &_p->key; }

    private:

        friend class basic_hash_table;
        basic_iterator( constT* p )                             : _p( p ? next_slot( p ) : nullptr ) {}
        keyval* next_slot( keyval* p )                          { while ( ! p->next ) ++p; return p; }

        keyval* _p;
    };

public:

    typedef K value_type;
    typedef K& reference_type;
    typedef const K& const_reference;
    typedef basic_iterator< K > iterator;
    typedef basic_iterator< const K > const_iterator;
    typedef ptrdiff_t difference_type;
    typedef size_t size_type;

    hash_set()                                  : _kv( nullptr ), _kvsize( 0 ), _length( 0 ) {}
    hash_set( hash_set&& s )                    : _kv( nullptr ), _kvsize( 0 ), _length( 0 ) { swap( s ); }
    hash_set( const hash_set& s )               : _kv( nullptr ), _kvsize( 0 ), _length( 0 ) { for ( const auto& key : s ) insert( key ); }
    hash_set& operator = ( hash_set&& s )       { clear(); free( _kv ); _kvsize = 0; swap( s ); return *this; }
    hash_set& operator = ( const hash_set& s )  { clear(); for ( const auto& key : s ) insert( key ); return *this; }
    ~hash_set()                                 { clear(); free( _kv ); }

    size_type size() const                      { return _length; }
    bool empty() const                          { return _length == 0; }

    const_iterator cbegin() const               { return const_iterator( _kv ); }
    const_iterator begin() const                { return const_iterator( _kv ); }
    const_iterator cend() const                 { return const_iterator( _kv + _kvsize ); }
    const_iterator end() const                  { return const_iteraotr( _kv + _kvsize ); }

    bool contains( const K& key ) const         { return lookup( key ) != _kv + _kvsize; }
    const_iterator find( const K& key ) const   { return const_iterator( lookup( key ) ); }

    iterator begin()                            { return iterator( _kv ); }
    iterator end()                              { return iterator( _kv + _kvsize ); }

    iterator find( const K& key )               { return iterator( lookup( key ) ); }

    iterator insert( K&& key );
    iterator insert( const K& key );

    iterator erase( const_iterator i );
    size_type erase( const K& key );
    void clear();

    void swap( hash_set& s )                    { std::swap( _kv, s._kv ); std::swap( _kvsize, s._kvsize ); std::swap( _length, s._length ); }

private:

    keyval* lookup( const K& key ) const        { return hash_table_lookup( hashkv(), _kv, _kvsize, key ); }
    keyval* insert( const K& key, size_t hash, keyval* main_slot );

    keyval* _kv;
    size_t _kvsize;
    size_t _length;
};

template < typename K, typename V, typename Hash, typename KeyEqual > template < typename M >
typename hash_table< K, V, Hash, KeyEqual >::iterator hash_table< K, V, Hash, KeyEqual >::assign( K&& key, M&& value )
{
    size_t hash = hashkv().hash( key );
    if ( keyval* slot = hash_table_assign( hashkv(), _kv, _kvsize, key, hash ) )
        slot->kv = std::make_pair( std::move( key ), std::forward< M >( value ) );
    else
        new ( &insert( key, hash )->kv ) std::pair< const K, V >( std::move( key ), std::forward< M >( value ) );
}

template < typename K, typename V, typename Hash, typename KeyEqual > template < typename M >
typename hash_table< K, V, Hash, KeyEqual >::iterator hash_table< K, V, Hash, KeyEqual >::assign( const K& key, M&& value )
{
    size_t hash = hashkv().hash( key );
    keyval* main_slot = _kv + ( hash % _kvsize );
    if ( keyval* slot = hash_table_assign( hashkv(), _kv, _kvsize, key, main_slot ) )
        slot->kv = std::make_pair( key, std::forward< M >( value ) );
    else
        new ( &insert( key, hash, main_slot )->kv ) std::pair< const K, V >( key, std::forward< M >( value ) );
}

template < typename K, typename V, typename Hash, typename KeyEqual >
typename hash_table< K, V, Hash, KeyEqual >::keyval* hash_table< K, V, Hash, KeyEqual >::insert( const K& key, size_t hash, keyval* main_slot )
{
    if ( hash_table_grow( hashkv(), _kv, _kvsize, _length ) )
        main_slot = _kv + ( hash % _kvsize );
    _length += 1;
    return hash_table_insert( hashkv(), _kv, _kvsize, key, main_slot );
}

template < typename K, typename V, typename Hash, typename KeyEqual >
typename hash_table< K, V, Hash, KeyEqual >::iterator hash_table< K, V, Hash, KeyEqual >::erase( const_iterator i )
{
    bool moved = hash_table_erase( hashkv(), _kv, _kvsize, i->first ).second;
    return iterator( moved ? i._p + 1 : i._p );
}

template < typename K, typename V, typename Hash, typename KeyEqual >
typename hash_table< K, V, Hash, KeyEqual >::size_type hash_table< K, V, Hash, KeyEqual >::erase( const K& key )
{
    return hash_table_erase( hashkv(), _kv, _kvsize, key ).first ? 1 : 0;
}

template < typename K, typename V, typename Hash, typename KeyEqual >
void hash_table< K, V, Hash, KeyEqual >::clear()
{
    hash_table_clear( hashkv(), _kv, _kvsize );
    _length = 0;
}

template < typename K, typename Hash, typename KeyEqual >
typename hash_set< K, Hash, KeyEqual >::iterator hash_set< K, Hash, KeyEqual >::insert( K&& key )
{
    size_t hash = hashkv().hash( key );
    if ( keyval* slot = hash_table_assign( hashkv(), _kv, _kvsize, key, hash ) )
        slot->key = std::move( key );
    else
        new ( &insert( key, hash )->kv ) K( std::move( key ) );
}

template < typename K, typename Hash, typename KeyEqual >
typename hash_set< K, Hash, KeyEqual >::iterator hash_set< K, Hash, KeyEqual >::insert( const K& key )
{
    size_t hash = hashkv().hash( key );
    if ( keyval* slot = hash_table_assign( hashkv(), _kv, _kvsize, key, hash ) )
        slot->key = key;
    else
        new ( &insert( key, hash )->kv ) K( key );
}

template < typename K, typename Hash, typename KeyEqual >
typename hash_set< K, Hash, KeyEqual >::keyval* hash_set< K, Hash, KeyEqual >::insert( const K& key, size_t hash, keyval* main_slot )
{
    if ( hash_table_grow( hashkv(), _kv, _kvsize, _length ) )
        main_slot = _kv + ( hash % _kvsize );
    _length += 1;
    return hash_table_insert( hashkv(), _kv, _kvsize, key, main_slot );
}

template < typename K, typename Hash, typename KeyEqual >
typename hash_set< K, Hash, KeyEqual >::iterator hash_set< K, Hash, KeyEqual >::erase( const_iterator i )
{
    bool moved = hash_table_erase( hashkv(), _kv, _kvsize, i->first ).second;
    return iterator( moved ? i._p + 1 : i._p );
}

template < typename K, typename Hash, typename KeyEqual >
typename hash_set< K, Hash, KeyEqual >::size_type hash_set< K, Hash, KeyEqual >::erase( const K& key )
{
    return hash_table_erase( hashkv(), _kv, _kvsize, key ).first ? 1 : 0;
}

template < typename K, typename Hash, typename KeyEqual >
void hash_set< K, Hash, KeyEqual >::clear()
{
    hash_table_clear( hashkv(), _kv, _kvsize );
    _length = 0;
}

}

#endif

