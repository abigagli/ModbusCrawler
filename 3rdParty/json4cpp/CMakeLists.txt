include(FetchContent)

FetchContent_Declare(json4cpp
    GIT_REPOSITORY https://github.com/ArthurSonzogni/nlohmann_json_cmake_fetchcontent
    GIT_TAG v3.9.1
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    )

set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(json4cpp)
