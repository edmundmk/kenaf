#
#   kenaf
#
#   Created by Edmund Kapusniak on 28/09/2019.
#   Copyright © 2019 Edmund Kapusniak
#
#   Licensed under the MIT License. See LICENSE file in the project root for
#   full license information.
#

project( 'kenaf', 'c', 'cpp', default_options : [ 'cpp_std=c++17' ] )

headers = [
    'include/kenaf/compiler.h',
    'include/kenaf/defines.h',
    'include/kenaf/handles.h',
    'include/kenaf/runtime.h'
]

sources = [
    'source/common/code.cpp',
    'source/common/escape_string.cpp',
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
    'source/runtime/execute.cpp',
    'source/runtime/heap.cpp',
    'source/runtime/runtime.cpp',
    'source/runtime/tick.cpp',
    'source/runtime/vmachine.cpp',
]

subdir( 'lemon' )
sources += lemon_gen.process( 'source/compiler/grammar.lemon' )

cpp_args = [ '-DKF_BUILD' ]
if meson.get_compiler( 'cpp' ).get_argument_syntax() == 'msvc'
    cpp_args += [ '/wd4200' ]
endif

kenaf_lib = library( 'kenaf', sources : sources, include_directories : include_directories( 'include', 'source/compiler' ), cpp_args : cpp_args, gnu_symbol_visibility : 'hidden', dependencies : [ dependency( 'threads' ) ], install : true )
kenaf_lic = files( 'LICENSE' )
kenaf_dep = declare_dependency( include_directories : include_directories( 'include' ), link_with : kenaf_lib )
install_headers( headers, subdir : 'kenaf' )

executable( 'kenaf', sources : [ 'main.cpp' ], include_directories : include_directories( 'include' ), link_with : [ kenaf_lib ], install : true )

