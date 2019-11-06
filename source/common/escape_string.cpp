//
//  escape_string.cpp
//
//  Created by Edmund Kapusniak on 06/11/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "escape_string.h"

namespace kf
{

std::string escape_string( std::string_view s, size_t max_length )
{
    std::string escaped = "\"";
    for ( size_t i = 0; i < s.size(); ++i )
    {
        unsigned c = s[ i ];
        switch ( c )
        {
        case '\"':  escaped += "\\\"";  break;
        case '\\':  escaped += "\\\\";  break;
        case '\b':  escaped += "\\b";   break;
        case '\f':  escaped += "\\f";   break;
        case '\n':  escaped += "\\n";   break;
        case '\r':  escaped += "\\r";   break;
        case '\t':  escaped += "\\t";   break;
        case '\v':  escaped += "\\v";   break;
        default:
        {
            if ( c >= 0x20 && c < 0x7F )
            {
                escaped.push_back( c );
                break;
            }

            unsigned hex, count;
            size_t remaining = s.size() - i;
            if ( remaining >= 2 && c >= 0xC0 && c < 0xE0 )
            {
                escaped += "\\u";
                unsigned d = s[ ++i ];
                hex = ( ( c & 0x1F ) << 6 ) | ( d & 0x3F );
                count = 4;
            }
            else if ( remaining >= 3 && c >= 0xE0 && c < 0xF0 )
            {
                escaped += "\\u";
                unsigned d = s[ ++i ];
                unsigned e = s[ ++i ];
                hex = ( ( c & 0x0F ) << 12 ) | ( ( d & 0x3F ) << 6 ) | ( e & 0x3F );
                count = 4;
            }
            else if ( remaining >= 4 && c >= 0xF0 && c < 0xF8 )
            {
                escaped += "\\U+";
                unsigned d = s[ ++i ];
                unsigned e = s[ ++i ];
                unsigned f = s[ ++i ];
                hex = ( ( c & 0x07 ) << 18 ) | ( ( d & 0x3F ) << 12 ) | ( ( e & 0x3F ) << 6 ) | ( f & 0x3F );
                count = 6;
            }
            else
            {
                escaped += "\\x";
                hex = c;
                count = 2;
            }
            while ( count-- )
            {
                unsigned x = ( hex >> count * 4 ) & 0xF;
                if ( x >= 0 && x <= 9 )
                    escaped.push_back( '0' + x );
                else
                    escaped.push_back( 'A' + ( x - 10 ) );
            }
        }
        }

        if ( escaped.size() >= max_length )
        {
            escaped += "...";
            break;
        }
    }
    escaped += "\"";
    return escaped;
}

}

