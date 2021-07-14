cmake_minimum_required(VERSION 3.16)

project(MODBUS LANGUAGES CXX VERSION 1.0)

include(GNUInstallDirs)
include(FetchContent)

set (CMAKE_CXX_STANDARD 17)
set (FIND_LIBRARY_USE_LIB64_PATHS TRUE)

option (USE_STANDALONE_ASIO "" ON)

add_subdirectory(3rdParty)

add_custom_target(
    copy_current_compile_commands
    DEPENDS ${CMAKE_SOURCE_DIR}/compile_commands.json
    )

add_custom_command (
    OUTPUT ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/compile_commands.json ${CMAKE_SOURCE_DIR}
    # Since we use copy_if_different, the target of the copy (i.e. ${CMAKE_SOURCE_DIR}/compile_commands.json)
    # is a "BYPRODUCT" since its timestamp may _not_ be newer than this command dependency (i.e. the source of the copy)
    # (simply because the source compile_commands.json gets a newer timestamp, but the content is the same so it is
    # not copied).
    # In that case this command would keep re-executing (even if it wouldn't actually copy anything since the contents is the same)
    # just because the OUTPUT's timestamp keeps staying the same while the dependency (i.e. the source of the copy) gets
    # a newer timestamp.
    # So by explicitly 'touching' it, we ensure its timestamp gets updated even if its contents were not, and so
    # the command doesn't actually execute unless there's a new/different source compile_commands.json
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_SOURCE_DIR}/compile_commands.json
    DEPENDS ${CMAKE_BINARY_DIR}/compile_commands.json
    COMMENT "Ensuring current compile_commands.json is copied to root folder"
    )

add_subdirectory(src)