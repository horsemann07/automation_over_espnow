/* Get Start Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
#include "esp_mac.h"
#endif

#include "core_mqtt.h"
#include "core_mqtt_agent.h"
#include "driver/uart.h"
#include "espnow.h"
#include "espnow_storage.h"
#include "espnow_utils.h"
#include "nvs_flash.h"
#include "wifim.h"

extern int aws_iot_demo_main(int argc, char **argv);
static const char *TAG = "app_main";

// temporary ssid and password
#define WIFI_SSID        ("GhostHouse")
#define WIFI_SSID_LEN    ((uint16_t)(strlen(WIFI_SSID)))
#define WIFI_PASSKEY     ("TheDemonSlayer")
#define WIFI_PASSKEY_LEN ((uint16_t)(strlen(WIFI_PASSKEY)))

void app_main(void)
{
    esp_err_t ret = ESP_FAIL;

    /* Initialize NVS partition */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        /* NVS partition was truncated
         * and needs to be erased */
        ESP_ERROR_CHECK(nvs_flash_erase());

        /* Retry nvs_flash_init */
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ret = wifim_wifi_on();
    if (ret != ESP_OK)
    {
        return;
    }

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config_t));

    // Copy the SSID and password to the wifi_config structure
    strncpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0'; // Ensure null-terminated string

    strncpy((char *)wifi_config.sta.password, WIFI_PASSKEY, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0'; // Ensure null-terminated string
    wifi_config.sta.channel = 0;
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    ret = wifim_connect_ap(&wifi_config);
    if (ret == ESP_OK)
    {
        // Connection successful
        ESP_LOGI(TAG, "Connection successful");
    }
    else
    {
        // Connection failed, handle the error
        ESP_LOGI(TAG, "Connection failed %s", esp_err_to_name(ret));
    }
    // aws_iot_demo_main(0, NULL);

    return;
}
