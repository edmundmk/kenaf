#!/usr/bin/env python3

import os
import sys
import subprocess

lemon = sys.argv[ 1 ]
input = sys.argv[ 2 ]
output_c = sys.argv[ 3 ]
output_h = sys.argv[ 4 ]

subprocess.check_call( [ lemon, input ] )

basename = os.path.splitext( input )[ 0 ]
os.remove( basename + ".out" )

with open( basename + ".c", 'r' ) as file:
    text = file.read()
text = text.replace( basename + ".c", output_c )
with open( output_c, 'w' ) as file:
    file.write( text )

os.remove( basename + ".c" )
os.replace( basename + ".h", output_h )

