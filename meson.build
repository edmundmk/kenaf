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
    'source/common/code.cpp',
    'source/common/imath.cpp',
    'source/compiler/ast.cpp',
    'source/compiler/ast_resolve.cpp',
    'source/compiler/code_unit.cpp',
    'source/compiler/compiler.cpp',
    'source/compiler/ir.cpp',
    'source/compiler/ir_alloc.cpp',
    'source/compiler/ir_build.cpp',
    'source/compiler/ir_emit.cpp',
    'source/compiler/ir_fold.cpp',
    'source/compiler/ir_foldk.cpp',
    'source/compiler/ir_live.cpp',
    'source/compiler/ir_regmap.cpp',
    'source/compiler/lexer.cpp',
    'source/compiler/source.cpp',
    'source/compiler/parser.cpp',
    'source/runtime/objects/array_object.cpp',
    'source/runtime/objects/cothread_object.cpp',
    'source/runtime/objects/function_object.cpp',
    'source/runtime/objects/lookup_object.cpp',
    'source/runtime/objects/object_model.cpp',
    'source/runtime/objects/string_object.cpp',
    'source/runtime/objects/table_object.cpp',
    'source/runtime/vm/vm_execute.cpp',
    'source/runtime/heap.cpp',
]

sources += lemon.process( 'source/compiler/grammar.lemon' )

libkenaf = library(
    'kenaf',
    include_directories : include_directories( 'include', 'source/compiler' ),
    sources : sources,
    install : true
)

sources = [
    'main.cpp'
]

kenaf = executable(
    'kenaf',
    sources : sources,
    include_directories : include_directories( 'include' ),
    link_with : [ libkenaf ],
    install : true
)

