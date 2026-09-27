cmake_minimum_required(VERSION 3.20)
project(LLVMFrontendCompiler CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find LLVM
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Include directories
include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/include)

# LLVM definitions
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# Sources
add_executable(kaleidoscope
    main.cpp
    lexer/lexer.cpp
    parser/parser.cpp
    parser/ast.cpp
    logger/logger.cpp
    codegen/codegen.cpp
)

# Link LLVM libraries
llvm_map_components_to_libnames(llvm_libs
    Core
    Support
    OrcJIT
    native
    IRReader
    Analysis
    TransformUtils
    Passes
)

target_link_libraries(kaleidoscope PRIVATE ${llvm_libs})