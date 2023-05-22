
set(srcs "")
set(requires "")
set(include_dirs "")

# list(APPEND srcs            "src/core_wifim.c" "src/wifim.c")
list(APPEND include_dirs    "include" )

# set(CMAKE_C_CLANG_TIDY_EXECUTABLE /usr/bin/clang-tidy)

# set(CMAKE_C_CLANG_TIDY "clang-tidy;-header-filter=${CMAKE_SOURCE_DIR}/components/wifi_manager*")


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
