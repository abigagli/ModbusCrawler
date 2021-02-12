cmake_minimum_required(VERSION 3.16)

project(MBCRAWLER LANGUAGES CXX VERSION 1.0)

include(GNUInstallDirs)
include(FetchContent)

set (CMAKE_CXX_STANDARD 17)


set (FIND_LIBRARY_USE_LIB64_PATHS TRUE)

#find_package (TaskScheduler 1.0 REQUIRED)


#[[
set (Boost_USE_MULTITHREADED ON)
set (Boost_NO_BOOST_CMAKE ON)
set (Boost_USE_STATIC_LIBS OFF)
find_package (Boost 1.67 REQUIRED)
#]]

set (BUILD_TASKSCHEDULER_TESTAPP OFF CACHE STRING "")
FetchContent_Declare(
    TaskSchedulerPrj
    GIT_REPOSITORY https://github.com/abigagli/TaskScheduler.git
    )

FetchContent_MakeAvailable(TaskSchedulerPrj)

add_subdirectory(3rdParty)


set (APPLICATION_TARGET_NAME mbcrawler)

add_executable (${APPLICATION_TARGET_NAME}
    src/main.cpp
    src/meas_config.cpp
    src/meas_scheduler.cpp
    $<$<CONFIG:Debug>:${CMAKE_SOURCE_DIR}/compile_commands.json> #<-- Having this as a dependency will cause the custom command below to execute...
    )

# Since this custom command's OUTPUT is a dependency of our executable target
# the custom command itself will be executed at build time to produce such OUTPUT 
add_custom_command (OUTPUT ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMAND ln -s ${CMAKE_BINARY_DIR}/compile_commands.json ${CMAKE_SOURCE_DIR}
    COMMENT "Symlinking compile_commands.json"
    )

target_link_libraries (${APPLICATION_TARGET_NAME}
    PRIVATE MODBUS::static nlohmann_json::nlohmann_json TaskScheduler::TaskScheduler)

target_compile_options (${APPLICATION_TARGET_NAME} PRIVATE $<$<CONFIG:Debug>:-fno-omit-frame-pointer>)

install (TARGETS ${APPLICATION_TARGET_NAME} RUNTIME DESTINATION bin)
