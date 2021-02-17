cmake_minimum_required(VERSION 3.16)

project(MBCRAWLER LANGUAGES CXX VERSION 1.0)
set (APPLICATION_TARGET_NAME mbcrawler)

include(GNUInstallDirs)
include(FetchContent)

option (USE_ASIO_BASED_SCHEDULER "" ON)

set (CMAKE_CXX_STANDARD 17)

add_executable (${APPLICATION_TARGET_NAME})

set (FIND_LIBRARY_USE_LIB64_PATHS TRUE)

if (USE_ASIO_BASED_SCHEDULER)
    set (Boost_USE_MULTITHREADED ON)
    set (Boost_NO_BOOST_CMAKE ON)
    set (Boost_USE_STATIC_LIBS OFF)
    find_package (Boost 1.67 REQUIRED)

    # See comment below fow why we need to add Threads as an explicit dep...
    find_package (Threads)

    target_sources (${APPLICATION_TARGET_NAME}
        PRIVATE
        src/periodic_scheduler.cpp
        )

    target_compile_definitions (${APPLICATION_TARGET_NAME}
        PRIVATE
        USE_ASIO_BASED_SCHEDULER)

    target_link_libraries (${APPLICATION_TARGET_NAME}
        PRIVATE
        Boost::headers
        Threads::Threads) #<<--- Omitting this makes linking fail on Linux: See https://discourse.cmake.org/t/boost-process-target-doesnt-exist-for-thread-linking/2113
else()
    #find_package (TaskScheduler 1.0 REQUIRED)

    set (BUILD_TASKSCHEDULER_TESTAPP OFF CACHE STRING "")
    FetchContent_Declare(
        TaskSchedulerPrj
        GIT_REPOSITORY https://github.com/abigagli/TaskScheduler.git
        )

    FetchContent_MakeAvailable(TaskSchedulerPrj)
    target_link_libraries (${APPLICATION_TARGET_NAME} PRIVATE TaskScheduler::TaskScheduler)
endif()

add_subdirectory(3rdParty)


target_sources (${APPLICATION_TARGET_NAME}
    PRIVATE
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
    PRIVATE MODBUS::static nlohmann_json::nlohmann_json)

target_compile_options (${APPLICATION_TARGET_NAME} PRIVATE $<$<CONFIG:Debug>:-fno-omit-frame-pointer>)

install (TARGETS ${APPLICATION_TARGET_NAME} RUNTIME DESTINATION bin)
