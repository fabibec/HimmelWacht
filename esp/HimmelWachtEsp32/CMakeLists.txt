# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

# Bluepad32 requires C11 or C++17
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Set path to Bluepad32
set(BLUEPAD32_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../bluepad32-v4.2.0)
set(BTSTACK_ROOT ${BLUEPAD32_ROOT}/external/btstack)
set(EXTRA_COMPONENT_DIRS
    ${BLUEPAD32_ROOT}/src/components
    ${CMAKE_CURRENT_SOURCE_DIR}/components
    ${CMAKE_CURRENT_SOURCE_DIR}/components/drivers
    ${CMAKE_CURRENT_SOURCE_DIR}/components/interfaces
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# "Trim" the build. Include the minimal set of components, main, and anything it depends on.
idf_build_set_property(MINIMAL_BUILD ON)
project(HimmelWachtEsp32)
