
set(srcs "")
set(requires "")
set(include_dirs "")

list(APPEND srcs            "srcs/vscp_helper.c" "srcs/vscp_event_handler.c")
list(APPEND include_dirs    "include" )

list(APPEND requires "spi_flash" 
                     "nvs_flash" 
                     "esp_timer" 
                     "esp_wifi" 
                     "driver"
                     "lwip"
                     "utils")
idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES ${requires}
)
