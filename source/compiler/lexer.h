//
//  lexer.h
//
//  Created by Edmund Kapusniak on 28/09/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef KF_LEXER_H
#define KF_LEXER_H

#include <string_view>
#include "grammar.h"
#include "source.h"

namespace kf
{

/*
    Tokens refer directly to the source text, or they're numbers, or they're
    strings with text stored in the source object.
*/

#define TOKEN_EOF 0

struct token
{
    unsigned kind;
    srcloc sloc;
    union
    {
        struct { const char* text; size_t size; };
        double n;
    };
};

std::string spelling( const token& token );

/*
    Analyze the source text and produce a stream of tokens.
*/

class lexer
{
public:

    lexer( report* report, source* source );
    ~lexer();

    token lex();

private:

    report* _report;
    source* _source;
    size_t _index;
    std::vector< char > _text;

    token lex_identifier();
    token lex_number();
    token lex_string();

    char string_hex( char c, size_t count, unsigned* x );
    void string_utf8( srcloc sloc, unsigned codepoint );

    token assign_token( unsigned normal_kind, unsigned assign_kind, size_t sloc );
    token source_token( unsigned kind, size_t sloc );

    char newline();
    char peek( size_t i );
    char next( size_t count = 1 );
    bool eof();

};

}

#endif

