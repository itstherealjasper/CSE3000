cmake_minimum_required (VERSION 3.13)

set(CMAKE_CXX_STANDARD 17)

project ("performance-testing")

add_subdirectory ("performance-testing")

target_link_libraries(performance-testing PUBLIC stdc++fs)

# pblib
add_subdirectory ("pblib")
target_link_libraries (performance-testing PUBLIC pblib)
target_include_directories (performance-testing PUBLIC
                            "${PROJECT_BINARY_DIR}"
                            "${PROJECT_SOURCE_DIR}/pblib"	
)

# pumpkin-solver
add_subdirectory ("pumpkin-solver")
target_link_libraries (performance-testing PUBLIC pumpkin-solver)
target_include_directories (performance-testing PUBLIC
                            "${PROJECT_BINARY_DIR}"
                            "${PROJECT_SOURCE_DIR}/pumpkin-solver"	
)