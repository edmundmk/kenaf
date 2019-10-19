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
    'common/code.cpp',
    'parser/ast.cpp',
    'parser/ast_resolve.cpp',
    'parser/ir.cpp',
    'parser/ir_alloc.cpp',
    'parser/ir_build.cpp',
    'parser/ir_fold.cpp',
    'parser/ir_foldk.cpp',
    'parser/ir_live.cpp',
    'parser/ir_regmap.cpp',
    'parser/lexer.cpp',
    'parser/source.cpp',
    'parser/parser.cpp',
    'runtime/heap.cpp',
]

sources += lemon.process( 'parser/grammar.lemon' )

libkenaf = library(
    'kenaf',
    include_directories : include_directories( 'parser' ),
    sources : sources,
    install : true
)

sources = [
    'main.cpp'
]

kenaf = executable(
    'kenaf',
    sources : sources,
    include_directories : libkenaf.private_dir_include(),
    link_with : [ libkenaf ],
    install : true
)

