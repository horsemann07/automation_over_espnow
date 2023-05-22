
set(srcs "")
set(requires "")
set(include_dirs "")

list(APPEND srcs         "src/espnow_security.c"
                         "src/espnow_security_initiator.c"
                         "src/espnow_security_responder.c"
                         "src/protocomm/security/client_security1.c")
list(APPEND include_dirs "include" 
                         "include/protocomm/security")
list(APPEND requires "mbedtls" 
                    "protobuf-c" 
                    "protocomm" 
                    "spi_flash" 
                    "nvs_flash" 
                    "esp_timer" 
                    "esp_wifi" 
                    "driver"
                    "espnow")

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES ${requires}
)

idf_component_get_property(COMPONENT_DIR protocomm COMPONENT_DIR)
target_include_directories(${COMPONENT_TARGET} PUBLIC
"${COMPONENT_DIR}/proto-c")