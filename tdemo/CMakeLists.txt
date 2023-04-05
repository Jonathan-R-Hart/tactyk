#add_executable(
#  tactyk-run
3  main.c
3  aux_testlib.c
#)
#
#target_link_libraries(tactyk-run libtactyk)
#
#add_compile_definitions(
#    TACTYK_SHELL_INTERFACE
#)
#
3add_custom_command(
#  TARGET tactyk-run PRE_BUILD
#  COMMAND ${CMAKE_COMMAND} -E make_directory
#          ${CMAKE_CURRENT_BINARY_DIR}/rsc
#  COMMAND ${CMAKE_COMMAND} -E make_directory
#          ${CMAKE_CURRENT_BINARY_DIR}/examples
#
#  COMMAND ${CMAKE_COMMAND} -E create_symlink
#          ${CMAKE_SOURCE_DIR}/rsc/tactyk_asmvm_header.asm
#          ${CMAKE_CURRENT_BINARY_DIR}/rsc/tactyk_asmvm_header.asm
#  COMMAND ${CMAKE_COMMAND} -E create_symlink
#          ${CMAKE_SOURCE_DIR}/rsc/tactyk_core.visa
#          ${CMAKE_CURRENT_BINARY_DIR}/rsc/tactyk_core.visa
#  
#  COMMAND ${CMAKE_COMMAND} -E create_symlink
#          ${CMAKE_SOURCE_DIR}/examples/fib.tkp
#          ${CMAKE_CURRENT_BINARY_DIR}/examples/fib.tkp
#  COMMAND ${CMAKE_COMMAND} -E create_symlink
#          ${CMAKE_SOURCE_DIR}/examples/lazy_quine.tkp
#          ${CMAKE_CURRENT_BINARY_DIR}/examples/lazy_quine.tkp
#)
