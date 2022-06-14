cmake_minimum_required (VERSION 3.13)

set(CMAKE_CXX_STANDARD 17)

project ("result-parser")

add_subdirectory ("result-parser")

target_link_libraries(result-parser PUBLIC stdc++fs)