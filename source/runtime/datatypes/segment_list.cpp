//
//  segment_list.cpp
//
//  Created by Edmund Kapusniak on 10/11/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "segment_list.h"
#include <assert.h>
#include <vector>

int main( int argc, char* argv[] )
{
    srand( 0 );

    std::vector< int > v;
    kf::segment_list< int > l;

    for ( size_t jj = 0; jj < 2; ++jj )
    {
        for ( size_t i = 0; i < 1000000; ++i )
        {
            int q = rand();
            v.push_back( q );
            l.push_back( q );
        }

        size_t pop_count = rand() % v.size();
        for ( size_t i = 0; i < pop_count; ++i )
        {
            assert( v.back() == l.back() );
            v.pop_back();
            l.pop_back();
        }

        size_t i = 0;
        for ( int q : l )
        {
            assert( q == v[ i++ ] );
        }

        v.clear();
        l.clear();

        assert( l.empty() );
    }

    return EXIT_SUCCESS;
}
