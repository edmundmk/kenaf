//
//  escape_string.h
//
//  Created by Edmund Kapusniak on 06/11/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KF_ESCAPE_STRING_H
#define KF_ESCAPE_STRING_H

#include <string>
#include <string_view>

namespace kf
{

std::string escape_string( std::string_view s, size_t max_length );

}

#endif

