idf_component_register(
    SRCS 
        "main.c"
        "app/turnout_storage.c"
        "app/turnout_manager.c"
        "app/panel_storage.c"
        "app/panel_layout.c"
        "app/lcc_node.cpp"
        "app/screen_timeout.c"
        "app/bootloader_hal.cpp"
        "app/bootloader_display.c"
        "app/sd_log.c"
        "ui/ui_common.c"
        "ui/ui_main.c"
        "ui/ui_turnouts.c"
        "ui/ui_add_turnout.c"
        "ui/ui_panel.c"
        "ui/ui_panel_builder.c"
        "ui/ui_splash.c"
        "ui/panel_geometry.c"
        "ui/ui_wifi_icon.c"
        "wifi/wifi_cred.cpp"
        "wifi/wifi_sta.cpp"
        "wifi/wifi_ids.c"
        "wifi/svc_reach.cpp"
    INCLUDE_DIRS 
        "."
        "app"
        "ui"
        "wifi"
    REQUIRES
        driver
        esp_lcd
        esp_timer
        fatfs
        nvs_flash
        board_drivers
        espressif__esp_jpeg
        lvgl__lvgl
        json
        OpenMRN
        app_update
        esp_wifi
        esp_event
        esp_netif
        mbedtls
        spi_flash
        lwip
)

# Set C++ standard for OpenMRN compatibility
set_source_files_properties(
    "app/lcc_node.cpp"
    "app/bootloader_hal.cpp"
    "wifi/wifi_cred.cpp"
    "wifi/wifi_sta.cpp"
    "wifi/svc_reach.cpp"
    PROPERTIES COMPILE_FLAGS "-std=gnu++14 -fno-strict-aliasing"
)

# Bake git tag into OpenLCB SNIP softwareVersion (v1.0.0 or v1.0.0+).
set(_gv_py "${CMAKE_SOURCE_DIR}/utils/git_version.py")
set(_gv_ver "v0.01+")
if(EXISTS "${_gv_py}")
    if(DEFINED PYTHON)
        set(_gv_python "${PYTHON}")
    else()
        find_program(_gv_python NAMES python3 python)
    endif()
    execute_process(
        COMMAND "${_gv_python}" "${_gv_py}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE _gv_ver
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _gv_rc
        ERROR_QUIET
    )
    if(NOT _gv_rc EQUAL 0 OR _gv_ver STREQUAL "")
        set(_gv_ver "v0.01+")
    endif()
endif()
message(STATUS "SNIP softwareVersion ${_gv_ver}")
target_compile_definitions(${COMPONENT_LIB} PRIVATE RR_GIT_VERSION=${_gv_ver})

# Host-encrypted wrap only (utils/provision_wifi_build.sh). Never a PSK here.
# Re-run CMake when the wrap file appears or disappears so the next flash
# is not a stale wrap-free .bin.
set(_wifi_wrap_inc "${CMAKE_CURRENT_LIST_DIR}/wifi/wifi_psk_wrap.inc")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_wifi_wrap_inc}")
if(EXISTS "${_wifi_wrap_inc}")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE WIFI_WRAP_BLOB_PRESENT=1)
    set_source_files_properties(wifi/wifi_cred.cpp PROPERTIES OBJECT_DEPENDS "${_wifi_wrap_inc}")
    message(STATUS "wifi_cred: baking host-encrypted wrap blob (ciphertext only)")
else()
    message(STATUS "wifi_cred: no wrap blob; DEBUG collect / NVS-only build")
endif()
