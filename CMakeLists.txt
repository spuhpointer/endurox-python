cmake_minimum_required(VERSION 3.4...3.18)
project(cmake_example)

add_subdirectory(pybind11)
pybind11_add_module(_endurox src/endurox.cpp)

#target_link_libraries(_math atmisrvinteg atmi ubf nstd)

target_link_libraries(_endurox PRIVATE pybind11::module atmisrvinteg atmi ubf nstd pthread rt)

# EXAMPLE_VERSION_INFO is defined by setup.py and passed into the C++ code as a
# define (VERSION_INFO) here.
target_compile_definitions(_endurox
                           PRIVATE VERSION_INFO=${EXAMPLE_VERSION_INFO})

