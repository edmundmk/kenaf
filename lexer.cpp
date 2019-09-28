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
        size_t sloc = _index;

        char c = peek( 0 );
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
                return source_token( TOKEN_NE, sloc );
            }
            break;

        case '"':
            return lex_string();

        case '#':
            next();
            return source_token( TOKEN_HASH, sloc );

        case '%':
            next();
            return assign_token( TOKEN_PERCENT, TOKEN_MOD_ASSIGN, sloc );

        case '&':
            next();
            return assign_token( TOKEN_AMPERSAND, TOKEN_BITAND_ASSIGN, sloc );

        case '(':
            next();
            return source_token( TOKEN_LPN, sloc );

        case ')':
            next();
            return source_token( TOKEN_RPN, sloc );

        case '*':
            next();
            return assign_token( TOKEN_ASTERISK, TOKEN_MUL_ASSIGN, sloc );

        case '+':
            next();
            return assign_token( TOKEN_PLUS, TOKEN_ADD_ASSIGN, sloc );

        case ',':
            next();
            return source_token( TOKEN_COMMA, sloc );

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
                return assign_token( TOKEN_MINUS, TOKEN_SUB_ASSIGN, sloc );
            }

        case '.':
            if ( peek( 1 ) == '.' && peek( 2 ) == '.' )
            {
                next( 3 );
                return source_token( TOKEN_ELLIPSIS, sloc );
            }
            else if ( peek( 1 ) >= '0' && peek( 1 ) <= '9' )
            {
                return lex_number();
            }
            else
            {
                next();
                return source_token( TOKEN_PERIOD, sloc );
            }

        case '/':
            if ( peek( 1 ) == '*' )
            {
                // Block comment.
                c = next( 2 );
                while ( peek( 0 ) != '*' || peek( 1 ) != '/' )
                {
                    if ( c == '\0' && eof() )
                    {
                        _source->error( sloc, "unterminated block comment" );
                        break;
                    }
                    else if ( c == '\r' || c == '\n' )
                    {
                        c = newline();
                    }
                    else
                    {
                        c = next();
                    }
                }
                if ( c != '\0' )
                {
                    next( 2 );
                }
                continue;
            }
            else if ( peek( 1 ) == '/' )
            {
                next( 2 );
                return assign_token( TOKEN_INTDIV, TOKEN_INTDIV_ASSIGN, sloc );
            }
            else
            {
                next();
                return assign_token( TOKEN_SOLIDUS, TOKEN_DIV_ASSIGN, sloc );
            }

        case ':':
            next();
            return source_token( TOKEN_COLON, sloc );

        case ';':
            next();
            return source_token( TOKEN_SEMICOLON, sloc );

        case '<':
            if ( peek( 1 ) == '<' )
            {
                next( 2 );
                return assign_token( TOKEN_LSHIFT, TOKEN_LSHIFT_ASSIGN, sloc );
            }
            else
            {
                next();
                return assign_token( TOKEN_LT, TOKEN_LE, sloc );
            }

        case '=':
            next();
            return assign_token( TOKEN_ASSIGN, TOKEN_EQ, sloc );

        case '>':
            if ( peek( 1 ) == '>' )
            {
                next( 2 );
                return assign_token( TOKEN_RSHIFT, TOKEN_RSHIFT_ASSIGN, sloc );
            }
            else
            {
                next();
                return assign_token( TOKEN_GT, TOKEN_GE, sloc );
            }

        case '[':
            next();
            return source_token( TOKEN_LSQ, sloc );

        case ']':
            next();
            return source_token( TOKEN_RSQ, sloc );

        case '^':
            next();
            return assign_token( TOKEN_CARET, TOKEN_BITXOR_ASSIGN, sloc );

        case '{':
            next();
            return source_token( TOKEN_LBR, sloc );

        case '|':
            next();
            return assign_token( TOKEN_VBAR, TOKEN_BITOR_ASSIGN, sloc );

        case '}':
            next();
            return source_token( TOKEN_RBR, sloc );

        case '~':
            if ( peek( 1 ) == '>' && peek( 2 ) == '>' )
            {
                next( 3 );
                return assign_token( TOKEN_ASHIFT, TOKEN_ASHIFT_ASSIGN, sloc );
            }
            else
            {
                next();
                return source_token( TOKEN_TILDE, sloc );
            }

        case '\0':
            if ( eof() )
            {
                return source_token( TOKEN_EOF, sloc );
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

        // UNEXPECTED!
    }
}

token lexer::lex_identifier()
{
    size_t sloc = _index;

    char c = peek( 0 );
    while ( ( c >= 'A' && c <= 'Z' ) || ( c >= 'a' && c <= 'z' ) || ( c >= '0' && c <= '9' ) || c == '_' )
    {
        c = next();
    }

    return source_token( TOKEN_IDENTIFIER, sloc );
}

static int digit( char c )
{
    if ( c >= '0' && c <= '9' )
    {
        return c - '0';
    }
    else if ( c >= 'A' && c <= 'Z' )
    {
        return 10 + ( c - 'A' );
    }
    else if ( c >= 'a' && c <= 'z' )
    {
        return 10 + ( c - 'a' );
    }
    return INT_MAX;
}

token lexer::lex_number()
{
    size_t sloc = _index;

    int base = 10;
    if ( peek( 0 ) == '0' )
    {
        if ( peek( 1 ) == 'b' )
        {
            next( 2 );
            base = 2;
        }
        else if ( peek( 1 ) == 'o' )
        {
            next( 2 );
            base = 8;
        }
        else if ( peek( 1 ) == 'x' )
        {
            next( 2 );
            base = 16;
        }
        else if ( peek( 1 ) >= '0' && peek( 1 ) <= '9' )
        {
            _source->error( sloc, "invalid C-style octal literal" );
        }
    }

    bool has_digit = false;
    char c = peek( 0 );
    while ( digit( c ) < base )
    {
        has_digit = true;
        _text.push_back( c );
        c = next();
    }

    bool real = false;
    if ( ( base == 10 || base == 16 ) && c == '.' )
    {
        real = true;
        _text.push_back( c );
        c = next();

        while ( digit( c ) < base )
        {
            has_digit = true;
            _text.push_back( c );
            c = next();
        }
    }

    if ( ! has_digit )
    {
        _source->error( sloc, "numeric literal has no digits" );
    }

    if ( ( base == 10 && c == 'e' ) || ( base == 16 && c == 'p' ) )
    {
        real = true;
        _text.push_back( c );
        c = next();

        if ( c == '+' || c == '-' )
        {
            _text.push_back( c );
            c = next();
        }

        if ( digit( c ) >= 10 )
        {
            _source->error( sloc, "missing exponent in numeric literal" );
        }

        while ( digit( c ) < 10 )
        {
            _text.push_back( c );
            c = next();
        }
    }

    if ( digit( c ) < INT_MAX )
    {
        _source->error( _index, "invalid suffix on numeric literal" );
    }

    double n;
    if ( real )
    {
        if ( base == 16 )
            _text.insert( _text.begin(), { '0', 'x' } );
        _text.push_back( '\0' );

        n = strtod( _text.data(), nullptr );
    }
    else
    {
        uintmax_t i = 0;
        for ( char c : _text )
        {
            i *= base;
            i += digit( c );
        }

        n = (double)i;
    }

    _text.clear();
    token token;
    token.kind = TOKEN_NUMBER;
    token.sloc = sloc;
    token.n = n;
    return token;
}

token lexer::lex_string()
{
    size_t sloc = _index;

    assert( peek( 0 ) == '"' );
    char c = next();

    size_t lower = _index;
    bool source = true;

    // Body of string.
    while ( c != '"' )
    {
        if ( c == '\\' )
        {
            size_t xloc = _index;

            unsigned x = 0;
            c = next();

            switch ( c )
            {
            case '"':
            case '\\':
            case '/':
                _text.push_back( c );
                c = next();
                break;

            case 'b':
                _text.push_back( '\b' );
                c = next();
                break;

            case 'f':
                _text.push_back( '\f' );
                c = next();
                break;

            case 'n':
                _text.push_back( '\n' );
                c = next();
                break;

            case 'r':
                _text.push_back( '\r' );
                c = next();
                break;

            case 'x':
                c = next();
                c = string_hex( c, 2, &x );
                _text.push_back( x );
                break;

            case 'u':
                c = next();
                c = string_hex( c, 4, &x );
                string_utf8( xloc, x );
                break;

            case 'U':
                c = next();
                if ( c == '+' )
                {
                    c = next();
                    c = string_hex( c, 6, &x );
                    string_utf8( xloc, x );
                }
                else
                {
                    _source->error( xloc, "Unicode escape must have form 'U+000000'" );
                    _text.push_back( 'U' );
                }
                break;

            default:
                _source->error( xloc, "invalid string escape" );
                _text.push_back( c );
                c = next();
                break;
            }

            source = false;
        }
        else if ( c == '\r' || c == '\n' )
        {
            _source->error( sloc, "newline in string" );
            _text.push_back( '\n' );
            source = false;
            c = newline();
        }
        else if ( c == '\0' && eof() )
        {
            _source->error( sloc, "end of file in string" );
            break;
        }
        else
        {
            _text.push_back( c );
            c = next();
        }
    }

    // Skip closing quote.
    size_t upper = _index;
    if ( c == '"' )
    {
        c = next();
    }

    // Return string.
    token token;
    token.kind = TOKEN_STRING;
    token.sloc = sloc;
    if ( source )
    {
        // String is exactly as it appears in the source text.
        token.text = &_source->text[ lower ];
        token.size = upper - lower;
    }
    else
    {
        // String has escapes, so we need to copy the text we accumulated.
        token.size = _text.size();
        token.text = (const char*)malloc( token.size );
        memcpy( (char*)token.text, _text.data(), token.size );
        _source->strings.push_back( { token.text, token.size } );
    }

    _text.clear();
    return token;
}

char lexer::string_hex( char c, size_t count, unsigned* x )
{
    *x = 0;
    while ( count-- )
    {
        int d = digit( c );
        if ( d < 16 )
        {
            *x = ( *x << 4 ) | d;
        }
        else
        {
            _source->error( _index, "invalid hexadecimal escape" );
            break;
        }
        c = next();
    }
    return c;
}

void lexer::string_utf8( srcloc xloc, unsigned codepoint )
{
    if ( codepoint <= 0x7F )
    {
        _text.push_back( codepoint );
    }
    else if ( codepoint <= 0x7FF )
    {
        _text.push_back( 0xC0 | ( ( codepoint >> 6  ) & 0x1F ) );
        _text.push_back( 0x80 | ( ( codepoint       ) & 0x3F ) );
    }
    else if ( codepoint <= 0xFFFF )
    {
        _text.push_back( 0xE0 | ( ( codepoint >> 12 ) & 0x0F ) );
        _text.push_back( 0x80 | ( ( codepoint >> 6  ) & 0x3F ) );
        _text.push_back( 0x80 | ( ( codepoint       ) & 0x3F ) );
    }
    else if ( codepoint <= 0x10FFFF )
    {
        _text.push_back( 0xF0 | ( ( codepoint >> 18 ) & 0x07 ) );
        _text.push_back( 0x80 | ( ( codepoint >> 12 ) & 0x3F ) );
        _text.push_back( 0x80 | ( ( codepoint >> 6  ) & 0x3F ) );
        _text.push_back( 0x80 | ( ( codepoint       ) & 0x3F ) );
    }
    else
    {
        // Encode U+FFFD REPLACEMENT CHARACTER
        _text.push_back( 0xEF );
        _text.push_back( 0xBF );
        _text.push_back( 0xBD );

        // Report error.
        _source->error( xloc, "invalid Unicode codepoint U+%06X", codepoint );
    }
}

token lexer::assign_token( unsigned normal_kind, unsigned assign_kind, size_t sloc )
{
    if ( peek( 0 ) == '=' )
    {
        next();
        return source_token( assign_kind, sloc );
    }
    else
    {
        return source_token( normal_kind, sloc );
    }
}

token lexer::source_token( unsigned kind, size_t sloc )
{
    token token;
    token.kind = kind;
    token.sloc = sloc;
    token.text =  &_source->text[ sloc ];
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

