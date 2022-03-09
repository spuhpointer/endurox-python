cmake_minimum_required(VERSION 3.1)
project(endurox-python)
SET(CMAKE_BUILD_TYPE Debug)

#IF (DEFINE_SANITIZE)
#	SET(CMAKE_C_FLAGS "-fsanitize=address -fno-omit-frame-pointer -O0 -ggdb ${CMAKE_C_FLAGS}")
#        SET(CMAKE_CXX_FLAGS "-fsanitize=address -fno-omit-frame-pointer -O0 -ggdb ${CMAKE_CXX_FLAGS}")
#ENDIF()

set (CMAKE_CXX_STANDARD 14)

SET(SOURCE_DIR "src/endurox")
# Tell cmake that headers are in alse in source_dir
include_directories(${SOURCE_DIR})
SET(SOURCES "${SOURCE_DIR}/endurox.cpp" "${SOURCE_DIR}/xatmibuf.cpp" "${SOURCE_DIR}/bufconv.cpp")

#SET(TEST_DIR "tests")
#SET(TESTS ${SOURCES}
#    "${TEST_DIR}/test_main.cpp"
#    "${TEST_DIR}/test_math.cpp")

# Generate a test executable
include_directories(${SOURCE_DIR})
#add_executable("${PROJECT_NAME}_test" ${TESTS})

# Generate python module
add_subdirectory(pybind11)

pybind11_add_module(endurox ${SOURCES})
target_link_libraries(endurox PRIVATE atmisrvinteg atmi ubf nstd pthread rt)

