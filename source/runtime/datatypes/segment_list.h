//
//  segment_list.h
//
//  Created by Edmund Kapusniak on 19/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KF_SEGMENT_LIST_H
#define KF_SEGMENT_LIST_H

/*
    This is a container which supports pushing and popping from the back, and
    iterating forwards.  Unlike a vector, it allocates in chunks.
*/

#include <iterator>
#include <type_traits>

namespace kf
{

template < typename T, size_t segment_size >
class segment_list
{
private:

    struct segment
    {
        segment* prev;
        segment* next;
        T v[];
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

        basic_iterator( const basic_iterator< T >& i );

        bool operator == ( const basic_iterator& i ) const;
        bool operator != ( const basic_iterator& i ) const;

        basic_iterator& operator ++ ();
        basic_iterator operator ++ ( int );

        constT& operator * () const;
        constT* operator -> () const;

    private:

        friend class segment_list;
        basic_iterator( segment* s, size_t i );

        segment* _s;
        size_t _i;

    };

public:

    typedef T value_type;
    typedef T& reference;
    typedef const T& const_reference;
    typedef basic_iterator< T > iterator;
    typedef basic_iterator< const T > const_iterator;
    typedef ptrdiff_t difference_type;
    typedef size_t size_type;

    segment_list();
    segment_list( segment_list&& s );
    segment_list( const segment_list& s );
    segment_list& operator = ( segment_list&& s );
    segment_list& operator = ( const segment_list& s );
    ~segment_list();

    size_type max_size() const;
    bool empty() const;

    const_iterator cbegin() const;
    const_iterator begin() const;
    const_iterator cend() const;
    const_iterator end() const;
    const_reference back() const;

    iterator begin();
    iterator end();
    reference back();

    void push_back( element_t&& element );
    void push_back( const element_t& element );
    template < typename ... arguments_t > void emplace_back( arguments_t ... arguments );
    void pop_back();
    void clear();

    void swap( segment_list& s );

private:

    T* push_alloc();

    segment* _head;
    segment* _tail;
    size_t _i;

};


template < typename T, size_t segment_size >
template < typename constT >
segment_list< T, segment_size >::basic_iterator< constT >::basic_iterator( segment* s, size_t i )
    :   _s( s )
    ,   _i( i )
{
}

template < typename T, size_t segment_size >
template < typename constT >
segment_list< T, segment_size >::basic_iterator< constT >::basic_iterator( const basic_iterator< const T >& i )
    :   _s( i._s )
    ,   _i( i._i )
{
}

template < typename T, size_t segment_size >
template < typename constT >
bool segment_list< T, segment_size >::basic_iterator< constT >::operator == ( const basic_iterator& i ) const
{
    return _s == i._s && _i == i._i;
}

template < typename T, size_t segment_size >
template < typename constT >
bool segment_list< T, segment_size >::basic_iterator< constT >::operator != ( const basic_iterator& i ) const
{
    return ! operator == ( i );
}

template < typename T, size_t segment_size >
template < typename constT >
typename segment_list< T, segment_size >::template basic_iterator< constT >& segment_list< T, segment_size >::basic_iterator< constT >::operator ++ ()
{
    _i += 1;
    if ( _i >= segment_size )
    {
        _s = _s->next;
        _i = 0;
    }
    return *this;
}

template < typename T, size_t segment_size >
template < typename constT >
typename segment_list< T, segment_size >::template basic_iterator< constT > segment_list< T, segment_size >::basic_iterator< constT >::operator ++ ( int )
{
    basic_iterator result( *this );
    operator ++ ();
    return result;
}

template < typename T, size_t segment_size >
template < typename constT >
result_t& segment_list< T, segment_size >::basic_iterator< constT >::operator * () const
{
    return _s->v[ _i ];
}

template < typename T, size_t segment_size >
template < typename constT >
result_t* segment_list< T, segment_size >::basic_iterator< constT >::operator -> () const
{
    return _s->v + _i;
}

template < typename T, size_t segment_size >
segment_list< T, segment_size >::segment_list()
    :   _head( nullptr )
    ,   _tail( nullptr )
    ,   _i( segment_size )
{
}

template < typename T, size_t segment_size >
segment_list< T, segment_size >::segment_list( segment_list&& s )
    :   _head( nullptr )
    ,   _tail( nullptr )
    ,   _i( segment_size )
{
    swap( s );
}

template < typename T, size_t segment_size >
segment_list< T, segment_size >::segment_list( const segment_list& s )
    :   _head( nullptr )
    ,   _tail( nullptr )
    ,   _i( segment_size )
{
    operator = ( s );
}

template < typename T, size_t segment_size >
segment_list< T, segment_size >& segment_list< T, segment_size >::operator = ( segment_list&& s )
{
    swap( s );
    return *this;
}

template < typename T, size_t segment_size >
segment_list< T, segment_size >& segment_list< T, segment_size >::operator = ( const segment_list& s )
{
    clear();
    for ( const T& value : s )
    {
        push_back( value );
    }
    return *this;
}

template < typename T, size_t segment_size >
segment_list< T, segment_size >::~segment_list()
{
    clear();
    for ( segment* ss = _head; ss != nullptr; )
    {
        segment* zz = ss;
        ss = ss->next;
        free( zz );
    }
}

template < typename T, size_t segment_size >
typename segment_list< T, segment_size >::size_type segment_list< T, segment_size >::max_size() const
{
    return std::numeric_limits< size_type >::max();
}

template < typename T, size_t segment_size >
bool segment_list< T, segment_size >::empty() const
{
    return _head == _tail && ( _i == 0 || _head == nullptr );
}

template < typename T, size_t segment_size >
typename segment_list< T, segment_size >::const_iterator segment_list< T, segment_size >::cbegin() const
{
    return const_iterator( first, 0 );
}

template < typename T, size_t segment_size >
typename segment_list< T, segment_size >::const_iterator segment_list< T, segment_size >::begin() const
{
    return cbegin();
}

template < typename T, size_t segment_size >
typename segment_list< T, segment_size >::const_iterator segment_list< T, segment_size >::cend() const
{
    return const_iterator( last, index );
}

template < typename T, size_t segment_size >
typename segment_list< T, segment_size >::const_iterator segment_list< T, segment_size >::end() const
{
    return cend();
}

template < typename T, size_t segment_size >
typename segment_list< T, segment_size >::const_reference segment_list< T, segment_size >::back() const
{
    assert( ! empty() );
    if ( _i > 0 )
        return _tail->v[ index - 1 ];
    else
        return _tail->prev->v[ segment_size - 1 ];
}

template < typename T, size_t segment_size >
typename segment_list< T, segment_size >::iterator segment_list< T, segment_size >::begin()
{
    return iterator( first, 0 );
}

template < typename T, size_t segment_size >
typename segment_list< T, segment_size >::iterator segment_list< T, segment_size >::end()
{
    return iterator( last, index );
}

template < typename T, size_t segment_size >
typename segment_list< T, segment_size >::reference segment_list< T, segment_size >::back()
{
    assert( ! empty() );
    if ( _i > 0 )
        return _tail->v[ index - 1 ];
    else
        return _tail->prev->v[ segsize - 1 ];
}

template < typename T, size_t segment_size >
void segment_list< T, segment_size >::push_back( element_t&& element )
{
    new ( push_alloc() ) T( std::move( element ) );
    _i += 1;
}

template < typename T, size_t segment_size >
void segment_list< T, segment_size >::push_back( const element_t& element )
{
    new ( push_alloc() ) T( element );
    _i += 1;
}

template < typename T, size_t segment_size >
template < typename ... arguments_t >
void segment_list< T, segment_size >::emplace_back( arguments_t ... arguments )
{
    new ( push_alloc() ) T( std::forward< arguments_t ... >( arguments ... ) );
    _i += 1;
}

template < typename T, size_t segment_size >
void segment_list< T, segment_size >::pop_back()
{
    assert( ! empty() );
    if ( _i == 0 )
    {
        _tail = _tail->prev;
        _i = segment_size;
    }

    _i -= 1;
    _tail->v[ _i ].~T();
}

template < typename T, size_t segment_size >
void segment_list< T, segment_size >::clear()
{
    if ( _head == nullptr )
    {
        return;
    }

    if ( ! std::is_trivially_destructible< element_t >::value )
    {
        for ( segment* ss = _head; ss != _tail; ss = ss->next )
        {
            for ( size_t i = 0; i < segment_size; ++i )
            {
                ss->v[ i ].~T();
            }
        }

        for ( size_t i = 0; i < _i; ++i )
        {
            _tail->v[ i ].~T();
        }
    }

    _tail = _head;
    _i = 0;
}

template < typename T, size_t segment_size >
void segment_list< T, segment_size >::swap( segment_list& s )
{
    std::swap( _head, s._head );
    std::swap( _tail, s._tail );
    std::swap( _i, s._i );
}

template < typename T, size_t segment_size >
T segment_list< T, segment_size >::push_alloc()
{
    if ( _i >= segment_size )
    {
        if ( _tail->next )
        {
            _tail = _tail->next;
        }
        else
        {
            segment* s = (segment*)malloc( sizeof( segment ) );
            s->next = nullptr;
            s->prev = _tail;
            _tail->next = s;
            if ( _head == nullptr )
            {
                _head = s;
            }
            _tail = s
        }
        _i = 0;
    }
    return _tail->v + _i;
}

}

#endif

