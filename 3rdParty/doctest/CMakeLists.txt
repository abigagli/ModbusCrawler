add_library(doctest INTERFACE)
add_library(DOCTEST::headers ALIAS doctest)

target_include_directories(doctest
INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
)