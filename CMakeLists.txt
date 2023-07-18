cmake_minimum_required(VERSION 3.5)

project(smart-contract VERSION 2.0.0)

include(ExternalProject)

find_package(eosio.cdt)

set(DEBUG FALSE CACHE BOOL "Preparing build contract")

ExternalProject_Add(
   swap.pcash
   SOURCE_DIR ${CMAKE_SOURCE_DIR}/swap.pcash
   BINARY_DIR ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/swap.pcash
   CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=${EOSIO_CDT_ROOT}/lib/cmake/eosio.cdt/EosioWasmToolchain.cmake -DBUILD_TESTS=${BUILD_TESTS} -DPREPROD=${PREPROD}
   UPDATE_COMMAND ""
   PATCH_COMMAND ""
   TEST_COMMAND ""
   INSTALL_COMMAND ""
   BUILD_ALWAYS 1
)

set(BUILD_TESTS FALSE CACHE BOOL "Build unit tests")

if(BUILD_TESTS AND ${CMAKE_BUILD_TYPE} MATCHES "Debug")
   message(STATUS "Downloading list.token source code.")

   ExternalProject_Add(
      list.token_sources
      DOWNLOAD_DIR ${CMAKE_SOURCE_DIR}/download/list.token/src
      DOWNLOAD_NO_PROGRESS TRUE
      TIMEOUT 60
      SOURCE_DIR ${CMAKE_SOURCE_DIR}/tests/list.token
      URL https://git.list.family/api/v4/projects/118/repository/archive?sha=develop
      HTTP_HEADER "$ENV{GITLAB_HTTP_HEADER}"
      CONFIGURE_COMMAND ""
      BUILD_COMMAND ""
      TEST_COMMAND   ""
      INSTALL_COMMAND ""
   )

   message(STATUS "Downloading list.token wasm & abi.")

   ExternalProject_Add(
      list.token
      DOWNLOAD_DIR ${CMAKE_SOURCE_DIR}/download/list.token
      DOWNLOAD_NO_PROGRESS TRUE
      TIMEOUT 60
      SOURCE_DIR ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/list.token
      URL https://git.list.family/api/v4/projects/118/jobs/artifacts/develop/download?job=build-develop
      HTTP_HEADER "$ENV{GITLAB_HTTP_HEADER}"
      CONFIGURE_COMMAND ""
      BUILD_COMMAND ""
      TEST_COMMAND   ""
      INSTALL_COMMAND ""
   )

   message(STATUS "Downloading token.pc source code.")

   ExternalProject_Add(
      token.pc_sources
      DOWNLOAD_DIR ${CMAKE_SOURCE_DIR}/download/token.pc/src
      DOWNLOAD_NO_PROGRESS TRUE
      TIMEOUT 60
      SOURCE_DIR ${CMAKE_SOURCE_DIR}/tests/token.pc
      URL https://git.list.family/api/v4/projects/322/repository/archive?sha=develop
      HTTP_HEADER "$ENV{GITLAB_HTTP_HEADER}"
      CONFIGURE_COMMAND ""
      BUILD_COMMAND ""
      TEST_COMMAND   ""
      INSTALL_COMMAND ""
   )

   message(STATUS "Downloading token.pc wasm & abi.")
   
   ExternalProject_Add(
      token.pc
      DOWNLOAD_DIR ${CMAKE_SOURCE_DIR}/download/token.pc
      DOWNLOAD_NO_PROGRESS TRUE
      TIMEOUT 60
      SOURCE_DIR ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/token.pc
      URL https://git.list.family/api/v4/projects/322/jobs/artifacts/develop/download?job=build-develop
      HTTP_HEADER "$ENV{GITLAB_HTTP_HEADER}"
      CONFIGURE_COMMAND ""
      BUILD_COMMAND ""
      TEST_COMMAND   ""
      INSTALL_COMMAND ""
   )

   message(STATUS "Building unit tests.")

   ExternalProject_Add(
     contracts_unit_tests
     LIST_SEPARATOR | # Use the alternate list separator
     CMAKE_ARGS -DCMAKE_BUILD_TYPE=${TEST_BUILD_TYPE} -DCMAKE_PREFIX_PATH=${TEST_PREFIX_PATH} -DCMAKE_FRAMEWORK_PATH=${TEST_FRAMEWORK_PATH} -DCMAKE_MODULE_PATH=${TEST_MODULE_PATH} -DEOSIO_ROOT=${EOSIO_ROOT} -DLLVM_DIR=${LLVM_DIR} -DBOOST_ROOT=${BOOST_ROOT}
     SOURCE_DIR ${CMAKE_SOURCE_DIR}/tests
     BINARY_DIR ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/tests
     BUILD_ALWAYS 1
     TEST_COMMAND   ""
     INSTALL_COMMAND ""
   )

else()
   message(STATUS "Unit tests will not be built. To build unit tests, set BUILD_TESTS to true.")
endif()
