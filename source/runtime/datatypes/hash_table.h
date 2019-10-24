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

#include <iterator>
#include <type_traits>

namespace kf
{

template < typename K, typename V >
class hash_table
{
private:

    struct keyval : public std::pair< const K, V >
    {
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

        basic_iterator( const basic_iterator< value_type >& i );

        bool operator == ( const basic_iterator& i ) const;
        bool operator != ( const basic_iterator& i ) const;

        basic_iterator& operator ++ ();
        basic_iterator operator ++ ( int );

        constT& operator * () const;
        constT* operator -> () const;

    private:

        friend class hash_table;
        basic_iterator( constT* p );

        constT* _p;

    };

public:

    typedef std::pair< const K, V > value_type;
    typedef std::pair< const K, V >& reference_type;
    typedef const std::pair< const K, V >& const_reference;
    typedef basic_iterator< std::pair< const K, V > > iterator;
    typedef basic_iterator< const std::pair< const K, V > > const_iterator;
    typedef ptrdiff_t difference_type;
    typedef size_t size_type;

    hash_table();
    hash_table( hash_table&& t );
    hash_table( const hash_table& t );
    hash_table& operator = ( hash_table&& t );
    hash_table& operator = ( const hash_table& t );
    ~hash_table();

    size_type size() const;
    size_type max_size() const;
    bool empty() const;

    const_iterator cbegin() const;
    const_iterator begin() const;
    const_iterator cend() const;
    const_iterator end() const;

    const V& at( const K& key ) const;
    const_iterator find( const K& key ) const;

    iterator begin();
    iterator end();

    V& at( const K& key );
    iterator find( const K& key );

    iterator assign( K&& key, V&& value );
    iterator assign( const K& key, V&& value );

    iterator erase( const_iterator i );
    iterator erase( const K& key );

    void clear();

    void swap( hash_table& t );

private:

    keyval* _kv;
    size_t _capacity;
    size_t _occupancy;

};

template < typename K >
class hash_set
{
private:

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

        friend class hash_set;
        basic_iterator( constT* p );

        constT* _p;

    };

public:

    typedef K value_type;
    typedef K& reference_type;
    typedef const K& const_reference;
    typedef basic_iterator< K > iterator;
    typedef basic_iterator< const K > const_iterator;
    typedef ptrdiff_t difference_type;
    typedef size_t size_type;

    hash_set();
    hash_set( hash_set&& s );
    hash_set( const hash_set& s );
    hash_set& operator = ( hash_set&& s );
    hash_set& operator = ( const hash_set& s );
    ~hash_set();

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

    iterator assign( K&& key );
    iterator assign( const K& key );

    iterator erase( const_iterator i );
    iterator erase( const K& key );

    void clear();

    void swap( hash_set& s );

private:

    K* _v;
    size_t _capacity;
    size_t _occupancy;

};

}

#endif

