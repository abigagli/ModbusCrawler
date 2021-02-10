include(ExternalProject)

if (CMAKE_CROSSCOMPILING)
    set (CROSS_COMPILING_SUPPORT --host=${CROSS_COMPILE_HOST} "CC=${CMAKE_C_COMPILER}" "CFLAGS=--sysroot=${CMAKE_SYSROOT}" "LDFLAGS=--sysroot=${CMAKE_SYSROOT}")
endif()

ExternalProject_Add(
    MODBUS_EXTPRJ
    GIT_REPOSITORY git@github.com:stephane/libmodbus.git
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
    INSTALL_DIR INSTALL
    UPDATE_COMMAND ""
    #CONFIGURE_COMMAND $<$<CONFIG:Debug>:CFLAGS="-g3 -O0"> <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --enable-static --disable-tests --without-documentation
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env "$<$<CONFIG:Debug>:CFLAGS=-g3 -O0>" <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --enable-static --disable-tests --without-documentation ${CROSS_COMPILING_SUPPORT}
    LOG_DOWNLOAD ON
    LOG_CONFIGURE ON
    LOG_BUILD ON
    LOG_INSTALL ON
    EXCLUDE_FROM_ALL ON
    )

ExternalProject_Add_Step(
    MODBUS_EXTPRJ
    GenerateConfigure
    COMMAND <SOURCE_DIR>/autogen.sh
    COMMENT "Running autogen"
    WORKING_DIRECTORY <SOURCE_DIR>
    DEPENDEES download
    DEPENDERS configure
    BYPRODUCTS configure
    LOG ON
    )

ExternalProject_Get_Property(MODBUS_EXTPRJ INSTALL_DIR)

add_library(modbus_imported STATIC IMPORTED)
set_target_properties(modbus_imported PROPERTIES IMPORTED_LOCATION ${INSTALL_DIR}/lib/libmodbus.a)

add_library(modbus_static INTERFACE)
target_include_directories(modbus_static INTERFACE ${INSTALL_DIR}/include/modbus)
target_link_libraries(modbus_static INTERFACE modbus_imported)
add_dependencies(modbus_static MODBUS_EXTPRJ)
add_library(MODBUS::static ALIAS modbus_static)
