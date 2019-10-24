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

/*
    Compiles a script.
*/

#include <vector>
#include <string>
#include <string_view>

namespace kf
{

class compile_result;

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
    std::string message;
};

compile_result compile( std::string_view filename, std::string_view text, unsigned debug_print = PRINT_NONE );

class compile_result
{
public:

    compile_result();
    compile_result( compile_result&& r );
    compile_result& operator = ( compile_result&& r );
    ~compile_result();

    explicit operator bool () const;

    const void* data() const;
    size_t size() const;

    size_t diagnostic_count() const;
    const struct diagnostic& diagnostic( size_t index ) const;

private:

    friend compile_result compile( std::string_view filename, std::string_view text, unsigned debug_print );

    explicit compile_result( const void* data, std::vector< struct diagnostic >&& diagnostics );
    explicit compile_result( std::vector< struct diagnostic >&& diagnostics );
    compile_result( const compile_result& ) = delete;
    compile_result& operator = ( const compile_result& ) = delete;

    const void* _data;
    std::vector< struct diagnostic > _diagnostics;

};

}

#endif

