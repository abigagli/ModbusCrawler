add_executable(test_crawler main.cpp)

target_link_libraries (test_crawler
PRIVATE
    OBJECTS::crawler
    OBJECTS::common
)

add_test(NAME crawler
        COMMAND test_crawler
)
