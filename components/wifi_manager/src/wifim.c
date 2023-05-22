

// #include "wifim.h"
// #include "esp_err.h"
// #include "esp_log.h"
// #include "string.h"

// #define TAG ("WIFIM")

// // temporary ssid and password
// #define WIFI_SSID        ("GhostHouse")
// #define WIFI_SSID_LEN    ((uint16_t)(strlen(WIFI_SSID)))
// #define WIFI_PASSKEY     ("TheDemonSlayer")
// #define WIFI_PASSKEY_LEN ((uint16_t)(strlen(WIFI_PASSKEY)))

// extern TaskHandle_t wifim_task_handle;
// extern void wifim_task(void *parameter);

// static void wifi_connect_cb(wifi_event_t event_type, void *event_data)
// {
//     (void)event_data;

//     global_wifi_status = (uint8_t)event_type;

//     // TODO: add leds codes
//     ESP_LOGI(TAG, "wifi connected green signal");
// }

// static void wifi_disconnect_cb(wifi_event_t event_type, void *event_data)
// {
//     (void)event_data;

//     global_wifi_status = (uint8_t)event_type;

//     // TODO: add leds codes
//     ESP_LOGI(TAG, "wifi connected red blink fast signal");
// }

// static void wifi_on_cb(wifi_event_t event_type, void *event_data)
// {
//     (void)event_data;

//     global_wifi_status = (uint8_t)event_type;

//     // TODO: add leds codes
//     ESP_LOGI(TAG, "wifi connected red signal ");
// }

// static void wifi_off_cb(wifi_event_t event_type, void *event_data)
// {
//     (void)event_data;

//     global_wifi_status = (uint8_t)event_type;

//     // TODO: add leds codes
//     ESP_LOGI(TAG, "wifi connected white signal");
// }

// // static void wifim_register_wifi_event_callbacks(void)
// // {
// //     wifim_register_event_cb(STARTED_BIT, &wifi_on_cb);
// //     wifim_register_event_cb(STOPPED_BIT, &wifi_off_cb);
// //     wifim_register_event_cb(CONNECTED_BIT, &wifi_connect_cb);
// //     wifim_register_event_cb(DISCONNECTED_BIT, &wifi_disconnect_cb);
// // }

// // static void wifim_unregister_wifi_event_callbacks(void)
// // {
// //     wifim_register_event_cb(STARTED_BIT, NULL);
// //     wifim_register_event_cb(STOPPED_BIT, NULL);
// //     wifim_register_event_cb(CONNECTED_BIT, NULL);
// //     wifim_register_event_cb(DISCONNECTED_BIT, NULL);
// // }

// // esp_err_t wifim_init(void)
// // {
// //     esp_err_t ret = ESP_OK;

// //     uint16_t num = 0;
// //     // wifi_config_t wifi_config = { 0 };
// //     global_wifi_status = WIFI_EVENT_MAX;

// //     /* register wifi event callbacks */
// //     wifim_register_wifi_event_callbacks();

// //     ret = wifim_wifi_on();
// //     if (ret != ESP_OK)
// //     {
// //         return ret;
// //     }
// //     /* get number of saved wifi networks */
// //     // wifim_get_saved_network(&num);
// //     // if (num == 0)
// //     // {
// //     // TODO: start ble-wifi provisioning
// //     /* connect to wifi temporary codes */
// //     // Copy the SSID and password to the wifi_config structure
// //     // wifi_config.sta.channel = 0;
// //     // wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
// //     // memcpy(wifi_config.sta.ssid, WIFI_SSID, WIFI_SSID_LEN + 1);
// //     // memcpy(wifi_config.sta.password, WIFI_PASSKEY, WIFI_PASSKEY_LEN + 1);
// //     //     /* connect to wifi */
// //     wifi_config_t wifi_config = { .sta = {
// //                                       .channel = 0, // all channel
// //                                       .threshold.authmode = WIFI_AUTH_WPA2_PSK,
// //                                       .ssid = WIFI_SSID,
// //                                       .password = WIFI_PASSKEY,
// //                                   } };
// //     ret = wifim_connect_ap(&wifi_config);
// //     // }
// //     // else
// //     // {
// //     //     for (int idx = 0; idx < num; idx++)
// //     //     {
// //     //         ret = wifim_get_network_profile(&wifi_config, idx);
// //     //         if (ret != ESP_OK)
// //     //         {
// //     //             ESP_LOGD(TAG, "failed to get the wifi profile at index %d", idx);
// //     //         }
// //     //         else
// //     //         {
// //     //             ret = wifim_connect_ap((const wifi_config_t *)&wifi_config);
// //     //             if (ret == ESP_OK)
// //     //             {
// //     //                 break;
// //     //             }
// //     //         }
// //     //     }
// //     // }

// //     if (ret != ESP_OK)
// //     {
// //         return ret;
// //     }

// //     /* if task created */
// //     // if (wifim_task_handle == NULL)
// //     // {
// //     //     xTaskCreate(&wifim_task, "wifim", CONFIG_WIFI_MANAGER_TASK_SIZE, NULL, CONFIG_WIFI_MANAGER_TASK_PRIORITY,
// //     //         wifim_task_handle);
// //     // }

// //     return ESP_OK;
// // }

// esp_err_t wifim_init(void)
// {
//     esp_err_t ret = ESP_OK;

//     uint16_t num = 0;
//     global_wifi_status = WIFI_EVENT_MAX;

//     /* register wifi event callbacks */
//     //wifim_register_wifi_event_callbacks();

//     ret = wifim_wifi_on();
//     if (ret != ESP_OK)
//     {
//         return ret;
//     }

//     wifi_config_t wifi_config;
//     memset(&wifi_config, 0, sizeof(wifi_config_t));

//     // Copy the SSID and password to the wifi_config structure
//     strncpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
//     wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0'; // Ensure null-terminated string

//     strncpy((char *)wifi_config.sta.password, WIFI_PASSKEY, sizeof(wifi_config.sta.password) - 1);
//     wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0'; // Ensure null-terminated string
//     wifi_config.sta.channel = 0;
//     wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK;
//     ret = wifim_connect_ap(&wifi_config);
//     if (ret == ESP_OK)
//     {
//         // Connection successful
//     }
//     else
//     {
//         // Connection failed, handle the error
//         return ret;
//     }

//     return ESP_OK;
// }

// esp_err_t wifim_deinit(void)
// {
//     global_wifi_status = WIFI_EVENT_MAX;

//     /* unregister wifi event callbacks */
//     //wifim_unregister_wifi_event_callbacks();

//     if (wifim_task_handle)
//     {
//         vTaskDelete(wifim_task_handle);
//         wifim_task_handle = NULL;
//     }
//     return ESP_OK;
// }
