cmake_minimum_required(VERSION 3.16)

project(MBCRAWLER LANGUAGES CXX VERSION 1.0)
set (APPLICATION_TARGET_NAME mbcrawler)

include(GNUInstallDirs)
include(FetchContent)

# See comment below fow why we need to add Threads as an explicit dep...
find_package (Threads)

option (USE_STANDALONE_ASIO "" ON)

set (CMAKE_CXX_STANDARD 17)

add_executable (${APPLICATION_TARGET_NAME})
set_target_properties (${APPLICATION_TARGET_NAME} PROPERTIES DEBUG_POSTFIX "D")

set (FIND_LIBRARY_USE_LIB64_PATHS TRUE)

add_subdirectory(3rdParty)

target_sources (${APPLICATION_TARGET_NAME}
    PRIVATE
    src/main.cpp
    src/meas_config.cpp
    src/meas_scheduler.cpp
    src/meas_reporter.cpp
    src/json_support.cpp
    src/periodic_scheduler.cpp
    ${CMAKE_SOURCE_DIR}/compile_commands.json #<-- Having this as a dependency will cause the custom command below to execute...
    )

# Since this custom command's OUTPUT is a dependency of our executable target
# the custom command itself will be executed at build time to produce such OUTPUT 
add_custom_command (OUTPUT ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMAND ln -sf ${CMAKE_BINARY_DIR}/compile_commands.json ${CMAKE_SOURCE_DIR}
    DEPENDS ${CMAKE_BINARY_DIR}/compile_commands.json
    COMMENT "Symlinking compile_commands.json"
    )

if (USE_STANDALONE_ASIO)
    target_include_directories (${APPLICATION_TARGET_NAME}
        PRIVATE
        ${CMAKE_BINARY_DIR}/3rdParty/asio/asio-src/include
        )
    target_compile_definitions (${APPLICATION_TARGET_NAME}
        PRIVATE
        ASIO_STANDALONE)
else (USE_STANDALONE_ASIO)
    target_link_libraries (${APPLICATION_TARGET_NAME}
        PRIVATE
        Boost::headers)
endif (USE_STANDALONE_ASIO)

target_link_libraries (${APPLICATION_TARGET_NAME}
    PRIVATE
    MODBUS::static
    nlohmann_json::nlohmann_json
    Threads::Threads) #<<--- Omitting this makes linking fail on Linux: See https://discourse.cmake.org/t/boost-process-target-doesnt-exist-for-thread-linking/2113

target_compile_options (${APPLICATION_TARGET_NAME} PRIVATE $<$<CONFIG:Debug>:-fno-omit-frame-pointer>)

install (TARGETS ${APPLICATION_TARGET_NAME} RUNTIME DESTINATION bin)
