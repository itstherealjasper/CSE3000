cmake_minimum_required (VERSION 3.22)

project ("SAT-encoding")

add_subdirectory ("SAT-encoding")

# pblib
add_subdirectory ("pblib")
target_link_libraries (SAT-encoding PUBLIC pblib)
target_include_directories (SAT-encoding PUBLIC
                            "${PROJECT_BINARY_DIR}"
                            "${PROJECT_SOURCE_DIR}/pblib"	
)