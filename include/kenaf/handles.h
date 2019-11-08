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
#include "compile.h"
#include <utility>

namespace kf
{

template < typename T, T* (*retain)( T* ), void (*release)( T* ) >
class basic_handle
{
public:

    basic_handle()                                  : _p( nullptr ) {}
    explicit basic_handle( T* p )                   : _p( retain( p ) ) {}
    basic_handle( basic_handle&& p )                : _p( nullptr ) { swap( p ); }
    basic_handle& operator = ( basic_handle&& p )   { reset(); swap( p ); return *this; }
    ~basic_handle()                                 { if ( _p ) release( _p ); }

    explicit operator bool () const                 { return _p != nullptr; }
    T* get() const                                  { return _p; }

    basic_handle copy()                             { return basic_handle( _p ); }
    void swap( basic_handle& p )                    { std::swap( _p, p._p ); }
    void reset( T* p = nullptr )                    { if ( _p ) release( _p ); _p = p; if ( _p ) retain( _p ); }

private:

    T* _p;

};

typedef basic_handle< runtime, retain_runtime, release_runtime > runtime_handle;
typedef basic_handle< context, retain_context, release_context > context_handle;
typedef basic_handle< compilation, retain_compilation, release_compilation > compilation_handle;

inline runtime_handle make_runtime() { return runtime_handle( create_runtime() ); }
inline context_handle make_context( runtime* r ) { return context_handle( create_context( r ) ); }

class handle
{
public:

    handle()                                        : _v( { 0 } ) {}
    explicit handle( value v )                      : _v( retain( v ) ) {}
    handle( handle&& v )                            : _v( { 0 } ) { swap( v ); }
    handle& operator = ( handle&& v )               { reset(); swap( v ); return *this; }
    ~handle()                                       { if ( _v.v ) release( _v ); }

    operator value () const                         { return _v; }
    value get() const                               { return _v; }

    handle copy()                                   { return handle( _v ); }
    void swap( handle& v )                          { std::swap( _v, v._v ); }
    void reset( value v = { 0 } )                   { if ( _v.v ) release( _v ); _v = v; if ( _v.v ) retain( _v ); }

private:

    value _v;

};

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

}

#endif

