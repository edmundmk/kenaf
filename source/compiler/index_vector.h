//
//  index_vector.h
//
//  Created by Edmund Kapusniak on 20/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef INDEX_VECTOR_H
#define INDEX_VECTOR_H

/*
    A lot of our data structures are stored in vectors, with indexes that have
    a maximum value.  This helper class specializes std::vector to check these
    limits.
*/

#include <vector>
#include <assert.h>

namespace kf
{

template < typename T, size_t limit >
struct index_vector : public std::vector< T >
{
    typename std::vector< T >::const_reference at( typename std::vector< T >::size_type pos ) const = delete;
    typename std::vector< T >::reference at( typename std::vector< T >::size_type pos ) = delete;
    void push_back( T&& value ) = delete;
    void push_back( const T& value ) = delete;

    unsigned append( T&& value )
    {
        unsigned index = this->size();
        if ( index >= limit )
        {
            throw std::out_of_range( "limit exceeded" );
        }
        std::vector< T >::push_back( std::move( value ) );
        return index;
    }

    unsigned append( const T& value )
    {
        unsigned index = this->size();
        if ( index >= limit )
        {
            throw std::out_of_range( "limit exceeded" );
        }
        std::vector< T >::push_back( value );
        return index;
    }

    typename std::vector< T >::const_reference operator [] ( size_t index ) const
    {
        assert( index < this->size() );
        return std::vector< T >::operator [] ( index );
    }

    typename std::vector< T >::reference operator [] ( size_t index )
    {
        assert( index < this->size() );
        return std::vector< T >::operator [] ( index );
    }
};

}

#endif

