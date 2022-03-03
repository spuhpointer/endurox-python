cmake_minimum_required(VERSION 3.1)
project(endurox-python)

SET(SOURCE_DIR "src/endurox")
# Tell cmake that headers are in alse in source_dir
include_directories(${SOURCE_DIR})
SET(SOURCES "${SOURCE_DIR}/endurox.cpp")

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

