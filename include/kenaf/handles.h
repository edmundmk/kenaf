//
//  handles.h
//
//  Created by Edmund Kapusniak on 04/11/2019.
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

#ifndef KENAF_HANDLES_H
#define KENAF_HANDLES_H

#include "runtime.h"
#include "compiler.h"
#include <utility>
#include <stdexcept>
#include <string>

namespace kf
{

/*
    Handles for reference-counted objects.
*/

template < typename T, T* (*retain)( T* ), void (*release)( T* ) >
class basic_handle
{
public:

    static basic_handle wrap( T* rawp )                 { basic_handle p; p._p = rawp; return p; }

    basic_handle()                                      : _p( nullptr ) {}
    explicit basic_handle( T* rawp )                    : _p( nullptr ) { reset( rawp ); }
    basic_handle( basic_handle&& p )                    : _p( nullptr ) { swap( p ); }
    basic_handle( const basic_handle& p )               : _p( nullptr ) { reset( p._p ); }
    basic_handle& operator = ( basic_handle&& p )       { reset(); swap( p ); return *this; }
    basic_handle& operator = ( const basic_handle& p )  { reset( p._p ); return *this; }
    ~basic_handle()                                     { reset( nullptr ); }

    explicit operator bool () const                     { return _p != nullptr; }
    T* get() const                                      { return _p; }

    basic_handle copy()                                 { return basic_handle( _p ); }
    void swap( basic_handle& p )                        { std::swap( _p, p._p ); }
    void reset( T* rawp = nullptr )                     { if ( _p ) release( _p ); _p = rawp ? retain( rawp ) : nullptr; }

private:

    T* _p;

};

typedef basic_handle< runtime, retain_runtime, release_runtime > runtime_handle;
typedef basic_handle< context, retain_context, release_context > context_handle;
typedef basic_handle< stack_trace, retain_stack_trace, release_stack_trace > stack_trace_handle;
typedef basic_handle< compiler, retain_compiler, release_compiler > compiler_handle;

inline runtime_handle make_runtime() { return runtime_handle::wrap( create_runtime() ); }
inline context_handle make_context( runtime* r ) { return context_handle::wrap( create_context( r ) ); }
inline compiler_handle make_compiler() { return compiler_handle::wrap( create_compiler() ); }

/*
    Handle to value.
*/

class handle
{
public:

    static handle wrap( value rawv )                    { handle v; v._v = rawv; return v; }

    handle()                                            : _v( null_value ) {}
    explicit handle( value rawv )                       : _v( retain( rawv ) ) {}
    handle( handle&& v )                                : _v( null_value ) { swap( v ); }
    handle( const handle& v )                           : _v( v._v.v ? retain( v._v ) : null_value ) {}
    handle& operator = ( handle&& v )                   { reset(); swap( v ); return *this; }
    handle& operator = ( const handle& v )              { reset( v._v ); return *this; }
    ~handle()                                           { if ( _v.v ) release( _v ); }

    operator value () const                             { return _v; }
    value get() const                                   { return _v; }

    void swap( handle& v )                              { std::swap( _v, v._v ); }
    void reset( value rawv = { 0 } )                    { release( _v ); _v = retain( rawv ); }

private:

    value _v;

};

/*
    Call frame scope and helpers.
*/

class scoped_frame
{
public:

    scoped_frame()                                  : _frame{ nullptr, 0 } {}
    ~scoped_frame()                                 { if ( _frame.sp ) pop_frame( &_frame ); }
    operator frame* ()                              { return &_frame; }

private:

    scoped_frame( const scoped_frame& ) = delete;
    scoped_frame& operator = ( const scoped_frame& ) = delete;

    frame _frame;

};

inline value call( value function, std::initializer_list< value > arguments )
{
    return call( function, arguments.begin(), arguments.size() );
}

template < typename ... A > inline value call( value function, A ... argvalues )
{
    return call( function, { std::forward< A >( argvalues ) ... } );
}

/*
    Exception.
*/

class script_error : public std::runtime_error
{
public:

    script_error( error_kind error, std::string_view message, stack_trace* backtrace, value raised );
    script_error( const script_error& e ) = default;
    script_error& operator = ( const script_error& e ) = default;
    ~script_error() = default;

    error_kind error() const noexcept;
    stack_trace* backtrace() const noexcept;
    value raised() const noexcept;

protected:

    error_kind _error;
    stack_trace_handle _backtrace;
    handle _raised;

};

inline script_error::script_error( error_kind error, std::string_view message, stack_trace* backtrace, value raised )
    :   std::runtime_error( std::string( message ) )
    ,   _error( error )
    ,   _backtrace( backtrace )
    ,   _raised( raised )
{
}

inline error_kind script_error::error() const noexcept
{
    return _error;
}

inline stack_trace* script_error::backtrace() const noexcept
{
    return _backtrace.get();
}

inline value script_error::raised() const noexcept
{
    return _raised;
}

}

#endif

