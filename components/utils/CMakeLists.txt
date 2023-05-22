
set(srcs "")
set(requires "")
set(include_dirs "")

list(APPEND srcs         "src/espnow_mem.c"
                         "src/espnow_reboot.c"
                         "src/espnow_storage.c"
                         "src/espnow_timesync.c"
                         "src/espnow_utils.c")
list(APPEND include_dirs "include")
list(APPEND requires "spi_flash" "nvs_flash" "esp_timer" "esp_wifi" "driver" "ota")

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES ${requires}
)