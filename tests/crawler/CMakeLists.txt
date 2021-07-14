add_executable(test_crawler main.cpp)

target_link_libraries (test_crawler
PRIVATE
    ${CMAKE_DL_LIBS}
    OBJECTS::crawler
    OBJECTS::common
    Threads::Threads
)

add_test(NAME crawler
        COMMAND test_crawler
)
