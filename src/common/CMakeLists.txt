
add_library (common
OBJECT
    modbus_ops.cpp
)

add_dependencies(common copy_current_compile_commands)
target_include_directories(common
PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}
    )
target_compile_options (common
PUBLIC
    #$<$<CONFIG:Debug>:-fno-omit-frame-pointer -fsanitize=address -fsanitize=signed-integer-overflow -fsanitize=unsigned-integer-overflow>
    $<$<CONFIG:Debug>:-fno-omit-frame-pointer -fsanitize=address -fsanitize=signed-integer-overflow>
)
target_compile_definitions (common
PUBLIC
#    $<$<CONFIG:Release,RelWithDebInfo>:DOCTEST_CONFIG_DISABLE>
$<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>:DOCTEST_CONFIG_DISABLE>
)

target_link_options (common
PUBLIC
    #$<$<CONFIG:Debug>:-fsanitize=address -fsanitize=signed-integer-overflow -fsanitize=unsigned-integer-overflow>
    $<$<CONFIG:Debug>:-fsanitize=address -fsanitize=signed-integer-overflow>
)
target_link_libraries(common
PUBLIC
MODBUS::static
DOCTEST::headers
)

add_library(OBJECTS::common ALIAS common)