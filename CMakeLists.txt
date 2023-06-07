# CMakeLists.txt
cmake_minimum_required(VERSION 3.5)

project(tactyk-p C CXX)
project(tactyk-p VERSION 0.8.1)

add_library(tactyk
  libtactyk/tactyk.c
  libtactyk/tactyk_alloc.c
  libtactyk/tactyk_asmvm.c
  libtactyk/tactyk_dblock.c
  libtactyk/tactyk_pl.c
  libtactyk/tactyk_util.c
  libtactyk/tactyk_assembler.c
  libtactyk/tactyk_emit.c
  libtactyk/tactyk_emit_svc.c
  libtactyk/tactyk_visa.c
  libtactyk/tactyk_debug.c
  libtactyk/tactyk_report.c
)

add_library(tactyk_aux
  aux/aux_util.c
  aux/aux_printit.c
)

add_executable(tdemo
  tdemo/main.c
  tdemo/aux_testlib.c
  tdemo/aux_sdl.c
)

add_executable(trun
  runtime/tactyk_basic_runtime.c
  runtime/tactyk_run_resource_pack.c
  runtime/tactyk_run_platform_functions.c
)

add_executable(ttest
  tactyktest/ttest.c
  tactyktest/tactyk_test__TestIt.c
  tactyktest/tactyk_test__util.c
  tactyktest/tactyk_test__platform_functions.c
  tactyktest/tactyk_test__var_functions.c
  tactyktest/tactyk_test__callbacks.c
)

find_package(SDL2 REQUIRED)

target_include_directories(tactyk_aux PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/aux
)

target_include_directories(tdemo PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/aux
  ${SDL2_INCLUDE_DIRS}
)
target_include_directories(tactyk PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_include_directories(ttest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_include_directories(trun PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/aux
)

target_link_libraries( tdemo tactyk tactyk_aux ${SDL2_LIBRARIES} )
target_link_libraries( trun tactyk tactyk_aux )
target_link_libraries( ttest tactyk )

add_compile_definitions(
    # USE_TACTYK_ALLOCATOR
    TACTYK_SHELL_INTERFACE
    TACTYK__VERSION="${CMAKE_PROJECT_VERSION}"
)

add_compile_options( -m64 -fPIC )

add_custom_command(
  TARGET ttest PRE_BUILD
  COMMAND ${CMAKE_COMMAND} -E create_symlink
          ${CMAKE_CURRENT_SOURCE_DIR}/rsc
          ${CMAKE_CURRENT_BINARY_DIR}/rsc
  COMMAND ${CMAKE_COMMAND} -E create_symlink
          ${CMAKE_CURRENT_SOURCE_DIR}/tests
          ${CMAKE_CURRENT_BINARY_DIR}/tests
)

add_custom_command(
  TARGET tdemo PRE_BUILD
  COMMAND ${CMAKE_COMMAND} -E create_symlink
          ${CMAKE_CURRENT_SOURCE_DIR}/rsc
          ${CMAKE_CURRENT_BINARY_DIR}/rsc
  COMMAND ${CMAKE_COMMAND} -E create_symlink
          ${CMAKE_CURRENT_SOURCE_DIR}/examples
          ${CMAKE_CURRENT_BINARY_DIR}/examples
)

add_custom_command(
  TARGET trun PRE_BUILD
  COMMAND ${CMAKE_COMMAND} -E create_symlink
          ${CMAKE_CURRENT_SOURCE_DIR}/rsc
          ${CMAKE_CURRENT_BINARY_DIR}/rsc
  COMMAND ${CMAKE_COMMAND} -E create_symlink
          ${CMAKE_CURRENT_SOURCE_DIR}/extras
          ${CMAKE_CURRENT_BINARY_DIR}/extras
)
