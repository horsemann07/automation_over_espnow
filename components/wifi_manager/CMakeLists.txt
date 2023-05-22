
set(srcs "")
set(requires "")
set(include_dirs "")

list(APPEND srcs            "src/core_wifim.c" "src/wifim.c")
list(APPEND include_dirs    "include" )

list(APPEND requires "spi_flash" 
                     "nvs_flash" 
                     "esp_timer" 
                     "esp_wifi" 
                     "driver"
                     "lwip")
idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES ${requires}
)
