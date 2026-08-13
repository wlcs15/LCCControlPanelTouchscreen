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
)

# Set C++ standard for OpenMRN compatibility
set_source_files_properties(
    "app/lcc_node.cpp"
    "app/bootloader_hal.cpp"
    "wifi/wifi_cred.cpp"
    "wifi/wifi_sta.cpp"
    PROPERTIES COMPILE_FLAGS "-std=gnu++14 -fno-strict-aliasing"
)

# Host-encrypted wrap only (utils/provision_wifi_build.sh). Never a PSK here.
set(_wifi_wrap_inc "${CMAKE_CURRENT_LIST_DIR}/wifi/wifi_psk_wrap.inc")
if(EXISTS "${_wifi_wrap_inc}")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE WIFI_WRAP_BLOB_PRESENT=1)
    set_source_files_properties(wifi/wifi_cred.cpp PROPERTIES OBJECT_DEPENDS "${_wifi_wrap_inc}")
    message(STATUS "wifi_cred: baking host-encrypted wrap blob (ciphertext only)")
else()
    message(STATUS "wifi_cred: no wrap blob; DEBUG collect / NVS-only build")
endif()
