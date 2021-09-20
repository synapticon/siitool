cmake_minimum_required(VERSION 3.12)

project(siitool LANGUAGES C VERSION 1.4)

if(${WIN32})
  execute_process(COMMAND conan install ${CMAKE_SOURCE_DIR})
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_BINARY_DIR}")

find_package(LibXml2 REQUIRED)

add_executable(siitool crc8.c crc8.h esi.c esi.h esifile.c esifile.h sii.c sii.h main.c)
target_link_libraries(siitool LibXml2::LibXml2)
target_compile_definitions(siitool PRIVATE VERSION="${CMAKE_PROJECT_VERSION}")
