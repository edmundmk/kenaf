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

lemon = generator(
    find_program( 'lemon.py' ),
    output : [ '@BASENAME@.cpp', '@BASENAME@.h' ],
    arguments : [ find_program( 'lemon' ).path(), '@INPUT@', '@OUTPUT0@', '@OUTPUT1@' ]
)

sources = [
    'source/common/code.cpp',
    'source/common/escape_string.cpp',
    'source/common/imath.cpp',
    'source/compile/ast.cpp',
    'source/compile/ast_resolve.cpp',
    'source/compile/code_unit.cpp',
    'source/compile/compile.cpp',
    'source/compile/ir.cpp',
    'source/compile/ir_alloc.cpp',
    'source/compile/ir_build.cpp',
    'source/compile/ir_emit.cpp',
    'source/compile/ir_fold.cpp',
    'source/compile/ir_foldk.cpp',
    'source/compile/ir_live.cpp',
    'source/compile/ir_regmap.cpp',
    'source/compile/lexer.cpp',
    'source/compile/source.cpp',
    'source/compile/parser.cpp',
    'source/runtime/corlib/cormath.cpp',
    'source/runtime/corlib/corobjects.cpp',
    'source/runtime/corlib/corprint.cpp',
    'source/runtime/objects/array_object.cpp',
    'source/runtime/objects/cothread_object.cpp',
    'source/runtime/objects/function_object.cpp',
    'source/runtime/objects/lookup_object.cpp',
    'source/runtime/objects/string_object.cpp',
    'source/runtime/objects/table_object.cpp',
    'source/runtime/objects/u64val_object.cpp',
    'source/runtime/call_stack.cpp',
    'source/runtime/collector.cpp',
    'source/runtime/errors.cpp',
    'source/runtime/execute.cpp',
    'source/runtime/heap.cpp',
    'source/runtime/runtime.cpp',
    'source/runtime/tick.cpp',
    'source/runtime/vmachine.cpp',
]

sources += lemon.process( 'source/compile/grammar.lemon' )

libkenaf = library(
    'kenaf',
    gnu_symbol_visibility : 'hidden',
    cpp_args : [ '-DKF_BUILD' ],
    include_directories : include_directories( 'include', 'source/compile' ),
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

