/**
 * @file vscp_led.h
 * @brief VSCP LED Indicator Control Functions
 */

#ifndef VSCP_LED_H
#define VSCP_LED_H

#include "esp_err.h"

/**
 * @brief Initialize the VSCP LED indicator.
 *
 * This function initializes the LED indicator with the provided GPIO number.
 *
 * @param led_gpio_num The GPIO number of the LED pin
 * @return esp_err_t ESP_OK if successful, otherwise ESP_FAIL
 */
esp_err_t vscp_led_init(uint32_t led_gpio_num);

/**
 * @brief Deinitialize the VSCP LED indicator.
 *
 * This function deinitializes the LED indicator.
 *
 * @return esp_err_t ESP_OK if successful, otherwise ESP_FAIL
 */
esp_err_t vscp_led_deinit(void);

/**
 * @brief Start the startup blink pattern.
 *
 * This function starts the LED indicator with the startup blink pattern.
 *
 * @return esp_err_t ESP_OK if successful, otherwise an error code
 */
esp_err_t vscp_led_startup_blink(void);

/**
 * @brief Start the factory reset blink pattern.
 *
 * This function starts the LED indicator with the factory reset blink pattern.
 *
 * @return esp_err_t ESP_OK if successful, otherwise an error code
 */
esp_err_t vscp_led_factory_reset_blink_start(void);

/**
 * @brief Stop the factory reset blink pattern.
 *
 * This function stops the LED indicator from the factory reset blink pattern.
 *
 * @return esp_err_t ESP_OK if successful, otherwise an error code
 */
esp_err_t vscp_led_factory_reset_blink_stop(void);

/**
 * @brief Start the OTA update blink pattern.
 *
 * This function starts the LED indicator with the OTA update blink pattern.
 *
 * @return esp_err_t ESP_OK if successful, otherwise an error code
 */
esp_err_t vscp_led_ota_update_blink_start(void);

/**
 * @brief Stop the OTA update blink pattern.
 *
 * This function stops the LED indicator from the OTA update blink pattern.
 *
 * @return esp_err_t ESP_OK if successful, otherwise an error code
 */
esp_err_t vscp_led_ota_update_blink_stop(void);

/**
 * @brief Start the Wi-Fi provisioning blink pattern.
 *
 * This function starts the LED indicator with the Wi-Fi provisioning blink pattern.
 *
 * @return esp_err_t ESP_OK if successful, otherwise an error code
 */
esp_err_t vscp_led_wifi_provisioning_blink_start(void);

/**
 * @brief Stop the Wi-Fi provisioning blink pattern.
 *
 * This function stops the LED indicator from the Wi-Fi provisioning blink pattern.
 *
 * @return esp_err_t ESP_OK if successful, otherwise an error code
 */
esp_err_t vscp_led_wifi_provisioning_blink_stop(void);

/**
 * @brief Start the Wi-Fi connected blink pattern.
 *
 * This function starts the LED indicator with the Wi-Fi connected blink pattern.
 *
 * @return esp_err_t ESP_OK if successful, otherwise an error code
 */
esp_err_t vscp_led_wifi_connected_blink_start(void);

/**
 * @brief Stop the Wi-Fi connected blink pattern.
 *
 * This function stops the LED indicator from the Wi-Fi connected blink pattern.
 *
 * @return esp_err_t ESP_OK if successful, otherwise an error code
 */
esp_err_t vscp_led_wifi_connected_blink_stop(void);

/**
 * @brief Start the Wi-Fi reconnecting blink pattern.
 *
 * This function starts the LED indicator with the Wi-Fi reconnecting blink pattern.
 *
 * @return esp_err_t ESP_OK if successful, otherwise an error code
 */
esp_err_t vscp_led_wifi_reconnecting_blink_start(void);

/**
 * @brief Stop the Wi-Fi reconnecting blink pattern.
 *
 * This function stops the LED indicator from the Wi-Fi reconnecting blink pattern.
 *
 * @return esp_err_t ESP_OK if successful, otherwise an error code
 */
esp_err_t vscp_led_wifi_reconnecting_blink_stop(void);

/**
 * @brief Start the connected to server blink pattern.
 *
 * This function starts the LED indicator with the connected to server blink pattern.
 *
 * @return esp_err_t ESP_OK if successful, otherwise an error code
 */
esp_err_t vscp_led_connected_to_server_blink_start(void);

/**
 * @brief Stop the connected to server blink pattern.
 *
 * This function stops the LED indicator from the connected to server blink pattern.
 *
 * @return esp_err_t ESP_OK if successful, otherwise an error code
 */
esp_err_t vscp_led_connected_to_server_blink_stop(void);

/**
 * @brief Start the ESP-NOW secure paired blink pattern.
 *
 * This function starts the LED indicator with the ESP-NOW secure paired blink pattern.
 *
 * @return esp_err_t ESP_OK if successful, otherwise an error code
 */
esp_err_t vscp_led_espnow_secure_paired_blink_start(void);

/**
 * @brief Stop the ESP-NOW secure paired blink pattern.
 *
 * This function stops the LED indicator from the ESP-NOW secure paired blink pattern.
 *
 * @return esp_err_t ESP_OK if successful, otherwise an error code
 */
esp_err_t vscp_led_espnow_secure_paired_blink_stop(void);

#endif /* VSCP_LED_H */
