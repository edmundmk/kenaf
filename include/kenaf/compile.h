//
//  compile.h
//
//  Created by Edmund Kapusniak on 19/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files (the "Software"),
//  to deal in the Software without restriction, including without limitation
//  the rights to use, copy, modify, merge, publish, distribute, sublicense,
//  and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//  DEALINGS IN THE SOFTWARE.
//

#ifndef KENAF_COMPILE_H
#define KENAF_COMPILE_H

#include <string_view>
#include "defines.h"

namespace kf
{

/*
    Compiler object.
*/

struct compiler;

KF_API compiler* create_compiler();
KF_API compiler* retain_compiler( compiler* c );
KF_API void release_compiler( compiler* c );

/*
    Compilation.
*/

KF_API bool compile( compiler* c, std::string_view path, std::string_view text );

/*
    Result of successful compilation is a bytecode block.  This is a block of
    bytes which can be freely copied around or serialized.
*/

KF_API const void* compiled_code( compiler* c );
KF_API size_t compiled_size( compiler* c );

/*
    Errors and warnings from failed compilation.
*/

enum diagnostic_kind { ERROR, WARNING };

struct diagnostic
{
    diagnostic_kind kind;
    unsigned line;
    unsigned column;
    std::string_view message;
};

KF_API size_t diagnostic_count( compiler* c );
KF_API diagnostic get_diagnostic( compiler* c, size_t index );

/*
    Debug printing stuff.
*/

enum
{
    PRINT_NONE          = 0,
    PRINT_AST_PARSED    = 1 << 0,
    PRINT_AST_RESOLVED  = 1 << 1,
    PRINT_IR_BUILD      = 1 << 2,
    PRINT_IR_FOLD       = 1 << 3,
    PRINT_IR_LIVE       = 1 << 4,
    PRINT_IR_FOLDK      = 1 << 5,
    PRINT_IR_FOLD_LIVE  = 1 << 6,
    PRINT_IR_ALLOC      = 1 << 7,
    PRINT_CODE          = 1 << 8,
};

KF_API void debug_print( compiler* c, unsigned debug_print );
KF_API void debug_print( const void* code, size_t size );

}

#endif

