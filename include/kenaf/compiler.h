//
//  compiler.h
//
//  Created by Edmund Kapusniak on 19/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KENAF_COMPILER_H
#define KENAF_COMPILER_H

#include <string_view>

namespace kf
{

/*
    Warnings and errors from compilation.
*/

enum diagnostic_kind
{
    ERROR,
    WARNING,
};

struct diagnostic
{
    diagnostic_kind kind;
    unsigned line;
    unsigned column;
    std::string_view message;
};

/*
    The result of compiling script source.  The bytecode is a block of bytes
    which can be freely copied around or serialized.
*/

struct compilation;
struct code_view { const void* code; size_t size; };

compilation* compilation_retain( compilation* cn );
void compilation_release( compilation* cn );

bool compilation_success( compilation* cn );
code_view compilation_code( compilation* cn );
size_t compilation_diagnostic_count( compilation* cn );
diagnostic compilation_diagnostic( compilation* cn, size_t index );

/*
    Compile source text.
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

compilation* compile( std::string_view filename, std::string_view text, unsigned debug_print = PRINT_NONE );

/*
    Print bytecode
*/

void debug_print_code( const void* code, size_t size );

}

#endif

