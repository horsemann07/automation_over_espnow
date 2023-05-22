
set(srcs "")
set(requires "")
set(include_dirs "")

list(APPEND srcs         "src/espnow_console.c"
                         "src/espnow_log_flash.c"
                         "src/espnow_log.c")
list(APPEND srcs         "src/sdcard/sdcard.c")
list(APPEND srcs         "src/commands/pcap/pcap.c"
                         "src/commands/cmd_espnow.c"
                         "src/commands/cmd_iperf.c"
                         "src/commands/cmd_peripherals.c"
                         "src/commands/cmd_sdcard.c"
                         "src/commands/cmd_system.c"
                         "src/commands/cmd_wifi_sniffer.c"
                         "src/commands/cmd_wifi.c"
                         "src/commands/espnow_commands.c")
list(APPEND include_dirs "include"
                         "include/sdcard"
                         "src"
                         "src/commands/pcap")
list(APPEND requires "console" 
                    "fatfs" 
                    "spiffs" 
                    "spi_flash" 
                    "nvs_flash" 
                    "esp_timer" 
                    "esp_wifi" 
                    "driver" 
                    "espnow" 
                    "security" 
                    "utils"
                    "control"
                    "provisioning")

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS ${include_dirs}
    REQUIRES ${requires}
)