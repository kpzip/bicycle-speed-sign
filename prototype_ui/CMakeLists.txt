cmake_minimum_required(VERSION 3.10)
project(radar CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-Wall -Wextra)

find_package(SDL3 REQUIRED)

add_executable(radar
	main.cpp
	receive.cpp
)

target_link_libraries(radar
	${SDL3_LIBRARIES}
)
