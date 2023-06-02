
set(srcs "")
set(requires "")
set(include_dirs "")

list(APPEND srcs            "src/wifim.c")
list(APPEND include_dirs    "include" )

list(APPEND requires "spi_flash" 
                     "nvs_flash" 
                     "wifi_provisioning"
                     "utils")
idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES ${requires}
)
