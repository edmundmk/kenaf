//
//  lexer.cpp
//
//  Created by Edmund Kapusniak on 28/09/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "lexer.h"

namespace kf
{

lexer::lexer( source* source )
    :   _source( source )
    ,   _index( 0 )
{
}

lexer::~lexer()
{
}

token lexer::lex()
{
    while ( true )
    {
        char c = peek( 0 );

        const char* text = &_source->text[ _index ];
        size_t sloc = _index;

        switch ( c )
        {
        case ' ':
        case '\t':
            next();
            continue;

        case '\r':
        case '\n':
            newline();
            continue;

        case '!':
            if ( peek( 1 ) == '=' )
            {
                next( 2 );
                return source_token( TOKEN_NE, sloc, text );
            }
            break;

        case '"':
            return lex_string();

        case '#':
            next();
            return source_token( TOKEN_HASH, sloc, text );

        case '%':
            next();
            return assign_token( TOKEN_PERCENT, TOKEN_MOD_ASSIGN, sloc, text );

        case '&':
            next();
            return assign_token( TOKEN_AMPERSAND, TOKEN_BITAND_ASSIGN, sloc, text );

        case '(':
            next();
            return source_token( TOKEN_LPN, sloc, text );

        case ')':
            next();
            return source_token( TOKEN_RPN, sloc, text );

        case '*':
            next();
            return assign_token( TOKEN_ASTERISK, TOKEN_MUL_ASSIGN, sloc, text );

        case '+':
            next();
            return assign_token( TOKEN_PLUS, TOKEN_ADD_ASSIGN, sloc, text );

        case ',':
            next();
            return source_token( TOKEN_COMMA, sloc, text );

        case '-':
            if ( peek( 1 ) == '-' )
            {
                // Inline comment.
                c = next( 2 );
                while ( c != '\r' && c != '\n' && ( c != '\0' || ! eof() ) )
                {
                    c = next();
                }
                continue;
            }
            else
            {
                next();
                return assign_token( TOKEN_MINUS, TOKEN_SUB_ASSIGN, sloc, text );
            }

        case '.':
            if ( peek( 1 ) == '.' && peek( 2 ) == '.' )
            {
                next( 3 );
                return source_token( TOKEN_ELLIPSIS, sloc, text );
            }
            else if ( peek( 1 ) == '.' )
            {
                next( 2 );
                return assign_token( TOKEN_CONCAT, TOKEN_CONCAT_ASSIGN, sloc, text );
            }
            else if ( peek( 1 ) >= '0' && peek( 1 ) <= '9' )
            {
                return lex_number();
            }
            else
            {
                next();
                return source_token( TOKEN_PERIOD, sloc, text );
            }

        case '/':
            if ( peek( 1 ) == '*' )
            {
                // Block comment.
                next( 2 );
                while ( peek( 0 ) != '*' || peek( 1 ) != '/' )
                {
                    if ( c == '\0' && eof() )
                    {
                        _source->error( sloc, "unterminated block comment" );
                        break;
                    }
                    if ( c == '\r' || c == '\n' )
                    {
                        c = newline();
                    }
                    else
                    {
                        c = next();
                    }
                }
                continue;
            }
            else if ( peek( 1 ) == '/' )
            {
                next( 2 );
                return assign_token( TOKEN_INTDIV, TOKEN_INTDIV_ASSIGN, sloc, text );
            }
            else
            {
                next();
                return assign_token( TOKEN_SOLIDUS, TOKEN_DIV_ASSIGN, sloc, text );
            }

        case ':':
            next();
            return source_token( TOKEN_COLON, sloc, text );

        case ';':
            next();
            return source_token( TOKEN_SEMICOLON, sloc, text );

        case '<':
            if ( peek( 1 ) == '<' )
            {
                next( 2 );
                return assign_token( TOKEN_LSHIFT, TOKEN_LSHIFT_ASSIGN, sloc, text );
            }
            else
            {
                next();
                return assign_token( TOKEN_LT, TOKEN_LE, sloc, text );
            }

        case '=':
            next();
            return assign_token( TOKEN_ASSIGN, TOKEN_EQ, sloc, text );

        case '>':
            if ( peek( 1 ) == '>' )
            {
                next( 2 );
                return assign_token( TOKEN_RSHIFT, TOKEN_RSHIFT_ASSIGN, sloc, text );
            }
            else
            {
                next();
                return assign_token( TOKEN_GT, TOKEN_GE, sloc, text );
            }

        case '[':
            next();
            return source_token( TOKEN_LSQ, sloc, text );

        case ']':
            next();
            return source_token( TOKEN_RSQ, sloc, text );

        case '^':
            next();
            return assign_token( TOKEN_CARET, TOKEN_BITXOR_ASSIGN, sloc, text );

        case '{':
            next();
            return source_token( TOKEN_LBR, sloc, text );

        case '|':
            next();
            return assign_token( TOKEN_VBAR, TOKEN_BITOR_ASSIGN, sloc, text );

        case '}':
            next();
            return source_token( TOKEN_RBR, sloc, text );

        case '~':
            if ( peek( 1 ) == '>' && peek( 2 ) == '>' )
            {
                next( 3 );
                return assign_token( TOKEN_ASHIFT, TOKEN_ASHIFT_ASSIGN, sloc, text );
            }
            else
            {
                next();
                return source_token( TOKEN_TILDE, sloc, text );
            }

        case '\0':
            if ( eof() )
            {
                return source_token( TOKEN_EOF, sloc, text );
            }
            break;
        }

        if ( c >= '0' && c <= '9' )
        {
            return lex_number();
        }

        if ( ( c >= 'A' && c <= 'Z' ) || ( c >= 'a' && c <= 'z' ) || c == '_' )
        {
            return lex_identifier();
        }


        // Check for other tokens.
    }
}

token lexer::assign_token( unsigned normal_kind, unsigned assign_kind, size_t sloc, const char* text )
{
    if ( peek( 0 ) == '=' )
    {
        next();
        return source_token( assign_kind, sloc, text );
    }
    else
    {
        return source_token( normal_kind, sloc, text );
    }
}

token lexer::source_token( unsigned kind, size_t sloc, const char* text )
{
    token token;
    token.kind = kind;
    token.sloc = sloc;
    token.text = text;
    token.size = _index - sloc;
    return token;
}

char lexer::newline()
{
    char c;
    if ( peek( 0 ) == '\r' && peek( 1 ) == '\n' )
        c = next( 2 );
    else
        c = next( 1 );
    _source->newline( _index );
    return c;
}

char lexer::peek( size_t i )
{
    assert( _index + i < _source->text.size() );
    return _source->text[ _index + i ];
}

char lexer::next( size_t count )
{
    _index += count;
    return peek( 0 );
}

bool lexer::eof()
{
    return _index >= _source->size();
}

}

