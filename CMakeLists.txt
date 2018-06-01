cmake_minimum_required(VERSION 3.11.3 FATAL_ERROR)
project(context VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
  add_definitions(-DWIN32_LEAN_AND_MEAN -DNOMINMAX -DWINVER=0x0A00 -D_WIN32_WINNT=0x0A00)
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE -D_ATL_SECURE_NO_DEPRECATE)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS -D_HAS_EXCEPTIONS=_CPPUNWIND)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  string(APPEND CMAKE_CXX_FLAGS " /await")
  foreach(config RELEASE RELWITHDEBINFO MINSIZEREL)
    string(APPEND CMAKE_CXX_FLAGS_${config} " /await:heapelide")
  endforeach()
else()
  add_compile_options(-fcoroutines-ts)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  string(REGEX REPLACE "[/-]W[0-9]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  string(APPEND CMAKE_CXX_FLAGS " /W4 /permissive-")
else()
  string(APPEND CMAKE_CXX_FLAGS " -Wall -Wextra -Wpedantic -Wconversion -Wno-unused-parameter")
  string(APPEND CMAKE_CXX_FLAGS " -fdiagnostics-absolute-paths -fcolor-diagnostics")
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER build)

file(GLOB sources src/asio.cpp src/mutex.cpp src/main.cpp)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX src FILES ${sources})

if(WIN32)
  file(GLOB native src/iocp.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  file(GLOB native src/epoll.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
  file(GLOB native src/kqueue.cpp)
endif()

if(native)
  source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX src FILES ${native})
  list(APPEND sources ${native})
endif()

add_executable(${PROJECT_NAME} ${sources})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR} src)

if(UNIX)
  find_package(Threads REQUIRED)
  target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
endif()

find_package(Boost 1.67.0 REQUIRED COMPONENTS system)
target_link_libraries(${PROJECT_NAME} PRIVATE Boost::system)

find_package(benchmark REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE benchmark::benchmark)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})
