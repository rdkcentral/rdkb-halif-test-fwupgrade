cmake_minimum_required(VERSION 3.10)
project(fwupgrade_vl LANGUAGES C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(COMMON_LIB "" CACHE STRING "Additional server libraries (CMake list)")
include(GNUInstallDirs)
add_library(vl_transport STATIC dispatcher/src/vl_transport.c)
target_include_directories(vl_transport PUBLIC cli)
target_compile_options(vl_transport PRIVATE -Wall -Wextra -g)
file(RELATIVE_PATH VL_LIBDIR_FROM_BINDIR
    "${CMAKE_INSTALL_FULL_BINDIR}" "${CMAKE_INSTALL_FULL_LIBDIR}")
add_executable(vl_cli cli/vl_cli.c)
target_link_libraries(vl_cli PRIVATE vl_transport)
target_compile_options(vl_cli PRIVATE -Wall -Wextra -g)
install(TARGETS vl_cli RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
set(FWUPGRADE_HAL_SYMBOLS
    fwupgrade_hal_set_download_url fwupgrade_hal_get_download_url
    fwupgrade_hal_set_download_interface fwupgrade_hal_get_download_interface
    fwupgrade_hal_download fwupgrade_hal_get_download_status
    fwupgrade_hal_reboot_ready fwupgrade_hal_download_reboot_now
    fwupgrade_hal_update_and_factoryreset fwupgrade_hal_download_install)

foreach(component ethsw fwupgrade)
    string(TOUPPER ${component} component_upper)
    set(component_dir "dispatcher/component/${component}")
    # Optional: link the server against a supplied lib<component>_hal.so instead
    # of building the stand-in from src/<component>_hal.c, e.g.
    #   cmake ... -DFWUPGRADE_HAL_LIB=/path/to/libfwupgrade_hal.so
    set(${component_upper}_HAL_LIB "" CACHE FILEPATH
        "Prebuilt lib${component}_hal.so to test (empty: build the stand-in HAL)")
    if(${component_upper}_HAL_LIB)
        get_filename_component(hal_lib "${${component_upper}_HAL_LIB}" ABSOLUTE)
        if(NOT EXISTS "${hal_lib}")
            message(FATAL_ERROR "${component_upper}_HAL_LIB not found: ${hal_lib}")
        endif()
        find_program(NM_TOOL nm)
        if(NM_TOOL)
            execute_process(COMMAND "${NM_TOOL}" -D --defined-only "${hal_lib}"
                OUTPUT_VARIABLE hal_exports RESULT_VARIABLE hal_nm_rc)
            if(NOT hal_nm_rc EQUAL 0)
                message(FATAL_ERROR "Cannot read symbols of ${hal_lib}")
            endif()
            foreach(symbol IN LISTS ${component_upper}_HAL_SYMBOLS)
                if(NOT hal_exports MATCHES "[ \t][TWi] ${symbol}\n")
                    message(FATAL_ERROR "${hal_lib} does not export ${symbol}")
                endif()
            endforeach()
        endif()
        add_library(${component}_hal SHARED IMPORTED)
        set_target_properties(${component}_hal PROPERTIES
            IMPORTED_LOCATION "${hal_lib}"
            INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/${component_dir}/include")
        message(STATUS "${component}: using prebuilt HAL ${hal_lib}")
    else()
        add_library(${component}_hal SHARED ${component_dir}/src/${component}_hal.c)
        target_include_directories(${component}_hal PUBLIC ${component_dir}/include)
        target_compile_options(${component}_hal PRIVATE -Wall -Wextra -g)
    endif()
    add_executable(${component}_vl
        dispatcher/src/vl_main.c dispatcher/src/vl_server.c
        ${component_dir}/src/component.c ${component_dir}/src/${component}hal_apis.c)
    target_compile_definitions(${component}_vl PRIVATE VL_SERVER)
    target_compile_options(${component}_vl PRIVATE -Wall -Wextra -g)
    target_link_libraries(${component}_vl PRIVATE vl_transport ${component}_hal ${COMMON_LIB})
    set_target_properties(${component}_vl PROPERTIES
        INSTALL_RPATH "$ORIGIN/${VL_LIBDIR_FROM_BINDIR}")
    install(TARGETS ${component}_vl RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    if(${component_upper}_HAL_LIB)
        install(FILES "${hal_lib}" DESTINATION "${CMAKE_INSTALL_LIBDIR}")
    else()
        install(TARGETS ${component}_hal LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
    endif()
endforeach()
