set (APPLICATION_TARGET_NAME mbcrawler)

FetchContent_GetProperties(loguru)

add_library (crawler
OBJECT
    meas_config.cpp
    meas_executor.cpp
    meas_reporter.cpp
    json_support.cpp
    periodic_scheduler.cpp
    ${loguru_SOURCE_DIR}/loguru.cpp
)

target_include_directories (crawler
PUBLIC
    ${loguru_SOURCE_DIR}
    )

target_link_libraries(crawler
PUBLIC
    nlohmann_json::nlohmann_json
    OBJECTS::common
    )

target_compile_options (crawler
PUBLIC
    -DUSE_LOGURU
    -DLOGURU_WITH_STREAMS=1
    -DLOGURU_WITH_FILEABS=1
)

if (USE_STANDALONE_ASIO)
    target_compile_definitions (crawler
        PUBLIC
        ASIO_STANDALONE)
    target_link_libraries (crawler
        PUBLIC
        ASIO::headers)
else (USE_STANDALONE_ASIO)
    target_link_libraries (crawler
        PUBLIC
        Boost::headers)
endif (USE_STANDALONE_ASIO)

add_library(OBJECTS::crawler ALIAS crawler)

add_executable (${APPLICATION_TARGET_NAME})
set_target_properties (${APPLICATION_TARGET_NAME} PROPERTIES DEBUG_POSTFIX "D")

target_sources (${APPLICATION_TARGET_NAME}
    PRIVATE
    main.cpp
    )

target_link_libraries (${APPLICATION_TARGET_NAME}
    PRIVATE
    ${CMAKE_DL_LIBS}
    OBJECTS::crawler
    OBJECTS::common
    Threads::Threads) #<<--- Omitting this makes linking fail on Linux: See https://discourse.cmake.org/t/boost-process-target-doesnt-exist-for-thread-linking/2113

install (TARGETS ${APPLICATION_TARGET_NAME} RUNTIME DESTINATION bin)
