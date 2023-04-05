# CMakeLists.txt
cmake_minimum_required(VERSION 3.5)

project(tactyk-run C CXX)

add_library(tactyk
  src/fibtest.c
  src/qstest.c
  src/tactyk_asmvm.c
  src/tactyk_dblock.c
  src/tactyk_pl.c
  src/tactyk_util.c
  src/tactyk_assembler.c
  src/tactyk_emit.c
  src/tactyk_visa.c
)

add_executable(tactyk-run 
  src/main.c
  src/aux_testlib.c
)

target_link_libraries( tactyk-run tactyk )

add_compile_definitions(
    TACTYK_SHELL_INTERFACE
)

add_compile_options( -m64 -fPIC )

add_custom_command(
  TARGET tactyk PRE_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory
          ${CMAKE_CURRENT_BINARY_DIR}/rsc
  COMMAND ${CMAKE_COMMAND} -E make_directory
          ${CMAKE_CURRENT_BINARY_DIR}/examples

  COMMAND ${CMAKE_COMMAND} -E create_symlink
          ${CMAKE_SOURCE_DIR}/rsc/tactyk_asmvm_header.asm
          ${CMAKE_CURRENT_BINARY_DIR}/rsc/tactyk_asmvm_header.asm
  COMMAND ${CMAKE_COMMAND} -E create_symlink
          ${CMAKE_SOURCE_DIR}/rsc/tactyk_core.visa
          ${CMAKE_CURRENT_BINARY_DIR}/rsc/tactyk_core.visa
  
  COMMAND ${CMAKE_COMMAND} -E create_symlink
          ${CMAKE_SOURCE_DIR}/examples/fib.tkp
          ${CMAKE_CURRENT_BINARY_DIR}/examples/fib.tkp
  COMMAND ${CMAKE_COMMAND} -E create_symlink
          ${CMAKE_SOURCE_DIR}/examples/lazy_quine.tkp
          ${CMAKE_CURRENT_BINARY_DIR}/examples/lazy_quine.tkp
)
