//
//  exception.cpp
//
//  Created by Edmund Kapusniak on 01/11/2019
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

#include "kenaf/exception.h"
#include "../common/escape_string.h"
#include "objects/object_model.h"
#include "objects/string_object.h"

namespace kf
{

/*
    Exceptions.
*/

static char* format_message( const char* format, va_list ap )
{
    va_list aq;
    va_copy( aq, ap );
    int size = vsnprintf( nullptr, 0, format, aq );
    va_end( aq );
    char* message = (char*)malloc( size + 1 );
    message[ size ] = '\0';
    va_copy( aq, ap );
    vsnprintf( message, size, format, aq );
    va_end( aq );
    return message;
}

static char* KF_PRINTF_FORMAT( 1, 2 ) format_message( const char* format, ... )
{
    va_list ap;
    va_start( ap, format );
    char* message = format_message( format, ap );
    va_end( ap );
    return message;
}

static char* format_value( value v )
{
    if ( box_is_number( v ) )
    {
        return format_message( "%f", unbox_number( v ) );
    }
    else if ( box_is_string( v ) )
    {
        string_object* s = unbox_string( v );
        char* message = (char*)malloc( s->size + 1 );
        memcpy( message, s->text, s->size + 1 );
        return message;
    }
    else if ( box_is_object( v ) )
    {
        const char* type_name = "object";
        switch ( header( unbox_object( v ) )->type )
        {
        case LOOKUP_OBJECT:             type_name = "lookup";           break;
        case ARRAY_OBJECT:              type_name = "array";            break;
        case TABLE_OBJECT:              type_name = "table";            break;
        case FUNCTION_OBJECT:           type_name = "function";         break;
        case NATIVE_FUNCTION_OBJECT:    type_name = "native function";  break;
        case COTHREAD_OBJECT:           type_name = "cothread";         break;
        default: break;
        }
        return format_message( "<%s %p>", type_name, unbox_object( v ) );
    }
    else if ( box_is_bool( v ) )
    {
        return format_message( "%s", v.v == boxed_true.v ? "true" : "false" );
    }
    else
    {
        return format_message( "null" );
    }
}

exception::exception()
    :   _message( nullptr )
{
}

exception::exception( const char* format, ... )
{
    va_list ap;
    va_start( ap, format );
    _message = format_message( format, ap );
    va_end( ap );
}

exception::exception( const exception& e )
{
    size_t size = strlen( e._message );
    _message = (char*)malloc( size + 1 );
    memcpy( _message, e._message, size + 1 );
}

exception& exception::operator = ( const exception& e )
{
    free( _message );
    size_t size = strlen( e._message );
    _message = (char*)malloc( size + 1 );
    memcpy( _message, e._message, size + 1 );
    return *this;
}

exception::~exception()
{
    free( _message );
}

const char* exception::what() const noexcept
{
    return _message;
}

script_error::script_error( struct value v )
    :   _value( retain( v ) )
{
    _message = format_value( v );
}

script_error::script_error( const script_error& e )
    :   exception( e )
    ,   _value( retain( e._value ) )
{
}

script_error& script_error::operator = ( const script_error& e )
{
    exception::operator = ( e );
    release( _value );
    _value = retain( e._value );
    return *this;
}

script_error::~script_error()
{
    release( _value );
}

value script_error::value() const noexcept
{
    return _value;
}

type_error::type_error( value v, const char* expected )
{
    const char* type_name = "object";
    if ( box_is_number( v ) )
    {
        _message = format_message( "%f is not %s", unbox_number( v ), expected );
        return;
    }
    else if ( box_is_string( v ) )
    {
        string_object* s = unbox_string( v );
        std::string escaped = escape_string( std::string_view( s->text, s->size ), 10 );
        _message = format_message( "%s is not %s", escaped.c_str(), expected );
        return;
    }
    else if ( box_is_object( v ) )
    {
        switch ( header( unbox_object( v ) )->type )
        {
        case LOOKUP_OBJECT:             type_name = "lookup";           break;
        case ARRAY_OBJECT:              type_name = "array";            break;
        case TABLE_OBJECT:              type_name = "table";            break;
        case FUNCTION_OBJECT:           type_name = "function";         break;
        case NATIVE_FUNCTION_OBJECT:    type_name = "native function";  break;
        case COTHREAD_OBJECT:           type_name = "cothread";         break;
        default: break;
        }
    }
    else if ( box_is_bool( v ) )
    {
        if ( v.v == boxed_true.v )
            type_name = "true";
        else
            type_name = "false";
    }
    else
    {
        type_name = "null";
    }
    _message = format_message( "%s is not %s", type_name, expected );
}

type_error::type_error( const type_error& e )
    :   exception( e )
{
}

type_error& type_error::operator = ( const type_error& e )
{
    exception::operator = ( e );
    return *this;
}

type_error::~type_error()
{
}

key_error::key_error( const char* format, ... )
{
    va_list ap;
    va_start( ap, format );
    _message = format_message( format, ap );
    va_end( ap );
}

key_error::key_error( const key_error& e )
    :   exception( e )
{
}

key_error& key_error::operator = ( const key_error& e )
{
    exception::operator = ( e );
    return *this;
}

key_error::~key_error()
{
}

index_error::index_error( const char* format, ... )
{
    va_list ap;
    va_start( ap, format );
    _message = format_message( format, ap );
    va_end( ap );
}

index_error::index_error( const index_error& e )
    :   exception( e )
{
}

index_error& index_error::operator = ( const index_error& e )
{
    exception::operator = ( e );
    return *this;
}

index_error::~index_error()
{
}

}

