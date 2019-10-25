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

namespace kf
{

template < typename T, size_t segment_size = 64 >
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

        basic_iterator( const basic_iterator< value_type >& i ) : _s( i._s ), _i( i._i ) {}

        bool operator == ( const basic_iterator& i ) const      { return _s == i._s && _i == i._i; }
        bool operator != ( const basic_iterator& i ) const      { return ! operator == ( i ); }

        basic_iterator& operator ++ ()                          { if ( ++_i >= segment_size ) { _s = _s->next; _i = 0; } return *this; }
        basic_iterator operator ++ ( int )                      { basic_iterator i( *this ); operator ++ (); return i; }

        constT& operator * () const                             { return _s->v[ _i ]; }
        constT* operator -> () const                            { return _s->v[ _i ]; }

    private:

        friend class segment_list;
        basic_iterator( segment* s, size_t i )                  : _s( s ), _i( i ) {}

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

    segment_list()                                      : _head( nullptr ), _tail( nullptr ), _i( segment_size ) {}
    segment_list( segment_list&& s )                    : _head( nullptr ), _tail( nullptr ), _i( segment_size ) { swap( s ); }
    segment_list( const segment_list& s )               : _head( nullptr ), _tail( nullptr ), _i( segment_size ) { for ( const auto& v : s ) push_back( s ); }
    segment_list& operator = ( segment_list&& s )       { destroy(); swap( s ); return *this; }
    segment_list& operator = ( const segment_list& s )  { clear(); for ( const auto& v : s ) push_back( s ); return *this; }
    ~segment_list()                                     { destroy(); }

    bool empty() const                                  { return _head == _tail && ( _i == 0 || _head == nullptr ); }

    const_iterator cbegin() const                       { return const_iterator( _head, 0 ); }
    const_iterator begin() const                        { return const_iterator( _head, 0 ); }
    const_iterator cend() const                         { return const_iterator( _tail, _i ); }
    const_iterator end() const                          { return const_iterator( _tail, _i ); }
    const_reference back() const                        { assert( ! empty() ); if ( _i > 0 ) return _tail->v[ _i - 1 ]; else return _tail->prev->v[ segment_size - 1 ]; }

    iterator begin()                                    { return iterator( _head, 0 ); }
    iterator end()                                      { return iterator( _tail, _i ); }
    reference back()                                    { assert( ! empty() ); if ( _i > 0 ) return _tail->v[ _i - 1 ]; else return _tail->prev->v[ segment_size - 1 ]; }

    void push_back( T&& element )                       { new ( push_alloc() ) T( std::move( element ) ); _i += 1; }
    void push_back( const T& element )                  { new ( push_alloc() ) T( element ); _i += 1; }
    template < typename ... A > void emplace_back( A ... arguments ) { new ( push_alloc() ) T( std::forward< A ... >( arguments ... ) ); _i += 1; }
    void pop_back();
    void clear();

    void swap( segment_list& s )                        { std::swap( _head, s._head ); std::swap( _tail, s._tail ); std::swap( _i, s._i ); }

private:

    T* push_alloc();
    void destroy();

    segment* _head;
    segment* _tail;
    size_t _i;

};

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

    for ( segment* ss = _head; ss != _tail; ss = ss->next )
    {
        for ( size_t i = 0; i < segment_size; ++i )
            ss->v[ i ].~T();
    }

    for ( size_t i = 0; i < _i; ++i )
        _tail->v[ i ].~T();

    _tail = _head;
    _i = 0;
}

template < typename T, size_t segment_size >
T* segment_list< T, segment_size >::push_alloc()
{
    if ( _i >= segment_size )
    {
        if ( _tail->next )
        {
            _tail = _tail->next;
        }
        else
        {
            segment* s = (segment*)malloc( sizeof( segment ) + sizeof( T ) * segment_size );
            s->next = nullptr;
            s->prev = _tail;
            _tail->next = s;
            if ( _head == nullptr )
            {
                _head = s;
            }
            _tail = s;
        }
        _i = 0;
    }
    return _tail->v + _i;
}

template < typename T, size_t segment_size >
void segment_list< T, segment_size >::destroy()
{
    clear();
    for ( segment* ss = _head; ss != nullptr; )
    {
        segment* zz = ss;
        ss = ss->next;
        free( zz );
    }
}

}

#endif

