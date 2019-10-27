//
//  hash_table.cpp
//
//  Created by Edmund Kapusniak on 27/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "hash_table.h"
#include <unordered_map>

int main( int argc, char* argv[] )
{
    srand( clock() );

    kf::hash_table< int, int > imap;
    std::unordered_map< int, int > umap;

    for ( size_t i = 0; i < 10000; ++i )
    {
        int key = rand();
        int val = rand();
        imap.insert_or_assign( key, val );
        umap.insert_or_assign( key, val );
    }

    for ( auto i = imap.cbegin(); i != imap.cend(); ++i )
    {
        auto j = umap.find( i->first );
        if ( j == umap.end() )
        {
            fprintf( stderr, "imap entry not found in umap\n" );
            return EXIT_FAILURE;
        }
        if ( j->second != i->second )
        {
            fprintf( stderr, "imap value mismatch to umap\n" );
            return EXIT_FAILURE;
        }
        umap.erase( j );
    }

    if ( ! umap.empty() )
    {
        fprintf( stderr, "iteration of imap skipped some keys\n" );
        return EXIT_FAILURE;
    }

    imap.clear();

    for ( size_t i = 0; i < 10000; ++i )
    {
        int key = rand();
        int val = rand();
        imap.insert_or_assign( key, val );
        umap.insert_or_assign( key, val );
    }

    for ( auto i = umap.cbegin(); i != umap.cend(); ++i )
    {
        auto j = imap.find( i->first );
        if ( j == imap.end() )
        {
            fprintf( stderr, "umap entry not found in imap\n" );
            return EXIT_FAILURE;
        }
        if ( j->second != i->second )
        {
            fprintf( stderr, "umap value mismatch to imap\n" );
            return EXIT_FAILURE;
        }
        imap.erase( j );
    }

    if ( ! imap.empty() )
    {
        fprintf( stderr, "imap is not empty after erasing all keys" );
        return EXIT_FAILURE;
    }

    umap.clear();

    for ( size_t i = 0; i < 10000; ++i )
    {
        int key = rand();
        int val = rand();
        imap.insert_or_assign( key, val );
    }

    for ( auto i = imap.begin(); i != imap.end(); )
    {
        i = imap.erase( i );
    }

    if ( ! imap.empty() )
    {
        fprintf( stderr, "erasing during iteration left non-empty imap" );
        return EXIT_FAILURE;
    }

    for ( size_t i = 0; i < 10000; ++i )
    {
        int key = rand();
        int val = rand();
        imap.insert_or_assign( key, val );
        umap.insert_or_assign( key, val );
    }

    for ( auto i = imap.begin(); i != imap.end(); )
    {
        if ( rand() % 2 )
        {
            umap.erase( i->first );
            i = imap.erase( i );
        }
        else
        {
            ++i;
        }
    }

    for ( auto i = imap.cbegin(); i != imap.cend(); ++i )
    {
        auto j = umap.find( i->first );
        if ( j == umap.end() )
        {
            fprintf( stderr, "after erasure, imap entry not found in umap\n" );
            return EXIT_FAILURE;
        }
        if ( j->second != i->second )
        {
            fprintf( stderr, "after erasure, imap value mismatch to umap\n" );
            return EXIT_FAILURE;
        }
    }

    for ( auto i = umap.cbegin(); i != umap.cend(); ++i )
    {
        auto j = imap.find( i->first );
        if ( j == imap.end() )
        {
            fprintf( stderr, "after erasure, umap entry not found in imap\n" );
            return EXIT_FAILURE;
        }
        if ( j->second != i->second )
        {
            fprintf( stderr, "after erasure, umap value mismatch to imap\n" );
            return EXIT_FAILURE;
        }
    }

    printf( "OK!\n" );
    return EXIT_SUCCESS;
}
