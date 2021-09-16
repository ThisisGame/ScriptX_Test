cmake_minimum_required(VERSION 3.17)
project(ScriptXTest)

set(CMAKE_CXX_STANDARD 17)



include(ScriptX/CMakeLists.txt)


include_directories(lua/src)
include_directories(ScriptX/src/include)

link_libraries(ScriptX)

file(GLOB_RECURSE lua_c lua/src/*.c)

add_executable(ScriptXTest main.cpp ${lua_c})