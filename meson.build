#
#   kenaf
#
#   Created by Edmund Kapusniak on 28/09/2019.
#   Copyright © 2019 Edmund Kapusniak
#
#   Licensed under the MIT License. See LICENSE file in the project root for
#   full license information.
#

project( 'kenaf', 'cpp', default_options : [ 'cpp_std=c++17' ] )

lemon_program = find_program( 'lemon' )
lemon_buildsh = find_program( 'lemon.sh' )
lemon = generator( lemon_buildsh, output : [ '@BASENAME@.cpp', '@BASENAME@.h' ], arguments : [ lemon_program.path(), '@INPUT@', '@OUTPUT0@', '@OUTPUT1@' ] )

sources = [
]

sources += lemon.process( 'kenaf.lemon' )

libkenaf = library(
    'kenaf',
    sources : sources,
    install : true
)

sources = [
    'main.cpp'
]

kenaf = executable(
    'kenaf',
    sources : sources,
    install : true
)

