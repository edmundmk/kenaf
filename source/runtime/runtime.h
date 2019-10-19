//
//  runtime.h
//
//  Created by Edmund Kapusniak on 12/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#ifndef RUNTIME_H
#define RUNTIME_H

/*
    The main runtime structure.  There is one runtime per-thread.

    Runtimes have several important maps which are used for object layout and
    garbage collection:

      - The roots map keeps a record of all roots.
      - The key name map is used to intern string when they are used as keys.
      - The key slot map is used to look up slot indexes.
      - The layout map links layouts to their child layouts.
      - The prototype map links prototype objects to their root layout.

    Many of these maps have weak keys, so the garbage collector is aware of
    them and cooperates to sweep entries that are no longer live.
*/

namespace kf
{

struct runtime
{


};


}

#endif

