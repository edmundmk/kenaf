//
//  defines.h
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

#ifndef KF_DEFINES_H
#define KF_DEFINES_H

#if defined( __GNUC__ )
#define KF_PRINTF_FORMAT( x, y ) __attribute__(( format( printf, x, y ) ))
#else
#define KF_PRINTF_FORMAT( x, y )
#endif

#if defined( __GNUC__ )
#define KF_EXPORT __attribute__(( visibility( "default" ) ))
#define KF_IMPORT __attribute__(( visibility( "default" ) ))
#elif defined( _MSC_VER )
#define KF_EXPORT __declspec( dllexport )
#define KF_IMPORT __declspec( dllimport )
#else
#define KF_EXPORT
#define KF_IMPORT
#endif

#ifdef KF_BUILD
#define KF_API KF_EXPORT
#else
#define KF_API KF_IMPORT
#endif

#endif

