cmake_minimum_required(VERSION 3.16)

project(MBCRAWLER CXX)
include (GNUInstallDirs)

set (CMAKE_CXX_STANDARD 17)


set (FIND_LIBRARY_USE_LIB64_PATHS TRUE)

#[[
set (Boost_USE_MULTITHREADED ON)
set (Boost_NO_BOOST_CMAKE ON)
set (Boost_USE_STATIC_LIBS OFF)
find_package (Boost 1.67 REQUIRED)
#]]

add_subdirectory(3rdParty)


set (APPLICATION_TARGET_NAME mbcrawler)

add_executable (${APPLICATION_TARGET_NAME}
    src/main.cpp
    $<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/compile_commands.json> #<-- Having this as a dependency will cause the custom command below to execute...
    )

# Since this custom command's OUTPUT is a dependency of our executable target
# the custom command itself will be executed at build time to produce such OUTPUT 
add_custom_command (OUTPUT ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMAND ln -s ${CMAKE_BINARY_DIR}/compile_commands.json ${CMAKE_SOURCE_DIR}
    COMMENT "Symlinking compile_commands.json"
    DEPENDS ${CMAKE_BINARY_DIR}/compile_commands.json
    )

target_link_libraries (${APPLICATION_TARGET_NAME} PRIVATE MODBUS::static nlohmann_json::nlohmann_json)

target_compile_options (${APPLICATION_TARGET_NAME} PRIVATE $<$<CONFIG:Debug>:-fno-omit-frame-pointer>)

install (TARGETS ${APPLICATION_TARGET_NAME} RUNTIME DESTINATION bin)
