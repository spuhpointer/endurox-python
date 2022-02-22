cmake_minimum_required(VERSION 3.4...3.18)
project(endurox-python)

add_subdirectory(pybind11)
pybind11_add_module(_endurox src/endurox.cpp)

#target_link_libraries(_math atmisrvinteg atmi ubf nstd)

target_link_libraries(_endurox PRIVATE pybind11::module atmisrvinteg atmi ubf nstd pthread rt)

# EXAMPLE_VERSION_INFO is defined by setup.py and passed into the C++ code as a
# define (VERSION_INFO) here.
target_compile_definitions(_endurox
                           PRIVATE VERSION_INFO=${EXAMPLE_VERSION_INFO})


set(CPACK_COMPONENTS_ALL_IN_ONE_PACKAGE 1)

set(CPACK_PACKAGE_NAME "endurox-python")
set(CPACK_PACKAGE_VENDOR "ACME")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "endurox-python - brought to you by Mavimax")
set(CPACK_PACKAGE_VERSION "1.0.2")
set(CPACK_PACKAGE_VERSION_MAJOR "1")
set(CPACK_PACKAGE_VERSION_MINOR "0")
set(CPACK_PACKAGE_VERSION_PATCH "2")
SET(CPACK_SYSTEM_NAME "i386")

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Mavimax SIA")
#set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.3.1-6), libgcc1 (>= 1:3.4.2-12), python2.6, libboost-program-options1.40.0 (>= 1.40.0)")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA "${CMAKE_SOURCE_DIR}/name_of_python_app/postinst;${CMAKE_SOURCE_DIR}/name_of_python_app/prerm;")
set(CPACK_SET_DESTDIR "ON")

include(CPack)
