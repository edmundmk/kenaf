//
//  exception.h
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

#ifndef KF_EXCEPTION_H
#define KF_EXCEPTION_H

#include <exception>
#include "defines.h"
#include "runtime.h"

namespace kf
{

/*
    Exceptions thrown from script code.
*/

class KF_API exception : public std::exception
{
public:

    exception( const char* format, ... ) KF_PRINTF_FORMAT( 2, 3 );
    exception( const exception& e );
    exception& operator = ( const exception& e );
    ~exception() override;
    const char* what() const noexcept override;

protected:

    exception();
    char* _message;

};

class KF_API script_error : public exception
{
public:

    explicit script_error( value v );
    script_error( const script_error& e );
    script_error& operator = ( const script_error& e );
    ~script_error() override;
    struct value value() const noexcept;

protected:

    struct value _value;

};

class KF_API type_error : public exception
{
public:

    type_error( value v, const char* expected );
    type_error( const type_error& e );
    type_error& operator = ( const type_error& e );
    ~type_error() override;

};

class KF_API key_error : public exception
{
public:

    key_error( const char* format, ... ) KF_PRINTF_FORMAT( 2, 3 );
    key_error( const key_error& e );
    key_error& operator = ( const key_error& e );
    ~key_error() override;

};

class KF_API index_error : public exception
{
public:

    index_error( const char* format, ... ) KF_PRINTF_FORMAT( 2, 3 );
    index_error( const index_error& e );
    index_error& operator = ( const index_error& e );
    ~index_error() override;

};

}

#endif

