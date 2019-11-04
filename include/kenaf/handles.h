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
#include <memory>

namespace kf
{

struct runtime_release { void operator () ( runtime* r ) const { release_runtime( r ); } };
typedef std::unique_ptr< runtime, runtime_release > runtime_handle;
inline runtime_handle make_runtime() { return runtime_handle( create_runtime() ); }

struct context_release { void operator () ( context* c ) const { release_context( c ); } };
typedef std::unique_ptr< context, context_release > context_handle;
inline context_handle make_context( runtime* r ) { return context_handle( create_context( r ) ); }

struct compilation_release { void operator () ( compilation* c ) const { release_compilation( c ); } };
typedef std::unique_ptr< compilation, compilation_release > compilation_handle;

}

#endif

