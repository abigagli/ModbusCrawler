set (APPLICATION_TARGET_NAME pwrc)

# See comment below fow why we need to add Threads as an explicit dep...
find_package (Threads)

add_executable (${APPLICATION_TARGET_NAME})
add_dependencies(${APPLICATION_TARGET_NAME} copy_current_compile_commands)
set_target_properties (${APPLICATION_TARGET_NAME} PROPERTIES DEBUG_POSTFIX "D")

target_include_directories (${APPLICATION_TARGET_NAME}
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/..
    )

target_sources (${APPLICATION_TARGET_NAME}
    PRIVATE
    main.cpp
    #${CMAKE_SOURCE_DIR}/compile_commands.json #<-- Having this as a dependency will cause the custom command below to execute...
    )

target_compile_options (${APPLICATION_TARGET_NAME}
PRIVATE
    #$<$<CONFIG:Debug>:-fno-omit-frame-pointer -fsanitize=address -fsanitize=signed-integer-overflow -fsanitize=unsigned-integer-overflow>
    $<$<CONFIG:Debug>:-fno-omit-frame-pointer -fsanitize=address -fsanitize=signed-integer-overflow>
)

target_link_options (${APPLICATION_TARGET_NAME}
PRIVATE
    #$<$<CONFIG:Debug>:-fsanitize=address -fsanitize=signed-integer-overflow -fsanitize=unsigned-integer-overflow>
    $<$<CONFIG:Debug>:-fsanitize=address -fsanitize=signed-integer-overflow>
)

target_link_libraries (${APPLICATION_TARGET_NAME}
    PRIVATE
    ${CMAKE_DL_LIBS}
    MODBUS::static
    Threads::Threads) #<<--- Omitting this makes linking fail on Linux: See https://discourse.cmake.org/t/boost-process-target-doesnt-exist-for-thread-linking/2113

install (TARGETS ${APPLICATION_TARGET_NAME} RUNTIME DESTINATION bin)
