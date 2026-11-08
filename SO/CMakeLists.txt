cmake_minimum_required(VERSION 3.25)
project(OSSIM C)

set(CMAKE_C_STANDARD 11)

add_compile_options(-Wall -Wextra -Wno-unused-parameter)

add_executable(ossim ossim.c queue.c scheduler.c virtmem.c)

add_executable(application application.c burst_queue.c)


set(OSSIM_ARCHIVE_NAME "entrega.zip" CACHE STRING "Nome do ficheiro criado pelo alvo archive")
set(OSSIM_ARCHIVE_DIR "${CMAKE_BINARY_DIR}" CACHE PATH "Pasta onde o alvo archive cria o zip")

file(GLOB OSSIM_ARCHIVE_ROOT CONFIGURE_DEPENDS RELATIVE "${CMAKE_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/*.c"
        "${CMAKE_SOURCE_DIR}/*.h"
        "${CMAKE_SOURCE_DIR}/*.csv"
        "${CMAKE_SOURCE_DIR}/*.sh"
        "${CMAKE_SOURCE_DIR}/*.md"
        "${CMAKE_SOURCE_DIR}/*.pdf"
        "${CMAKE_SOURCE_DIR}/*.py")

file(GLOB_RECURSE OSSIM_ARCHIVE_SCENARIOS CONFIGURE_DEPENDS RELATIVE "${CMAKE_SOURCE_DIR}"
        "${CMAKE_SOURCE_DIR}/cenarios/*"
        "${CMAKE_SOURCE_DIR}/tests/*"
        "${CMAKE_SOURCE_DIR}/resultados/*.csv"
        "${CMAKE_SOURCE_DIR}/resultados/*.log")

add_custom_target(archive
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OSSIM_ARCHIVE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E tar cf "${OSSIM_ARCHIVE_DIR}/${OSSIM_ARCHIVE_NAME}" --format=zip --
        CMakeLists.txt
        .gitignore .gitattributes
        ${OSSIM_ARCHIVE_ROOT}
        ${OSSIM_ARCHIVE_SCENARIOS}
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "A criar ${OSSIM_ARCHIVE_DIR}/${OSSIM_ARCHIVE_NAME}"
        VERBATIM)
