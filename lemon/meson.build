
template_path = join_paths( get_option( 'prefix' ), get_option( 'datadir' ), 'lemon', 'lempar.c' )
lemon_exe = executable( 'lemon', sources : [ 'lemon.c' ], c_args : [ '-DTEMPLATE_PATH="' + template_path + '"' ], install : true )
install_data( 'lempar.c', install_dir : join_paths( get_option( 'datadir' ), 'lemon' ) )

lemon_script = find_program( 'lemon.py' )
lemon_actual = lemon_exe
lemon_output = [ '@BASENAME@.cpp', '@BASENAME@.h' ]
lemon_invoke = [ lemon_exe.full_path(), join_paths( meson.current_source_dir(), 'lempar.c' ), '@INPUT@', '@OUTPUT0@', '@OUTPUT1@' ]
lemon_gen = generator( lemon_script, depends : lemon_actual, output : lemon_output, arguments : lemon_invoke )
