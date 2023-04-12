# CMakeLists.txt
cmake_minimum_required(VERSION 3.5)

project(tactyk-p C CXX)
project(tactyk-p VERSION 0.5.1)

add_library(tactyk
  libtactyk/tactyk.c
  libtactyk/tactyk_asmvm.c
  libtactyk/tactyk_dblock.c
  libtactyk/tactyk_pl.c
  libtactyk/tactyk_util.c
  libtactyk/tactyk_assembler.c
  libtactyk/tactyk_emit.c
  libtactyk/tactyk_visa.c
  libtactyk/tactyk_debug.c
)

add_executable(tdemo
  tdemo/main.c
  tdemo/aux_testlib.c
  tdemo/aux_sdl.c
)

find_package(SDL2 REQUIRED)

target_include_directories(tdemo PRIVATE 
  ${CMAKE_SOURCE_DIR}/include
  ${SDL2_INCLUDE_DIRS}
)
target_include_directories(tactyk PRIVATE 
  ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries( tdemo tactyk ${SDL2_LIBRARIES} )

add_compile_definitions(
    # USE_TACTYK_RANDOM_ALLOCATOR
    TACTYK_SHELL_INTERFACE
    TACTYK__VERSION="${CMAKE_PROJECT_VERSION}"
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
  COMMAND ${CMAKE_COMMAND} -E create_symlink
          ${CMAKE_SOURCE_DIR}/examples/hello_sdl.tkp
          ${CMAKE_CURRENT_BINARY_DIR}/examples/hello_sdl.tkp
  COMMAND ${CMAKE_COMMAND} -E create_symlink
          ${CMAKE_SOURCE_DIR}/examples/recursive.tkp
          ${CMAKE_CURRENT_BINARY_DIR}/examples/recursive.tkp
  COMMAND ${CMAKE_COMMAND} -E create_symlink
          ${CMAKE_SOURCE_DIR}/examples/julia.tkp
          ${CMAKE_CURRENT_BINARY_DIR}/examples/julia.tkp
)
