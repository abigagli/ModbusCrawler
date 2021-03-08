include(FetchContent)

FetchContent_Declare(
    loguru
    GIT_REPOSITORY https://github.com/emilk/loguru.git
    GIT_TAG master
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    SOURCE_DIR loguru-src
    )

FetchContent_MakeAvailable(loguru)
