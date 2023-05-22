
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"

// #include "esp_log.h"
// #include "esp_system.h"
// #include "esp_wifi.h"

// #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
// #include "esp_mac.h"
// #include "esp_random.h"
// #endif

// #include "espnow.h"
// #include "espnow_security.h"
// #include "espnow_security_handshake.h"
// #include "espnow_storage.h"
// #include "espnow_utils.h"

// esp_err_t espnow_app_init(handler_for_data_t handle)
// {
//     espnow_storage_init();
//     app_wifi_init();

//     espnow_config_t espnow_config = ESPNOW_INIT_CONFIG_DEFAULT();
//     espnow_config.sec_enable = 1;
//     espnow_config.send_retry_num = 5;
//     espnow_config.receive_enable.sec_status = 1;
//     ret = espnow_init(&espnow_config);
//     if (ret != ESP_OK)
//     {
//         return ret;
//     }
//     ret = espnow_set_config_for_data_type(ESPNOW_DATA_TYPE_DATA, true, handle);
// }
// esp_err_t espnow_app_start_sec_initiator(uint8_t *addr, size_t addr_num, uint8_t *app_key)
// {
//     esp_err_t ret = ESP_OK;

//     uint32_t start_time1 = xTaskGetTickCount();
//     espnow_sec_result_t espnow_sec_result = { 0 };
//     espnow_sec_responder_t *info_list = NULL;
//     size_t num = 0;
//     ret = espnow_sec_initiator_scan(&info_list, &num, pdMS_TO_TICKS(3000));
//     ESP_ERROR_RETURN(ret != ESP_OK, ret, "<%s> espnow_sec_initiator_scan", esp_err_to_name(ret));

//     ESP_LOGW(TAG, "espnow wait security num: %u", num);

//     if (num == 0)
//     {
//         ESP_FREE(info_list);
//         return ESP_ERR_TIMEOUT;
//     }

//     espnow_addr_t *dest_addr_list = ESP_MALLOC(num * ESPNOW_ADDR_LEN);

//     for (size_t i = 0; i < num; i++)
//     {
//         memcpy(dest_addr_list[i], info_list[i].mac, ESPNOW_ADDR_LEN);
//     }

//     espnow_sec_initiator_scan_result_free();

//     uint32_t start_time2 = xTaskGetTickCount();
//     ret = espnow_sec_initiator_start(app_key, addr, dest_addr_list, num, &espnow_sec_result);
//     ESP_ERROR_GOTO(ret != ESP_OK, EXIT, "<%s> espnow_sec_initiator_start", esp_err_to_name(ret));

//     ESP_LOGI(TAG, "App key is sent to the device to complete, Spend time: %" PRId32 "ms, Scan time: %" PRId32 "ms",
//         (xTaskGetTickCount() - start_time1) * portTICK_PERIOD_MS, (start_time2 - start_time1) * portTICK_PERIOD_MS);
//     ESP_LOGI(TAG, "Devices security completed, successed_num: %u, unfinished_num: %u", espnow_sec_result.successed_num,
//         espnow_sec_result.unfinished_num);

// EXIT:
//     ESP_FREE(dest_addr_list);
//     espnow_sec_initiator_result_free(&espnow_sec_result);
//     return ret;
// }
// esp_err_t espnow_app_start_sec_initiator(void)
// {
// }