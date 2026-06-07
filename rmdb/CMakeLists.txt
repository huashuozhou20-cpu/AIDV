cmake_minimum_required(VERSION 3.16)
project(RMDB)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-Wall -O2 -g -ggdb3")
# set(CMAKE_CXX_FLAGS "-Wall -O3")

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -O0 -g")

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -flto")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG -march=native -flto")
# LTO requires link-time flags too
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-flto")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "-flto")

# AddressSanitizer — enabled via -DENABLE_ASAN=ON
option(ENABLE_ASAN "Build with AddressSanitizer" OFF)
if(ENABLE_ASAN)
    message(STATUS "AddressSanitizer ENABLED")
    set(ASAN_FLAGS "-fsanitize=address -fno-omit-frame-pointer -g -O1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${ASAN_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${ASAN_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
endif()

# ThreadSanitizer — enabled via -DENABLE_TSAN=ON
# Detects data races on the epoll worker pool, Connection::busy flag,
# BufferPoolManager async flushes, and all shared state accesses.
# NOTE: TSAN is incompatible with ASAN; enable only one at a time.
option(ENABLE_TSAN "Build with ThreadSanitizer" OFF)
if(ENABLE_TSAN)
    message(STATUS "ThreadSanitizer ENABLED")
    set(TSAN_FLAGS "-fsanitize=thread -fno-omit-frame-pointer -g -O1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TSAN_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${TSAN_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=thread")
endif()

# Memory-only mode: disable fdatasync to measure true CPU ceiling
option(DISABLE_FSYNC "Build without fdatasync (memory-only benchmark)" OFF)
if(DISABLE_FSYNC)
    message(STATUS "DISABLE_FSYNC: fdatasync stripped — memory-only mode")
    add_compile_definitions(DISABLE_FSYNC)
endif()

enable_testing()
add_subdirectory(src)
add_subdirectory(deps)
