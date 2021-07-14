set (APPLICATION_TARGET_NAME pwrc)

# See comment below fow why we need to add Threads as an explicit dep...
find_package (Threads)

add_executable (${APPLICATION_TARGET_NAME})
set_target_properties (${APPLICATION_TARGET_NAME} PROPERTIES DEBUG_POSTFIX "D")

target_sources (${APPLICATION_TARGET_NAME}
    PRIVATE
    main.cpp
    )

target_compile_options (${APPLICATION_TARGET_NAME}
PRIVATE
    -UUSE_LOGURU
)

target_link_libraries (${APPLICATION_TARGET_NAME}
    PRIVATE
    ${CMAKE_DL_LIBS}
    OBJECTS::common
    Threads::Threads) #<<--- Omitting this makes linking fail on Linux: See https://discourse.cmake.org/t/boost-process-target-doesnt-exist-for-thread-linking/2113

install (TARGETS ${APPLICATION_TARGET_NAME} RUNTIME DESTINATION bin)
