
set(srcs "")
set(requires "")
set(include_dirs "")

list(APPEND srcs         "src/espnow_ota_initiator.c"
                         "src/espnow_ota_responder.c")
list(APPEND include_dirs "include/")

list(APPEND requires "app_update" 
                    "esp_http_client" 
                    "esp_https_ota" 
                    "efuse" 
                    "spi_flash" 
                     "nvs_flash" 
                     "esp_timer" 
                     "esp_wifi" 
                     "driver"
                     "security"
                     "utils")
idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES ${requires}
)
