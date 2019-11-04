//
//  index_vector.h
//
//  Created by Edmund Kapusniak on 20/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KF_INDEX_VECTOR_H
#define KF_INDEX_VECTOR_H

/*
    A lot of our data structures are stored in vectors, with indexes that have
    a maximum value.  This helper class specializes std::vector to check these
    limits.
*/

#include <vector>
#include <assert.h>
#include <stdexcept>

namespace kf
{

template < typename T, size_t limit >
struct index_vector : public std::vector< T >
{
    unsigned append( T&& value )
    {
        unsigned index = this->size();
        if ( index >= limit )
        {
            throw std::length_error( "limit exceeded" );
        }
        std::vector< T >::push_back( std::move( value ) );
        return index;
    }

    unsigned append( const T& value )
    {
        unsigned index = this->size();
        if ( index >= limit )
        {
            throw std::length_error( "limit exceeded" );
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

