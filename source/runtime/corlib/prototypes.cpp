//
//  prototypes.cpp
//
//  Created by Edmund Kapusniak on 03/10/2019.
//  Copyright © 2019 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information.
//

#include "prototypes.h"
#include "kenaf/runtime.h"
#include "../vm/vm_context.h"

namespace kf
{

/*

    global

    def superof( v ) end
    def getkey( o, key ) end
    def setkey( o, key ) end
    def haskey( o, key ) end
    def delkey( o, key ) end
    def keys( object ) end

    def object end

    def string is object
        def self( o ) return to_string( o ) end
    end

    def array is object
        def resize( n ) end
        def append( n ) end
        def extend( x ... ) end
        def push( v ) end
        def pop() end
    end

    def table is object
        def has( k ) end
        def get( k, default/null ) end
        def del( k ) end
    end

    def function is object end

    def cothread is object
        def done() end
    end

    def number is object
        def self( o ) return to_number( o ) end
    end

    def bool is object end

*/

void expose_prototypes( vm_context* vm )
{



}

}

