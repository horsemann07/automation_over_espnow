/**
 * @file wifim.h
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-05-01
 *
 * @copyright Copyright (c) 2023
 *
 */

#ifndef WIFI_MANAGER_H_
#define WIFI_MANAGER_H_

#ifdef __cplusplus
extern "C" {
#endif
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"
#include <stdint.h>
/**
 * @brief Maximum number of connection retries.
 */
#define WIFIM_NUM_CONNECTION_RETRY (3)

/**
 * @brief Maximum number of connected station in Access Point mode.
 */
#define WIFIM_MAX_CONNECTED_STATIONS (4)

/**
 * @brief Max SSID length
 */
#define WIFIM_MAX_SSID_LEN (32)

/**
 * @brief Max BSSID length
 */
#define WIFIM_MAX_BSSID_LEN (6)

/**
 * @brief Max passphrase length
 * Maximum allowed WPA2 passphrase length (per specification) is 63
 */
#define WIFIM_MAX_PASSPHRASE_LEN (63)

/**
 * @brief Soft Access point SSID
 */
#define WIFIM_ACCESS_POINT_SSID_PREFIX ("WIFIM")

/**
 * @brief Soft Access point Passkey
 */
#define WIFIM_ACCESS_POINT_PASSKEY ("password")

/**
 * @brief Soft Access point Channel
 */
#define WIFIM_ACCESS_POINT_CHANNEL (11)

/**
 * @brief Maximum number of network profiles stored.
 */
#define WIFIM_MAX_NETWORK_PROFILES (8)

/**
 * @brief WiFi semaphore timeout
 */
#define WIFIM_MAX_SEMAPHORE_WAIT_TIME_MS (60000)

/**
 * @brief Soft Access point security
 * WPA2 Security, see WIFISecurity_t
 * other values are - eWiFiSecurityOpen, eWiFiSecurityWEP, eWiFiSecurityWPA
 */
#define WIFIM_ACCESS_POINT_SECURITY (WIFI_AUTH_WPA_WPA2_PSK)

/**
 * @brief IPV6 length in 32-bit words
 *
 * @note This is the constant for IPV6 length in 32-bit words
 */
#define IPV6_LENGTH (4)

/* Define the timeout for the ping operation */
#define PING_TIMEOUT_MS (1000)

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT    (BIT0)
#define WIFI_DISCONNECTED_BIT (BIT1)
#define WIFI_STARTED_BIT      (BIT2)
#define WIFI_STOPPED_BIT      (BIT3)
#define AUTH_CHANGE_BIT       (BIT4)
#define AP_STARTED_BIT        (BIT4)
#define AP_STOPPED_BIT        (BIT5)
#define WIFI_PROV_START_BIT   (BIT6)
#define WIFI_PROV_STOP_BIT    (BIT6)
/**
 * @brief Wi-Fi event handler definition.
 *
 * @param[in] xEvent - Wi-Fi event data structure.
 *
 * @return None.
 */
typedef void (*wifi_event_handler_t)(wifi_event_t event_type, void *event_data);

/**
 * @brief Global variable to hold the status of WiFi.
 *
 * This variable is used to store the current status of the WiFi connection.
 */
volatile uint8_t global_wifi_status;

/**
 * @brief Initializes the WiFi module.
 *
 * This function initializes the WiFi module and prepares it for operation.
 * It performs any necessary setup, configuration, or resource allocation
 * required for WiFi functionality.
 *
 * @return
 *    - ESP_OK: Initialization was successful.
 *    - ESP_FAIL: Initialization failed.
 *    - Other error codes may be returned depending on the implementation.
 */
esp_err_t wifim_init(void);

/**
 * @brief Deinitializes the WiFi module.
 *
 * This function deinitializes the WiFi module and cleans up any resources
 * or configurations that were allocated during initialization. It should
 * be called when WiFi functionality is no longer needed or during system shutdown.
 *
 * @return
 *    - ESP_OK: Deinitialization was successful.
 *    - ESP_FAIL: Deinitialization failed.
 *    - Other error codes may be returned depending on the implementation.
 */
esp_err_t wifim_deinit(void);

/**
 * @brief Initializes Wi-Fi and starts SmartConfig.
 *
 * @return esp_err_t ESP_OK on success, or an error code otherwise.
 */
esp_err_t wifim_provision(void);

/**
 * @brief Checks if Wi-Fi is connected to a specific network.
 *
 * @param[in] ntwk_param Pointer to the Wi-Fi network configuration parameters.
 *                      If NULL, any network configuration matches.
 *
 * @return esp_err_t ESP_OK if connected, or ESP_FAIL otherwise.
 */
esp_err_t wifim_is_connected(const wifi_config_t *ntwk_param);

/**
 * @brief
 *
 * @return esp_err_t
 */
esp_err_t wifim_reconnect(void);

/**
 * @brief Turns off Wi-Fi and deinitializes it.
 *
 * @return esp_err_t ESP_OK on success, or an error code otherwise.
 */
esp_err_t wifim_wifi_off(void);

/**
 * @brief Initializes Wi-Fi and sets it to station mode.
 *
 * @return esp_err_t ESP_OK on success, or an error code otherwise.
 */
esp_err_t wifim_wifi_on(void);

/**
 * @brief Connect to an access point with the specified configuration.
 *
 * This function establishes a connection to the specified access point using the given configuration.
 *
 * @param[in] ap_config Configuration for the access point to connect to.
 *
 * @return ESP_OK on success; otherwise, an error code indicating the reason for failure.
 */
esp_err_t wifim_connect_ap(const wifi_config_t *const ap_config);

/**
 * @brief Disconnect from the current access point.
 *
 * This function disconnects from the currently connected access point.
 *
 * @return ESP_OK on success; otherwise, an error code indicating the reason for failure.
 */
esp_err_t wifim_disconnect(void);

/**
 * @brief Reset the WiFi manager to its default state.
 *
 * This function resets the WiFi manager to its default state.
 *
 * @return ESP_OK on success; otherwise, an error code indicating the reason for failure.
 */
esp_err_t wifim_reset(void);

/**
 * @brief Reset the WiFi manager and erase all stored network profiles.
 *
 * This function resets the WiFi manager to its default state and erases all stored network profiles.
 *
 * @return ESP_OK on success; otherwise, an error code indicating the reason for failure.
 */
esp_err_t wifim_factory_reset(void);

/**
 * @brief Scan for available access points.
 *
 * This function scans for available access points and stores the results in the provided buffer.
 *
 * @param[out] scanned_ap Pointer to an array of wifi_ap_record_t structures to store the results.
 * @param[in] num_aps Maximum number of access points to scan for.
 *
 * @return ESP_OK on success; otherwise, an error code indicating the reason for failure.
 */
esp_err_t wifim_scan(wifi_ap_record_t *scanned_ap, uint16_t num_aps);

/**
 * @brief Set the WiFi mode to the specified mode.
 *
 * This function sets the WiFi mode to the specified mode.
 *
 * @param[in] mode WiFi mode to set.
 *
 * @return ESP_OK on success; otherwise, an error code indicating the reason for failure.
 */
esp_err_t wifim_set_mode(wifi_mode_t mode);

/**
 * @brief Get the current WiFi mode.
 *
 * This function retrieves the current WiFi mode.
 *
 * @param[out] mode Pointer to a wifi_mode_t variable to store the mode in.
 *
 * @return ESP_OK on success; otherwise, an error code indicating the reason for failure.
 */
esp_err_t wifim_get_mode(wifi_mode_t *mode);

/**
 * @brief Serialize a network profile to a buffer.
 *
 * This function serializes a network profile to a buffer for storage or transmission.
 *
 * @param[in] network Pointer to the network profile to serialize.
 * @param[out] buffer Pointer to the buffer to store the serialized data in.
 * @param[in,out] size Pointer to a variable containing the size of the buffer. On output, this variable will contain
 * the actual size of the serialized data.
 *
 * @return ESP_OK on success; otherwise, an error code indicating the reason for failure.
 */
esp_err_t wifim_serialize_network_profile(const wifi_config_t *const network, uint8_t *buffer, uint32_t *size);

/**

@brief Deserialize a Wi-Fi network profile from a buffer.
This function deserializes a Wi-Fi network profile from a buffer.
@param[in] ntwrk_prof Wi-Fi network profile to store the deserialized data.
@param[in] buffer Pointer to the buffer containing the serialized network profile data.
@param[in] size Size of the buffer.
@return ESP_OK on success, ESP_ERR_INVALID_ARG if the arguments are invalid, or ESP_FAIL if the operation fails.
*/
esp_err_t wifim_deserialize_network_profile(wifi_config_t *const ntwrk_prof, uint8_t *buffer, uint32_t size);
/**

@brief Store a Wi-Fi network profile in the NVS.
This function stores a Wi-Fi network profile in the non-volatile storage (NVS).
@param[in] xNvsHandle Handle to the NVS partition.
@param[in] ntwrk_prof Pointer to the Wi-Fi network profile to store.
@param[in] usIndex Index of the network profile in the NVS.
@return ESP_OK on success, ESP_ERR_INVALID_ARG if the arguments are invalid, or ESP_FAIL if the operation fails.
*/
esp_err_t wifim_store_network_profile(nvs_handle_t xNvsHandle, const wifi_config_t *ntwrk_prof, uint16_t usIndex);
/**

@brief Get a Wi-Fi network profile from the NVS.
This function retrieves a Wi-Fi network profile from the non-volatile storage (NVS).
@param[in] xNvsHandle Handle to the NVS partition.
@param[out] ntwrk_prof Pointer to the Wi-Fi network profile to store the retrieved data.
@param[in] usIndex Index of the network profile in the NVS.
@return ESP_OK on success, ESP_ERR_INVALID_ARG if the arguments are invalid, or ESP_FAIL if the operation fails.
*/
esp_err_t wifim_get_stored_network_profile(nvs_handle_t xNvsHandle, wifi_config_t *ntwrk_prof, uint16_t usIndex);
/**

@brief Initialize the NVS partition.
This function initializes the non-volatile storage (NVS) partition.
@param[in] xNvsHandle Handle to the NVS partition.
@return ESP_OK on success, ESP_ERR_INVALID_ARG if the arguments are invalid, or ESP_FAIL if the operation fails.
*/
esp_err_t wifim_init_resgistry(nvs_handle_t xNvsHandle);
/**

@brief Add a Wi-Fi network profile to the NVS.
This function adds a Wi-Fi network profile to the non-volatile storage (NVS).
@param[in] ntwrk_prof Pointer to the Wi-Fi network profile to add.
@param[out] index Index of the added network profile in the NVS.
@return ESP_OK on success, ESP_ERR_INVALID_ARG if the arguments are invalid, or ESP_FAIL if the operation fails.
*/
esp_err_t wifim_network_add(const wifi_config_t *const ntwrk_prof, uint16_t *index);

/**
 * @brief Get the number of saved Wi-Fi networks
 *
 * This function returns the number of saved Wi-Fi networks.
 *
 * @param[out] num_ntwk Pointer to a variable to store the number of saved networks
 *
 * @return ESP_OK if successful, otherwise an error code indicating the reason for failure
 */
esp_err_t wifim_get_saved_network(uint16_t *num_ntwk);

/**
 * @brief Get the network profile for a saved network
 *
 * This function retrieves the network profile for a saved Wi-Fi network,
 * identified by its index.
 *
 * @param[out] ntwrk_prof Pointer to a wifi_config_t structure to store the network profile
 * @param[in] usIndex Index of the saved Wi-Fi network
 *
 * @return ESP_OK if successful, otherwise an error code indicating the reason for failure
 */
esp_err_t wifim_get_network_profile(wifi_config_t *ntwrk_prof, uint16_t usIndex);

/**
 * @brief Delete a saved Wi-Fi network
 *
 * This function deletes a saved Wi-Fi network, identified by its index.
 *
 * @param[in] index Index of the saved Wi-Fi network to delete
 *
 * @return ESP_OK if successful, otherwise an error code indicating the reason for failure
 */
esp_err_t wifim_delete_network(uint16_t index);

/**
 * @brief Ping an IP address
 *
 * This function pings an IP address a specified number of times with a
 * specified interval between pings.
 *
 * @param[in] ip_addr_str IP address to ping, in string format (e.g. "192.168.1.1")
 * @param[in] count Number of times to ping the IP address
 * @param[in] interval_ms Interval between pings, in milliseconds
 *
 * @return ESP_OK if successful, otherwise an error code indicating the reason for failure
 */
esp_err_t wifm_ping(uint8_t *ip_addr_str, uint16_t count, uint32_t interval_ms);

/**
 * @brief Get IP address information
 *
 * This function retrieves IP address information for the Wi-Fi interface.
 *
 * @param[out] ip_addr Pointer to an ip_addr_t structure to store the IP address information
 *
 * @return ESP_OK if successful, otherwise an error code indicating the reason for failure
 */
esp_err_t wifm_get_ip_info(ip_addr_t *ip_addr);

/**
 * @brief Get the MAC address of the device.
 *
 * @param[out] mac Pointer to the MAC address buffer where the result will be stored.
 *                 The buffer must be at least 6 bytes long.
 *
 * @return ESP_OK on success, or an error code otherwise.
 */
esp_err_t wifim_get_mac(uint8_t *mac);

/**
 * @brief Get the IP address information of the device.
 *
 * @param[out] ip_addr Pointer to the IP address buffer where the result will be stored.
 *                     Must be of type `ip_addr_t`.
 *
 * @return ESP_OK on success, or an error code otherwise.
 */
esp_err_t wifm_get_ip_info(ip_addr_t *ip_addr);

/**
 * @brief Get the IP address of a given host.
 *
 * @param[in] host The hostname or IP address to resolve.
 * @param[out] ip_addr Pointer to the IP address buffer where the result will be stored.
 *                     Must be of type `ip_addr_t`.
 *
 * @return ESP_OK on success, or an error code otherwise.
 */
esp_err_t wifm_get_host_ip(char *host, ip_addr_t *ip_addr);

/**
 * @brief Start the Wi-Fi access point (AP) mode.
 *
 * @return ESP_OK on success, or an error code otherwise.
 */
esp_err_t wifim_start_ap(void);

/**
 * @brief Stop the Wi-Fi access point (AP) mode.
 *
 * @return ESP_OK on success, or an error code otherwise.
 */
esp_err_t wifim_stop_ap(void);

/**
 * @brief Configure the Wi-Fi access point (AP) mode.
 *
 * @param[in] ntwk_param Pointer to the Wi-Fi configuration structure.
 *
 * @return ESP_OK on success, or an error code otherwise.
 */
esp_err_t wifim_configure_ap(const wifi_config_t *const ntwk_param);

/**
 * @brief Set the Wi-Fi power save (PS) mode.
 *
 * @param[in] wifi_pm_mode The power save mode to set. Must be of type `wifi_ps_type_t`.
 *
 * @return ESP_OK on success, or an error code otherwise.
 */
esp_err_t wifim_set_pm_mode(wifi_ps_type_t wifi_pm_mode);

/**
 * @brief Get the Wi-Fi power save (PS) mode.
 *
 * @param[out] wifi_pm_mode Pointer to the variable where the power save mode will be stored.
 *                          Must be of type `wifi_ps_type_t`.
 *
 * @return ESP_OK on success, or an error code otherwise.
 */
esp_err_t wifim_get_pm_mode(wifi_ps_type_t *wifi_pm_mode);

/**
 * @brief This function retrieves the connection information of the Wi-Fi interface, including the
 * current connection status, SSID, BSSID, channel and other information.
 * @param[in,out] connection_info Pointer to a wifi_config_t structure to store the connection information.
 * @return
 * ESP_OK: Success
 * ESP_ERR_INVALID_ARG: Invalid argument
 * ESP_ERR_WIFI_NOT_INIT: Wi-Fi driver is not initialized
 */
esp_err_t wifim_get_connection_info(wifi_config_t *connection_info);

/**
 * @brief This function retrieves the connection information of the Wi-Fi interface, including the
 * current connection status, SSID, BSSID, channel and other information.
 * @param[in,out] connection_info Pointer to a wifi_config_t structure to store the connection information.
 * @return
 * ESP_OK : Successful
 * ESP_FAIL : Failed to retrieve the RSSI value.
 */
esp_err_t wifim_get_rssi(int *rssi);

/**
 * @brief Register a callback function for a specific WiFi event.
 * This function allows you to register a callback function to handle a specific WiFi event.
 * @param event_type The type of WiFi event to register the callback for.
 * @param handler The callback function to be invoked when the specified WiFi event occurs.
 * @return esp_err_t Returns ESP_OK if the callback registration is successful, or an error code if it fails.
 */
esp_err_t wifim_register_event_cb(wifi_event_t event_type, wifi_event_handler_t handler);

/*-------------------------- WIFI PROV CODE -------------------*/
/**
 * @brief Disconnects from the Wi-Fi network.
 *
 * @return ESP_OK on success, or an error code if the disconnection fails.
 */
esp_err_t wifi_disconnect_wifi(void);

/**
 * @brief Reconnects to the Wi-Fi network.
 *
 * @return ESP_OK on success, or an error code if the reconnection fails.
 */
esp_err_t wifim_reconnect_wifi(void);

/**
 * @brief Resets the Wi-Fi manager.
 *
 * @return ESP_OK on success, or an error code if the reset fails.
 */
esp_err_t wifim_reset(void);

/**
 * @brief Restarts the reprovisioning process.
 *
 * @return ESP_OK on success, or an error code if the restart fails.
 */
esp_err_t wifim_restart_reprovisioning(void);

/**
 * @brief Starts the Wi-Fi provisioning process.
 *
 * @return ESP_OK on success, or an error code if the provisioning fails to start.
 */
esp_err_t wifim_start_provisioning(void);

/**
 * @brief Deinitializes the Wi-Fi provisioning manager.
 *
 * @return ESP_OK on success, or an error code if the deinitialization fails.
 */
esp_err_t wifi_prov_deinit(void);

/**
 * @brief Initializes the Wi-Fi provisioning manager.
 *
 * @return ESP_OK on success, or an error code if the initialization fails.
 */
esp_err_t wifim_prov_init(void);

#ifdef __cplusplus
}
#endif
#endif /* WIFI_MANAGER_H_ */
