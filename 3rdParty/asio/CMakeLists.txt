include(FetchContent)

if (USE_STANDALONE_ASIO)
    message(STATUS "******* STANDALONE ASIO *******")
    FetchContent_Declare(
        asio
        URL https://sourceforge.net/projects/asio/files/asio/1.18.1%20%28Stable%29/asio-1.18.1.tar.gz
        URL_MD5 656a1decf9f6995b74a95966263cb4f7
        SOURCE_DIR asio-src
        )

    FetchContent_MakeAvailable(asio)
    add_library(asio INTERFACE)
    target_include_directories(asio INTERFACE ${asio_SOURCE_DIR}/include)
    set_target_properties(asio
        PROPERTIES
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES ${asio_SOURCE_DIR}/include)
    add_library(ASIO::headers ALIAS asio)
else(USE_STANDALONE_ASIO)

    message(STATUS "******* BOOST ASIO *******")
    set (Boost_USE_MULTITHREADED ON)
    set (Boost_NO_BOOST_CMAKE ON)
    set (Boost_USE_STATIC_LIBS OFF)
    find_package (Boost 1.67 REQUIRED)
    set_target_properties(Boost::headers
        PROPERTIES
            IMPORTED_GLOBAL ON
            INTERFACE_SYSTEM_INCLUDE_DIRECTORIES $<TARGET_PROPERTY:Boost::headers, INTERFACE_INCLUDE_DIRECTORIES>)

endif(USE_STANDALONE_ASIO)
