//
//  lexer.h
//
//  Created by Edmund Kapusniak on 28/09/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

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

/*
    Analyze the source text and produce a stream of tokens.
*/

class lexer
{
public:

    explicit lexer( source* source );
    ~lexer();

    token lex();

private:

    source* _source;
    size_t _index;
    std::vector< char > _text;

    token lex_identifier();
    token lex_number();
    token lex_string();

    token assign_token( unsigned normal_kind, unsigned assign_kind, size_t sloc, const char* text );
    token source_token( unsigned kind, size_t sloc, const char* text );

    char newline();
    char peek( size_t i );
    char next( size_t count = 1 );
    bool eof();

};

}

