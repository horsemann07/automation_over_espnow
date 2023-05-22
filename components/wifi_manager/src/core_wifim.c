
/**
 * @file core_wifim.c
 * @author your name (you@domain.com)
 * @brief
 * @version 0.1
 * @date 2023-05-01
 *
 * @copyright Copyright (c) 2023
 *
 */
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"

#include "esp_event.h"
#include "esp_log.h"

#include "stdlib.h"
#include "string.h"

#include "esp_smartconfig.h"
#include "nvs.h"
#include "nvs_flash.h"

#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "ping/ping_sock.h"
#include <lwip/dns.h>
#include <lwip/icmp.h>
#include <lwip/inet_chksum.h>
#include <lwip/ip_addr.h>

/* Socket and WiFi interface includes. */
#include "wifim.h"

#define TAG ("coreWIFIM")

#define WIFI_FLASH_NS "WiFi"
#define MAX_WIFI_KEY_WIDTH (5)
#define MAX_SECURITY_MODE_LEN (1)
#define MAX_AP_CONNECTIONS (4)

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define CONNECTED_BIT (BIT0)
#define DISCONNECTED_BIT (BIT1)
#define STARTED_BIT (BIT2)
#define STOPPED_BIT (BIT3)
#define AUTH_CHANGE_BIT (BIT4)
#define AP_STARTED_BIT (BIT4)
#define AP_STOPPED_BIT (BIT5)
#define ESPTOUCH_DONE_BIT (BIT6)

#define CHECK_VALID_SSID_LEN(x) ((x) > 0 && (x) <= WIFIM_MAX_SSID_LEN)
#define CHECK_VALID_PASSPHRASE_LEN(x) ((x) > 0 && (x) <= WIFIM_MAX_PASSPHRASE_LEN)

typedef struct storage_registry
{
    uint16_t num_ntwrk;
    uint16_t next_storage_idx;
    uint16_t storage[WIFIM_MAX_NETWORK_PROFILES];
} storage_registry_t;

static bool wifi_conn_state = false;
static bool wifi_ap_state = false;
static bool wifi_auth_failure = false;
static bool wifi_started = false;
static bool is_registry_init = false;

static storage_registry_t g_registry = {0};
static wifi_config_t cur_connected_wifi_ntwrk;
static esp_netif_t *esp_netif_info;
static esp_netif_t *esp_softap_netif_info;
static system_event_info_t *info;
static EventGroupHandle_t s_wifi_event_group;
static wifi_event_handler_t g_wifi_event_cb[WIFI_EVENT_MAX] = {0};

/**
 * @brief Semaphore for WiFI module.
 */
static SemaphoreHandle_t s_wifi_sem; /**< WiFi module semaphore. */

/**
 * @brief Maximum time to wait in ticks for obtaining the WiFi semaphore
 * before failing the operation.
 */
static const TickType_t sem_wait_ticks = pdMS_TO_TICKS(WIFIM_MAX_SEMAPHORE_WAIT_TIME_MS);

/**
 * @brief
 *
 */
TaskHandle_t wifim_task_handle = NULL;

/**
 * @brief wifi manager task use to manager the wifi connection.
 *
 * @param parameter NULL
 */
void wifim_task(void *parameter);

/**
 * @brief Function to set a memory block to zero.
 * The function sets memory to zero using a volatile pointer so that compiler
 * wont optimize out the function if the buffer to be set to zero is not used further.
 *
 * @param pBuf Pointer to buffer to be set to zero
 * @param size Length of the buffer to be set zero
 */
static void prvMemzero(void *pBuf, size_t size)
{
    volatile uint8_t *pMem = pBuf;
    uint32_t i;

    for (i = 0U; i < size; i++)
    {
        pMem[i] = 0U;
    }
}

static esp_err_t validate_ssid(const char *ssid)
{
    if (!ssid)
    {
        return ESP_ERR_INVALID_ARG;
    }

    int ssid_len = strlen(ssid);

    // Check length
    if (ssid_len < 1 || ssid_len > WIFIM_MAX_SSID_LEN)
    {
        return ESP_ERR_INVALID_SIZE; // Invalid length
    }

    // Check characters
    for (int i = 0; i < ssid_len; i++)
    {
        if (ssid[i] < 32 || ssid[i] > 126)
        {
            return ESP_ERR_INVALID_STATE; // Invalid character
        }
    }

    // All checks passed
    return ESP_OK;
}

static esp_err_t validate_password(const char *password)
{
    if (!password)
    {
        return ESP_ERR_INVALID_ARG;
    }
    int password_len = strlen(password);

    // Check length
    if (password_len < 1 || password_len > WIFIM_MAX_PASSPHRASE_LEN)
    {
        return ESP_ERR_INVALID_SIZE; // Invalid length
    }

    // Check characters
    for (int i = 0; i < password_len; i++)
    {
        if (password[i] < 32 || password[i] > 126)
        {
            return ESP_ERR_INVALID_STATE; // Invalid character
        }
    }

    // All checks passed
    return ESP_OK;
}
/* Compare function to sort the WiFi scan result array based on RSSI */
static int compare_rssi(const void *a, const void *b)
{
    const wifi_ap_record_t *ap1 = (const wifi_ap_record_t *)a;
    const wifi_ap_record_t *ap2 = (const wifi_ap_record_t *)b;
    return ap2->rssi - ap1->rssi;
}

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    /* For accessing reason codes in case of disconnection */
    system_event_info_t *info = &((system_event_t *)event_data)->event_info;
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
            xEventGroupClearBits(s_wifi_event_group, STOPPED_BIT);
            xEventGroupSetBits(s_wifi_event_group, STARTED_BIT);
            break;
        case WIFI_EVENT_STA_STOP:
            ESP_LOGI(TAG, "WIFI_EVENT_STA_STOP");
            xEventGroupClearBits(s_wifi_event_group, STARTED_BIT);
            xEventGroupSetBits(s_wifi_event_group, STOPPED_BIT);
            break;
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "WIFI_EVENT_STA_DISCONNECTED FROM [%.*s] reason %d", info->disconnected.ssid_len,
                     (const char *)info->disconnected.ssid, info->disconnected.reason);
            wifi_auth_failure = false;

            /* Set code corresponding to the reason for disconnection */
            switch (info->disconnected.reason)
            {
            case WIFI_REASON_AUTH_EXPIRE:
            case WIFI_REASON_ASSOC_EXPIRE:
            case WIFI_REASON_AUTH_LEAVE:
            case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
            case WIFI_REASON_BEACON_TIMEOUT:
            case WIFI_REASON_AUTH_FAIL:
            case WIFI_REASON_ASSOC_FAIL:
            case WIFI_REASON_HANDSHAKE_TIMEOUT:
                ESP_LOGD(TAG, "STA AUTH ERROR");
                wifi_auth_failure = true;
                break;
            case WIFI_REASON_NO_AP_FOUND:
                ESP_LOGD(TAG, "STA AP NOT FOUND");
                wifi_auth_failure = true;
                break;
            default:
                break;
            }

            wifi_conn_state = false;
            xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
            xEventGroupSetBits(s_wifi_event_group, DISCONNECTED_BIT);
            break;
        case WIFI_EVENT_STA_AUTHMODE_CHANGE:
            ESP_LOGI(TAG, "WIFI_EVENT_STA_AUTHMODE_CHANGE");
            xEventGroupSetBits(s_wifi_event_group, AUTH_CHANGE_BIT);
            break;
        case WIFI_EVENT_AP_START:
            ESP_LOGI(TAG, "WIFI_EVENT_AP_START");
            wifi_ap_state = true;
            xEventGroupClearBits(s_wifi_event_group, AP_STOPPED_BIT);
            xEventGroupSetBits(s_wifi_event_group, AP_STARTED_BIT);
            break;
        case WIFI_EVENT_AP_STOP:
            ESP_LOGI(TAG, "WIFI_EVENT_AP_STOP");
            wifi_ap_state = false;
            xEventGroupClearBits(s_wifi_event_group, AP_STARTED_BIT);
            xEventGroupSetBits(s_wifi_event_group, AP_STOPPED_BIT);
            break;
        case WIFI_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "WIFI_EVENT_AP_STACONNECTED");
            break;
        case WIFI_EVENT_AP_STADISCONNECTED:
            ESP_LOGI(TAG, "WIFI_EVENT_AP_STADISCONNECTED");
            break;
        default:
            break;
        }
    }
    else if (event_base == IP_EVENT)
    {
        switch (event_id)
        {
        case IP_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "SYSTEM_EVENT_STA_GOT_IP");
            wifi_conn_state = true;
            xEventGroupClearBits(s_wifi_event_group, DISCONNECTED_BIT);
            xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
            break;

        default:
            break;
        }
    }
}
/*-----------------------------------------------------------*/

static void sc_callback(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id)
    {
    case SC_EVENT_SCAN_DONE:
        ESP_LOGI(TAG, "SC_EVENT_SCAN_DONE");
        break;
    case SC_EVENT_FOUND_CHANNEL:
        ESP_LOGI(TAG, "SC_EVENT_FOUND_CHANNEL");
        break;
    case SC_EVENT_GOT_SSID_PSWD:
    {
        ESP_LOGI(TAG, "SC_EVENT_GOT_SSID_PSWD");
        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        wifi_config_t wifi_config;
        uint8_t ssid[33] = {0};
        uint8_t password[65] = {0};

        memset(&wifi_config, 0, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
        wifi_config.sta.bssid_set = evt->bssid_set;
        if (wifi_config.sta.bssid_set)
        {
            memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
        }

        memcpy(ssid, evt->ssid, sizeof(evt->ssid));
        memcpy(password, evt->password, sizeof(evt->password));
        ESP_LOGI(TAG, "SSID:%s", ssid);
        esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
        memset(&wifi_config, 0, sizeof(wifi_config_t));
        memset(password, 0, sizeof(password));
        esp_wifi_connect();
    }
    break;
    case SC_EVENT_SEND_ACK_DONE:
        ESP_LOGI(TAG, "SC_EVENT_SEND_ACK_DONE");
        xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
        break;
    default:
        break;
    }
}
/*-----------------------------------------------------------*/
static esp_err_t esp_hal_wifi_start(void)
{
    if (esp_wifi_start() != ESP_OK)
    {
        ESP_LOGE(TAG, "%s: failed to start the wifi", __func__);
        return ESP_FAIL; // Return ESP_FAIL in case of failure
    }

    if (xEventGroupWaitBits(s_wifi_event_group, STARTED_BIT | AP_STARTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY))
    {
        ESP_LOGI(TAG, "Waiting for WIFI connection...");
        wifi_started = true;
        return ESP_OK;
    }

    return ESP_FAIL; // Return ESP_FAIL if waiting for bits times out
}
/*-----------------------------------------------------------*/
static esp_err_t esp_hal_wifi_stop(void)
{
    esp_err_t ret = esp_wifi_stop();
    if (ret != ESP_OK)
    {
        return ret;
    }

    if (xEventGroupWaitBits(s_wifi_event_group, STOPPED_BIT | AP_STOPPED_BIT, pdFALSE, pdFALSE, portMAX_DELAY))
    {
        wifi_started = false;
        return ESP_OK;
    }

    return ESP_FAIL;
}
/*-----------------------------------------------------------*/
esp_err_t wifim_provision(void)
{
    esp_err_t ret = ESP_OK;
    if (xSemaphoreTake(s_wifi_sem, sem_wait_ticks) != pdTRUE)
    {
        ESP_LOGE(TAG, "%d:%s Failed to acquire WiFi semaphore", __LINE__, __func__);
        return ESP_ERR_TIMEOUT;
    }

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "%s: Failed to set wifi mode %s", __func__, esp_err_to_name(ret));
        goto cleanup;
    }

    ret = esp_hal_wifi_start();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "%s: Failed to start wifi %s", __func__, esp_err_to_name(ret));
        goto cleanup;
    }

    esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &sc_callback, NULL);
    xEventGroupWaitBits(s_wifi_event_group, STARTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    esp_smartconfig_set_type(SC_TYPE_ESPTOUCH);
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    esp_smartconfig_start(&cfg);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "%s: Failed to start smartconfig %s", __func__, esp_err_to_name(ret));
        goto cleanup;
    }

    xEventGroupWaitBits(s_wifi_event_group, ESPTOUCH_DONE_BIT | DISCONNECTED_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
    esp_smartconfig_stop();
    esp_event_handler_unregister(SC_EVENT, ESP_EVENT_ANY_ID, &sc_callback);

    if (wifi_conn_state == true)
    {
        ret = ESP_OK;
    }

cleanup:
    xSemaphoreGive(s_wifi_sem);
    return ret;
}
/*-----------------------------------------------------------*/
esp_err_t wifim_is_connected(const wifi_config_t *network_config)
{
    esp_err_t ret = ESP_FAIL;

    if (xSemaphoreTake(s_wifi_sem, sem_wait_ticks) != pdTRUE)
    {
        ESP_LOGE(TAG, "%d:%s Failed to acquire WiFi semaphore", __LINE__, __func__);
        return ret;
    }

    if (wifi_conn_state == true && (network_config == NULL || memcmp(network_config->sta.ssid, cur_connected_wifi_ntwrk.sta.ssid,
                                                                     sizeof(cur_connected_wifi_ntwrk.sta.ssid)) == 0))
    {
        ret = ESP_OK;
    }

    xSemaphoreGive(s_wifi_sem);

    return ret;
}
/*-----------------------------------------------------------*/
esp_err_t wifim_wifi_off(void)
{
    esp_err_t ret = ESP_FAIL;

    if (xSemaphoreTake(s_wifi_sem, sem_wait_ticks) != pdTRUE)
    {
        ESP_LOGE(TAG, "%d:%s Failed to acquire WiFi semaphore", __LINE__, __func__);
        return ESP_ERR_TIMEOUT;
    }

    if (wifi_conn_state == true)
    {
        ret = esp_wifi_disconnect();
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "%s: Failed to disconnect WiFi %s", __func__, esp_err_to_name(ret));
            goto exit;
        }

        // Wait for WiFi disconnected event
        xEventGroupWaitBits(s_wifi_event_group, DISCONNECTED_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
    }

    ret = esp_wifi_deinit();
    if (ret != ESP_OK)
    {
        if (ret == ESP_ERR_WIFI_NOT_STOPPED)
        {
            ret = esp_hal_wifi_stop();
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "%s: Failed to stop WiFi %s", __func__, esp_err_to_name(ret));
                goto exit;
            }

            ret = esp_wifi_deinit();
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "%s: Failed to deinit WiFi %s", __func__, esp_err_to_name(ret));
                goto exit;
            }
        }
        else
        {
            ESP_LOGE(TAG, "%s: Failed to deinit WiFi %s", __func__, esp_err_to_name(ret));
            goto exit;
        }
    }

    if (s_wifi_event_group)
    {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }

    ret = ESP_OK;

exit:
    xSemaphoreGive(s_wifi_sem);
    return ret;
}

/*-----------------------------------------------------------*/
esp_err_t wifim_wifi_on(void)
{
    static bool event_loop_inited = false;
    esp_err_t ret = ESP_FAIL;

    if (!event_loop_inited)
    {
        ret = esp_netif_init();
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "%s: Failed to esp_netif_init (%s)", __func__, esp_err_to_name(ret));
            return ret;
        }
        ret = esp_event_loop_create_default();
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "%s: Failed to initialize the event loop (%s)", __func__, esp_err_to_name(ret));
            return ret;
        }

        esp_netif_info = esp_netif_create_default_wifi_sta();
        esp_softap_netif_info = esp_netif_create_default_wifi_ap();

        if (esp_netif_info == NULL || esp_softap_netif_info == NULL)
        {
            ESP_LOGE(TAG, "%s: Failed to create default network interfaces", __func__);
            return ESP_FAIL;
        }

        esp_event_handler_instance_t instance_any_id;
        esp_event_handler_instance_t instance_got_ip;

        ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "%s: Failed to register event handler for WIFI_EVENT", __func__);
            return ret;
        }

        ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL,
                                                  &instance_got_ip);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "%s: Failed to register event handler for IP_EVENT", __func__);
            return ret;
        }

        event_loop_inited = true;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "%s: Failed to initialize Wi-Fi (%s)", __func__, esp_err_to_name(ret));
        return ret;
    }

    ret = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "%s: Failed to set Wi-Fi storage (%s)", __func__, esp_err_to_name(ret));
        return ret;
    }

    if (s_wifi_event_group == NULL)
    {
        s_wifi_event_group = xEventGroupCreate();
        if (s_wifi_event_group == NULL)
        {
            ESP_LOGE(TAG, "%s: Failed to create event group", __func__);
            return ESP_FAIL;
        }
    }

    static StaticSemaphore_t xSemaphoreBuffer;
    if (s_wifi_sem == NULL)
    {
        s_wifi_sem = xSemaphoreCreateMutexStatic(&xSemaphoreBuffer);
        if (s_wifi_sem == NULL)
        {
            ESP_LOGE(TAG, "%s: Failed to create semaphore", __func__);
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

/*--------------------------------------------------------------------*/
esp_err_t wifim_connect_ap(const wifi_config_t *const ap_config)
{
    wifi_mode_t curr_mode;
    wifi_config_t wifi_config = {0};
    esp_err_t ret = ESP_OK;

    if (!ap_config || validate_ssid((const char *)&ap_config->sta.ssid) != ESP_OK || ap_config->sta.threshold.authmode == WIFI_AUTH_MAX || (ap_config->sta.threshold.authmode != WIFI_AUTH_OPEN && validate_password((const char *)&ap_config->sta.password)))
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_wifi_sem, sem_wait_ticks) != pdTRUE)
    {
        return ESP_FAIL;
    }

    if (wifi_conn_state)
    {
        ret = esp_wifi_disconnect();
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "%s: Failed to disconnect to wifi  %s", __func__, esp_err_to_name(ret));
            goto cleanup;
        }
        xEventGroupWaitBits(s_wifi_event_group, DISCONNECTED_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
    }

    ret = esp_wifi_get_mode(&curr_mode);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "%s: Failed to get wifi mode %s", __func__, esp_err_to_name(ret));
        goto cleanup;
    }

    if (curr_mode != WIFI_MODE_STA && curr_mode != WIFI_MODE_APSTA)
    {
        ret = esp_wifi_set_mode(WIFI_MODE_STA);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "%s: Failed to set wifi mode %s", __func__, esp_err_to_name(ret));
            goto cleanup;
        }
    }

    if (!wifi_started)
    {
        ret = esp_hal_wifi_start();
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "%s: Failed to start wifi %s", __func__, esp_err_to_name(ret));
            goto cleanup;
        }

        xEventGroupWaitBits(s_wifi_event_group, STARTED_BIT | AP_STARTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

        ESP_LOGI(TAG, "Waiting for WIFI connection...");
        wifi_started = true;
    }

    strncpy((char *)wifi_config.sta.ssid, (const char *)ap_config->sta.ssid, sizeof(wifi_config.sta.ssid) - 1);
    wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';

    if (ap_config->sta.threshold.authmode != WIFI_AUTH_OPEN)
    {
        strncpy((char *)wifi_config.sta.password, (const char *)ap_config->sta.password,
                sizeof(wifi_config.sta.password) - 1);
        wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';
    }

    ret = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    memset(&wifi_config, 0, sizeof(wifi_config_t));

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "%s: Failed to set wifi config %s", __func__, esp_err_to_name(ret));
        goto cleanup;
    }

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_connect());

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT | DISCONNECTED_BIT, pdTRUE, pdFALSE, portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & CONNECTED_BIT)
    {
        ESP_LOGD(TAG, "connected to ap SSID:%s password:%s", (char *)wifi_config.sta.ssid,
                 (char *)wifi_config.sta.password);
    }
    else if (bits & DISCONNECTED_BIT)
    {
        ESP_LOGD(TAG, "Failed to connect to SSID:%s, password:%s", (char *)wifi_config.sta.ssid,
                 (char *)wifi_config.sta.password);
        ret = ESP_FAIL;
    }

    if (wifi_conn_state)
    {
        memcpy(&cur_connected_wifi_ntwrk, ap_config, sizeof(wifi_config_t));
        ret = ESP_OK;
    }

cleanup:
    xSemaphoreGive(s_wifi_sem);
    return ret;
}

esp_err_t wifim_reconnect(void)
{
    ESP_LOGI(TAG, "Reconnecting to %s", cur_connected_wifi_ntwrk.sta.ssid);
    uint16_t num = 0;
    wifi_config_t wifi_sta_conf = {0};

    esp_err_t ret = wifim_connect_ap(&cur_connected_wifi_ntwrk);
    if (ret == ESP_OK)
    {
        return ret;
    }

    /* get number of saved wifi networks */
    wifim_get_saved_network(&num);
    if (num == 0)
    {
        // TODO: start ble-wifi provisioning
        return ESP_FAIL; // Placeholder return statement for provisioning
    }

    for (int idx = 0; idx < num; idx++)
    {
        ret = wifim_get_network_profile(&wifi_sta_conf, idx);
        if (ret != ESP_OK)
        {
            ESP_LOGD(TAG, "failed to get the wifi profile at index %d", idx);
            continue;
        }

        ret = wifim_configure_ap((const wifi_config_t *)&wifi_sta_conf);
        if (ret == ESP_OK)
        {
            break;
        }
    }

    return ret;
}
/*-----------------------------------------------------------*/

esp_err_t wifim_disconnect(void)
{
    esp_err_t ret = ESP_FAIL;

    /* Try to acquire the semaphore. */
    if (!s_wifi_sem)
    {
        ESP_LOGI(TAG, "%d:%s s_wifi_sem null", __LINE__, __func__);
    }
    if (xSemaphoreTake(s_wifi_sem, sem_wait_ticks) == pdTRUE)
    {
        // Check if WiFi is already disconnected
        if (wifi_conn_state == false)
        {
            xSemaphoreGive(s_wifi_sem);
            return ESP_OK;
        }

        ret = esp_wifi_disconnect();
        if (ret == ESP_OK)
        {
            // Wait for wifi disconnected event
            xEventGroupWaitBits(s_wifi_event_group, DISCONNECTED_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
        }
        /* Return the semaphore. */
        xSemaphoreGive(s_wifi_sem);
    }
    else
    {
        ret = ESP_ERR_TIMEOUT;
    }
    return ret;
}
/*-----------------------------------------------------------*/

esp_err_t wifim_reset(void)
{
    esp_err_t ret = ESP_OK;

    /* deinitialize the wifi */
    ret = wifim_wifi_off();
    if (ret != ESP_OK)
    {
        return ret;
    }

    /* initialize wifi again */
    ret = wifim_wifi_on();
    if (ret != ESP_OK)
    {
        return ret;
    }

    /* connected to previous connected network */
    ret = wifim_connect_ap(&cur_connected_wifi_ntwrk);
    if (ret != ESP_OK)
    {
        return ret;
    }
    return ret;
}

/*-----------------------------------------------------------*/

esp_err_t wifim_factory_reset(void)
{
    esp_err_t ret = ESP_OK;

    /* deinitialize the wifi */
    ret = wifim_wifi_off();
    if (ret != ESP_OK)
    {
        return ret;
    }

    /* delete all saved network */
    for (uint16_t idx = 0; idx < g_registry.num_ntwrk; idx++)
    {
        (void)wifim_delete_network(idx);
    }
    /* Delete the static semaphore */
    if (s_wifi_sem)
    {
        vSemaphoreDelete(s_wifi_sem);
    }
    /* init global variables */
    wifi_conn_state = false;
    wifi_ap_state = false;
    wifi_auth_failure = false;
    wifi_started = false;

    esp_netif_info = NULL;
    esp_softap_netif_info = NULL;
    is_registry_init = false;
    memset(&g_registry, 0, sizeof(g_registry));
    memset(&cur_connected_wifi_ntwrk, 0, sizeof(wifi_config_t));

    return ret;
}
/*-----------------------------------------------------------*/

esp_err_t wifim_scan(wifi_ap_record_t *scanned_ap, uint16_t num_aps)
{
    esp_err_t ret = ESP_FAIL;
    wifi_mode_t curr_mode;
    wifi_config_t wifi_config = {0};

    if (!scanned_ap || num_aps == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    /* set init parameter for scanning */
    wifi_scan_config_t temp_scan = {.ssid = NULL, .bssid = NULL, .channel = 0, .show_hidden = true};

    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;

    /* Try to acquire the semaphore. */
    if (!s_wifi_sem)
    {
        ESP_LOGI(TAG, "%d:%s s_wifi_sem null", __LINE__, __func__);
    }
    if (xSemaphoreTake(s_wifi_sem, sem_wait_ticks) == pdTRUE)
    {
        ret = esp_wifi_get_mode(&curr_mode);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "%s: Failed to set wifi mode %s", __func__, esp_err_to_name(ret));
            xSemaphoreGive(s_wifi_sem);
            return ret;
        }

        if (curr_mode != WIFI_MODE_STA)
        {
            ret = esp_wifi_set_mode(WIFI_MODE_STA);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "%s: Failed to set wifi mode %s", __func__, esp_err_to_name(ret));
                xSemaphoreGive(s_wifi_sem);
                return ret;
            }

            ret = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "%s: Failed to set wifi config %s", __func__, esp_err_to_name(ret));
                xSemaphoreGive(s_wifi_sem);
                return ret;
            }
        }

        if (!wifi_started)
        {
            ret = esp_hal_wifi_start();
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "%s: Failed to start wifi %s", __func__, esp_err_to_name(ret));
                xSemaphoreGive(s_wifi_sem);
                return ret;
            }
        }

        if (wifi_conn_state == false && wifi_auth_failure == true)
        {
            /* It seems that WiFi needs explicit disassoc before scan request post
             * attempt to connect to invalid network name or SSID.
             */
            esp_wifi_disconnect();
            xEventGroupWaitBits(s_wifi_event_group, DISCONNECTED_BIT, pdTRUE, pdFALSE, 30);
        }

        ret = esp_wifi_scan_start(&temp_scan, true);
        if (ret == ESP_OK)
        {
            esp_wifi_scan_get_ap_records(&num_aps, scanned_ap);
            /* Sort the scan result array based on RSSI */
            qsort(scanned_ap, num_aps, sizeof(wifi_ap_record_t), compare_rssi);
        }

        /* Return the semaphore. */
        xSemaphoreGive(s_wifi_sem);
    }
    else
    {
        ret = ESP_ERR_TIMEOUT;
    }

    return ret;
}
/*-----------------------------------------------------------*/

esp_err_t wifim_set_mode(wifi_mode_t mode)
{
    esp_err_t ret = ESP_OK;

    /* Try to acquire the semaphore. */
    if (!s_wifi_sem)
    {
        ESP_LOGI(TAG, "%d:%s s_wifi_sem null", __LINE__, __func__);
    }
    if (xSemaphoreTake(s_wifi_sem, sem_wait_ticks) == pdTRUE)
    {
        ret = esp_wifi_set_mode(WIFI_MODE_STA);
        /* Return the semaphore. */
        xSemaphoreGive(s_wifi_sem);
    }
    {
        ret = ESP_ERR_TIMEOUT;
    }
    return ret;
}
/*-----------------------------------------------------------*/

esp_err_t wifim_get_mode(wifi_mode_t *mode)
{
    esp_err_t ret;

    if (mode == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    /* Try to acquire the semaphore. */
    if (!s_wifi_sem)
    {
        ESP_LOGI(TAG, "%d:%s s_wifi_sem null", __LINE__, __func__);
    }
    if (xSemaphoreTake(s_wifi_sem, sem_wait_ticks) == pdTRUE)
    {
        ret = esp_wifi_get_mode(mode);
        /* Return the semaphore. */
        xSemaphoreGive(s_wifi_sem);
    }
    else
    {
        ret = ESP_ERR_TIMEOUT;
    }
    return ret;
}
/*-----------------------------------------------------------*/

esp_err_t wifim_serialize_network_profile(const wifi_config_t *const network, uint8_t *buffer, uint32_t *size)
{
    esp_err_t ret = ESP_OK;
    uint32_t ulSize;

    if (!network)
    {
        return ESP_ERR_INVALID_ARG;
    }
    uint16_t ssid_len = strlen((const char *)&network->sta.ssid);
    uint16_t pswd_len = strlen((const char *)&network->sta.password);
    if (buffer == NULL)
    {
        /**
         * Add size with null termination in the flash.
         */
        ulSize = ssid_len + 1;
        ulSize += pswd_len + 1;
        ulSize += WIFIM_MAX_BSSID_LEN;
        ulSize += MAX_SECURITY_MODE_LEN;

        *size = ulSize;
    }
    else
    {
        ulSize = *size;
        memset(buffer, 0x00, ulSize);
        if (ssid_len < ulSize)
        {
            memcpy(buffer, network->sta.ssid, ssid_len);
            buffer += (ssid_len + 1);
            ulSize -= (ssid_len + 1);
        }
        else
        {
            ret = ESP_FAIL;
        }

        if (RESET_REASON_CORE_DEEP_SLEEP == ESP_OK)
        {
            if (pswd_len < ulSize)
            {
                memcpy(buffer, network->sta.password, pswd_len);
                buffer += (pswd_len + 1);
                ulSize -= (pswd_len + 1);
            }
            else
            {
                ret = ESP_FAIL;
            }
        }

        if (ret == ESP_OK)
        {
            if (WIFIM_MAX_BSSID_LEN < ulSize)
            {
                memcpy(buffer, network->sta.bssid, WIFIM_MAX_BSSID_LEN);
                buffer += WIFIM_MAX_BSSID_LEN;
                ulSize -= WIFIM_MAX_BSSID_LEN;
            }
            else
            {
                ret = ESP_FAIL;
            }
        }

        if (ret == ESP_OK)
        {
            if (ulSize >= MAX_SECURITY_MODE_LEN)
            {
                *buffer = (uint8_t)network->sta.threshold.authmode;
            }
            else
            {
                ret = ESP_FAIL;
            }
        }
    }

    return ret;
}

esp_err_t wifim_deserialize_network_profile(wifi_config_t *const ntwrk_prof, uint8_t *buffer, uint32_t size)
{
    esp_err_t ret = ESP_OK;
    uint32_t ulLen;

    if (!buffer)
    {
        return ESP_ERR_INVALID_ARG;
    }
    ulLen = strlen((char *)buffer);
    if (ulLen <= WIFIM_MAX_SSID_LEN)
    {
        memcpy(ntwrk_prof->sta.ssid, buffer, ulLen);
        buffer += (ulLen + 1);
    }
    else
    {
        ret = ESP_FAIL;
    }

    if (ret == ESP_OK)
    {
        ulLen = strlen((char *)buffer);
        if (ulLen <= WIFIM_MAX_PASSPHRASE_LEN)
        {
            memcpy(ntwrk_prof->sta.password, buffer, ulLen);
            buffer += (ulLen + 1);
        }
        else
        {
            ret = ESP_FAIL;
        }
    }

    if (ret == ESP_OK)
    {
        memcpy(ntwrk_prof->sta.bssid, buffer, WIFIM_MAX_BSSID_LEN);
        buffer += WIFIM_MAX_BSSID_LEN;
        ntwrk_prof->sta.threshold.authmode = (wifi_auth_mode_t)*buffer;
    }

    return ret;
}

esp_err_t wifim_store_network_profile(nvs_handle_t xNvsHandle, const wifi_config_t *ntwrk_prof, uint16_t usIndex)
{
    uint32_t ulSize = 0;
    uint8_t *buffer;
    char cWifiKey[MAX_WIFI_KEY_WIDTH + 1] = {0};
    esp_err_t ret = ESP_FAIL;

    (void)wifim_serialize_network_profile(ntwrk_prof, NULL, &ulSize);
    buffer = calloc(1, ulSize);

    if (buffer != NULL)
    {
        ret = wifim_serialize_network_profile(ntwrk_prof, buffer, &ulSize);
    }

    if (ret == ESP_OK)
    {
        snprintf(cWifiKey, MAX_WIFI_KEY_WIDTH + 1, "%d", usIndex);
        ret = nvs_set_blob(xNvsHandle, cWifiKey, buffer, ulSize);
        free(buffer);
    }

    return ret;
}

esp_err_t wifim_get_stored_network_profile(nvs_handle_t xNvsHandle, wifi_config_t *ntwrk_prof, uint16_t usIndex)
{
    uint32_t ulSize = 0;
    uint8_t *pucBuffer = NULL;
    char cWifiKey[MAX_WIFI_KEY_WIDTH + 1] = {0};
    esp_err_t ret = ESP_FAIL;

    if (ntwrk_prof != NULL)
    {
        snprintf(cWifiKey, MAX_WIFI_KEY_WIDTH + 1, "%d", usIndex);
        ret = nvs_get_blob(xNvsHandle, cWifiKey, NULL, &ulSize);

        if (ret == ESP_OK)
        {
            pucBuffer = calloc(1, ulSize);
            if (pucBuffer != NULL)
            {
                ret = nvs_get_blob(xNvsHandle, cWifiKey, pucBuffer, &ulSize);
            }
            else
            {
                ret = ESP_FAIL;
            }
        }

        if (ret == ESP_OK)
        {
            ret = wifim_deserialize_network_profile(ntwrk_prof, pucBuffer, ulSize);
            free(pucBuffer);
        }
    }

    return ret;
}

esp_err_t wifim_init_resgistry(nvs_handle xNvsHandle)
{
    esp_err_t ret = ESP_OK;
    uint32_t ulSize = sizeof(storage_registry_t);
    ret = nvs_get_blob(xNvsHandle, "registry", &g_registry, &ulSize);

    if (ret == ESP_ERR_NVS_NOT_FOUND)
    {
        memset(&g_registry, 0x00, sizeof(storage_registry_t));
        ret = ESP_OK;
    }
    if (ret == ESP_OK)
    {
        is_registry_init = true;
    }

    return ret;
}

esp_err_t wifim_network_add(const wifi_config_t *const ntwrk_prof, uint16_t *index)
{
    esp_err_t ret = ESP_OK;
    nvs_handle xNvsHandle;
    bool opened = true;

    if (ntwrk_prof != NULL && index != NULL)
    {
        /* Try to acquire the semaphore. */
        if (xSemaphoreTake(s_wifi_sem, sem_wait_ticks) == pdTRUE)
        {
            ret = nvs_open(WIFI_FLASH_NS, NVS_READWRITE, &xNvsHandle);
            if (ret == ESP_OK)
            {
                opened = true;
                if (is_registry_init == false)
                {
                    ret = wifim_init_resgistry(xNvsHandle);
                }
            }
            if (ret == ESP_OK)
            {
                if (g_registry.num_ntwrk == WIFIM_MAX_NETWORK_PROFILES)
                {
                    ret = ESP_FAIL;
                }
            }

            if (ret == ESP_OK)
            {
                ret = wifim_store_network_profile(xNvsHandle, ntwrk_prof, g_registry.next_storage_idx);
            }

            if (ret == ESP_OK)
            {
                g_registry.storage[g_registry.num_ntwrk] = g_registry.next_storage_idx;
                g_registry.next_storage_idx++;
                g_registry.num_ntwrk++;
                ret = nvs_set_blob(xNvsHandle, "registry", &g_registry, sizeof(g_registry));
            }

            // Commit
            if (ret == ESP_OK)
            {
                ret = nvs_commit(xNvsHandle);
            }

            if (ret == ESP_OK)
            {
                *index = (g_registry.num_ntwrk - 1);
            }

            // Close
            if (opened)
            {
                nvs_close(xNvsHandle);
            }

            xSemaphoreGive(s_wifi_sem);
        }
    }

    return ret;
}
/*-----------------------------------------------------------*/

esp_err_t wifim_get_saved_network(uint16_t *num_ntwk)
{
    uint16_t idx;
    wifi_config_t profile;
    esp_err_t ret = ESP_OK;
    for (idx = 0; idx < WIFIM_MAX_NETWORK_PROFILES; idx++)
    {
        ret = wifim_get_network_profile(&profile, idx);

        if (ret != ESP_OK)
        {
            break;
        }
    }
    *num_ntwk = idx;
    return ret;
}

esp_err_t wifim_get_network_profile(wifi_config_t *ntwrk_prof, uint16_t usIndex)
{
    esp_err_t ret = ESP_FAIL;
    nvs_handle xNvsHandle;
    bool opened = false;

    if (!ntwrk_prof)
    {
        return ESP_ERR_INVALID_ARG;
    }

    /* Try to acquire the semaphore. */

    if (!s_wifi_sem)
    {
        ESP_LOGI(TAG, "%d:%s s_wifi_sem null", __LINE__, __func__);
    }
    if (xSemaphoreTake(s_wifi_sem, sem_wait_ticks) == pdTRUE)
    {
        ret = nvs_open(WIFI_FLASH_NS, NVS_READWRITE, &xNvsHandle);
        if (ret == ESP_OK)
        {
            opened = true;
            if (is_registry_init == false)
            {
                ret = wifim_init_resgistry(xNvsHandle);
            }
        }
        if (ret == ESP_OK)
        {
            if (usIndex < g_registry.num_ntwrk)
            {
                ret = wifim_get_stored_network_profile(xNvsHandle, ntwrk_prof, g_registry.storage[usIndex]);
            }
            else
            {
                ret = ESP_FAIL;
            }
        }

        // Close
        if (opened)
        {
            nvs_close(xNvsHandle);
        }
        xSemaphoreGive(s_wifi_sem);
    }

    return ret;
}
/*-----------------------------------------------------------*/

esp_err_t wifim_delete_network(uint16_t index)
{
    esp_err_t ret = ESP_FAIL;
    nvs_handle xNvsHandle;
    char cWifiKey[MAX_WIFI_KEY_WIDTH + 1] = {0};
    BaseType_t opened = pdFALSE;
    uint16_t usIdx;

    /* Try to acquire the semaphore. */
    if (!s_wifi_sem)
    {
        ESP_LOGI(TAG, "%d:%s s_wifi_sem null", __LINE__, __func__);
    }
    if (xSemaphoreTake(s_wifi_sem, sem_wait_ticks) == pdTRUE)
    {
        ret = nvs_open(WIFI_FLASH_NS, NVS_READWRITE, &xNvsHandle);
        if (ret == ESP_OK)
        {
            opened = pdTRUE;
            if (is_registry_init == false)
            {
                ret = wifim_init_resgistry(xNvsHandle);
            }
        }
        if (ret == ESP_OK && index < g_registry.num_ntwrk)
        {
            snprintf(cWifiKey, MAX_WIFI_KEY_WIDTH + 1, "%d", g_registry.storage[index]);
            ret = nvs_erase_key(xNvsHandle, cWifiKey);

            if (ret == ESP_OK)
            {
                for (usIdx = index + 1; usIdx < g_registry.num_ntwrk; usIdx++)
                {
                    g_registry.storage[usIdx - 1] = g_registry.storage[usIdx];
                }
                g_registry.num_ntwrk--;
                ret = nvs_set_blob(xNvsHandle, "registry", &g_registry, sizeof(g_registry));
            }

            if (ret == ESP_OK)
            {
                ret = nvs_commit(xNvsHandle);
            }
        }

        if (ret == ESP_OK)
        {
            ret = ESP_OK;
        }

        // Close
        if (opened)
        {
            nvs_close(xNvsHandle);
        }
        xSemaphoreGive(s_wifi_sem);
    }

    return ret;
}

// #define PING_EVENT
// static void ping_cb(void *arg, esp_event_base_t event_base,
//              int32_t event_id, void *event_data)
// {
//     esp_ping_handle_t hdl = (esp_ping_handle_t)arg;

//     if (event_id == PING_EVENT_IP_RESOLVED) {
//         struct ping_ip_found *f = (struct ping_ip_found *)event_data;
//         ESP_LOGI(TAG, "PING target IP address found: %s", ip4addr_ntoa((const ip4_addr_t *)&f->ip_addr));
//         esp_ping_start(hdl, (const ip_addr_t *)&f->ip_addr, 0, NULL);
//     } else if (event_id == PING_EVENT_TIMEOUT) {
//         ESP_LOGI(TAG, "PING timeout");
//         esp_ping_stop(hdl);
//     } else if (event_id == PING_EVENT_RESULT) {
//         esp_ping_result_t *res = (esp_ping_result_t *)event_data;
//         ESP_LOGI(TAG, "PING packets sent=%u, received=%u, lost=%u, min_rtt=%u, max_rtt=%u, avg_rtt=%u",
//                  res->total_count, res->received_count, res->total_count - res->received_count,
//                  res->min_time, res->max_time, res->total_time / res->received_count);
//         esp_ping_stop(hdl);
//     }
// }

/*-----------------------------------------------------------*/

esp_err_t wifm_ping(uint8_t *ip_addr_str, uint16_t count, uint32_t inteval_ms)
{
    // ip_addr_t target_ip;
    // memset(&target_ip, 0, sizeof(target_ip));
    // if (ipaddr_aton((const char *)ip_addr_str, &target_ip))
    // {
    //     esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    //     ping_config.interval_ms = inteval_ms;
    //     ping_config.target_addr = target_ip;       // target IP address
    //     ping_config.count = count;

    //     /* set callback functions */
    //     esp_ping_callbacks_t cbs;
    //     cbs.on_ping_success = test_on_ping_success;
    //     cbs.on_ping_timeout = test_on_ping_timeout;
    //     cbs.on_ping_end = test_on_ping_end;
    //     cbs.cb_args = NULL; // arguments that will feed to all callback functions, can be NULL
    //     cbs.cb_args = eth_event_group;

    //     esp_ping_handle_t ping;
    //     esp_ping_new_session(&ping_config, &cbs, &ping);

    //     // Open a new raw socket to send and receive ICMP packets
    //     int sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    //     if (sock < 0)
    //     {
    //         ESP_LOGE(TAG, "Failed to create raw socket: %d", errno);
    //         return ESP_FAIL;
    //     }

    //     // Set the socket receive timeout
    //     struct timeval timeout;
    //     timeout.tv_sec = inteval_ms / 1000;
    //     timeout.tv_usec = (inteval_ms % 1000) * 1000;
    //     setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    //     // Send the ICMP echo request packet to the target IP address
    //     struct sockaddr_in target_addr;
    //     target_addr.sin_family = AF_INET;
    //     target_addr.sin_addr.s_addr = ip4_addr_get_u32(&target_ip.u_addr.ip4);
    //     target_addr.sin_len = sizeof(target_addr);
    //     int sendto_ret = sendto(sock, packet, packet_size, 0, (struct sockaddr *)&target_addr, sizeof(target_addr));
    //     if (sendto_ret < 0)
    //     {
    //         ESP_LOGE(TAG, "Failed to send ICMP echo request: %d", errno);
    //         close(sock);
    //         return ESP_FAIL;
    //     }

    //     // Wait for the ICMP echo reply packet
    //     uint8_t reply_packet[packet_size];
    //     struct sockaddr_in from_addr;
    //     socklen_t from_addr_len = sizeof(from_addr);
    //     int recvfrom_ret = recvfrom(sock, reply_packet, packet_size, 0, (struct sockaddr *)&from_addr,
    //     &from_addr_len); close(sock);

    //     if (recvfrom_ret < 0)
    //     {
    //         ESP_LOGE(TAG, "Failed to receive ICMP echo reply: %d", errno);
    //         return ESP_FAIL;
    //     }

    //     // Verify that the ICMP echo reply packet is valid
    //     icmp_echo_hdr *reply_hdr = (icmp_echo_hdr *)reply_packet;
    //     if (reply_hdr->type != ICMP_ER)
    //     {
    //         ESP_LOGE(TAG, "Invalid ICMP echo reply type: %d", reply_hdr->type);
    //         return ESP_FAIL;
    //     }
    //     if (reply_hdr->id != htons(0xABCD))
    //     {
    //         ESP_LOGE(TAG, "Invalid ICMP echo reply ID: %d", ntohs(reply_hdr->id));
    //         return ESP_FAIL;
    //         if (reply_hdr->seqno != htons(1))
    //         {
    //             ESP_LOGE(TAG, "Invalid ICMP echo reply sequence number: %d", ntohs(reply_hdr->seqno));
    //             return ESP_FAIL;
    //         }
    //         return ESP_OK;
    //     }
    //     else
    //     {
    //         ESP_LOGE(TAG, "Invalid IP address string: %s", ip_addr_str);
    //         return ESP_FAIL;
    //     }
    // }
    return ESP_FAIL;
}
/*-----------------------------------------------------------*/
esp_err_t wifm_get_ip_info(ip_addr_t *ip_addr)
{
    if (ip_addr == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = ESP_OK;
    memset(ip_addr, 0x00, sizeof(ip_addr_t));
    /* Try to acquire the semaphore. */
    if (!s_wifi_sem)
    {
        ESP_LOGI(TAG, "%d:%s s_wifi_sem null", __LINE__, __func__);
    }
    if (xSemaphoreTake(s_wifi_sem, sem_wait_ticks) == pdTRUE)
    {
        /* Get the IP information for the network interface */
        esp_netif_ip_info_t ip_info;
        ret = esp_netif_get_ip_info(esp_netif_info, &ip_info);
        if (ret == ESP_OK)
        {
            /* Print the IP address and subnet mask */
            ESP_LOGI(TAG, "IP Address: " IPSTR "\n", IP2STR(&ip_info.ip));
            ESP_LOGI(TAG, "Subnet Mask: " IPSTR "\n", IP2STR(&ip_info.netmask));

            // Store the IP address in the ip_addr_t pointer parameter
            // ip4_addr_set_ip4addr(ip_addr, &ip_info.ip);
            memcpy(&ip_addr->u_addr.ip4, &ip_info.ip.addr, sizeof(uint32_t));
            ip_addr->type = IPADDR_TYPE_V4;
        }
        else
        {
            ESP_LOGE(TAG, "Failed to get the IP info %s", esp_err_to_name(ret));
        }

        /* Return the semaphore. */
        xSemaphoreGive(s_wifi_sem);
    }
    else
    {
        ret = ESP_ERR_TIMEOUT;
    }
    return ret;
}
/*-----------------------------------------------------------*/

esp_err_t wifim_get_mac(uint8_t *mac)
{
    esp_err_t ret = ESP_FAIL;

    if (mac == NULL)
    {
        return ESP_FAIL;
    }

    /* Try to acquire the semaphore. */
    if (!s_wifi_sem)
    {
        ESP_LOGI(TAG, "%d:%s s_wifi_sem null", __LINE__, __func__);
    }
    if (xSemaphoreTake(s_wifi_sem, sem_wait_ticks) == pdTRUE)
    {
        wifi_mode_t mode;
        esp_err_t ret = esp_wifi_get_mode(&mode);
        if (ret == ESP_OK)
        {
            if (mode == WIFI_MODE_STA)
            {
                ret = esp_wifi_get_mac(WIFI_IF_STA, mac);
            }
            else if (mode == WIFI_MODE_AP)
            {
                ret = esp_wifi_get_mac(WIFI_IF_AP, mac);
            }
            else
            {
                ret = ESP_ERR_INVALID_ARG;
            }
        }
        /* Return the semaphore. */
        xSemaphoreGive(s_wifi_sem);
    }
    else
    {
        ret = ESP_ERR_TIMEOUT;
    }

    return ret;
}
/*-----------------------------------------------------------*/

esp_err_t wifm_get_host_ip(char *host, ip_addr_t *ip_addr)
{
    esp_err_t ret = ESP_FAIL;

    if (host == NULL || ip_addr == NULL)
    {
        return ret;
    }

    /* Try to acquire the semaphore. */
    if (!s_wifi_sem)
    {
        ESP_LOGI(TAG, "%d:%s s_wifi_sem null", __LINE__, __func__);
    }
    if (xSemaphoreTake(s_wifi_sem, sem_wait_ticks) == pdTRUE)
    {
        // Resolve the host name using DNS
        ret = dns_gethostbyname(host, ip_addr, NULL, NULL);
        if (ret != ERR_OK)
        {
            ESP_LOGE(TAG, "Failed to resolve host name '%s': %s", host, esp_err_to_name(ret));
            // Set the IP address to all zeroes to indicate an error
            IP_ADDR4(ip_addr, 0, 0, 0, 0);
        }
        /* Return the semaphore. */
        xSemaphoreGive(s_wifi_sem);
    }
    else
    {
        ret = ESP_ERR_TIMEOUT;
    }

    return ret;
}
/*-----------------------------------------------------------*/

esp_err_t wifim_start_ap(void)
{
    esp_err_t ret = ESP_FAIL;
    if (!s_wifi_sem)
    {
        ESP_LOGI(TAG, "%d:%s s_wifi_sem null", __LINE__, __func__);
    }
    if (xSemaphoreTake(s_wifi_sem, sem_wait_ticks) == pdTRUE)
    {
        if (wifi_ap_state == true)
        {
            xSemaphoreGive(s_wifi_sem);
            return ESP_OK;
        }
        if (!wifi_started)
        {
            esp_err_t ret = esp_hal_wifi_start();
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "%s: Failed to start wifi %s", __func__, esp_err_to_name(ret));
                xSemaphoreGive(s_wifi_sem);
                return ret;
            }
        }

        /* Enable DHCP server, if it's not already enabled */
        if (ESP_ERR_ESP_NETIF_INVALID_PARAMS == esp_netif_dhcps_start(esp_softap_netif_info))
        {
            ESP_LOGE(TAG, "%s: Failed to start DHCP server", __func__);
            return ESP_FAIL;
        }

        // Wait for wifi started event
        xEventGroupWaitBits(s_wifi_event_group, AP_STARTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
        ret = ESP_OK;

        /* Return the semaphore. */
        xSemaphoreGive(s_wifi_sem);
    }
    return ret;
}
/*-----------------------------------------------------------*/

esp_err_t wifim_stop_ap(void)
{
    esp_err_t ret = ESP_FAIL;

    /* Try to acquire the semaphore. */
    if (!s_wifi_sem)
    {
        ESP_LOGI(TAG, "%d:%s s_wifi_sem null", __LINE__, __func__);
    }
    if (xSemaphoreTake(s_wifi_sem, sem_wait_ticks) == pdTRUE)
    {
        // Check if AP is already stopped
        if (wifi_ap_state == false)
        {
            xSemaphoreGive(s_wifi_sem);
            return ESP_OK;
        }

        esp_err_t ret = esp_hal_wifi_stop();
        if (ret == ESP_OK)
        {
            ret = ESP_OK;
        }
        /* Return the semaphore. */
        xSemaphoreGive(s_wifi_sem);
    }
    else
    {
        ret = ESP_ERR_TIMEOUT;
    }
    return ret;
}
/*-----------------------------------------------------------*/

esp_err_t wifim_configure_ap(const wifi_config_t *const ntwk_param)
{
    if (!ntwk_param)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_FAIL;
    wifi_config_t wifi_config = {
        .ap = {
            .max_connection = MAX_AP_CONNECTIONS,
        },
    };

    if ((!CHECK_VALID_SSID_LEN(ntwk_param->ap.ssid_len) && validate_ssid((char *)&ntwk_param->ap.ssid)) || ((ntwk_param->ap.authmode != WIFI_AUTH_OPEN) && validate_password((char *)&ntwk_param->ap.password)))

        if (xSemaphoreTake(s_wifi_sem, sem_wait_ticks) == pdTRUE)
        {
            // Check if AP is already started
            if (wifi_ap_state == true)
            {
                esp_err_t ret = esp_hal_wifi_stop();
                if (ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "%s:Failed to stop wifi %s", __func__, esp_err_to_name(ret));
                }
            }

            /* ssid/password is required */
            /* SSID can be a non NULL terminated string if ssid_len is specified.
             * Hence, memcpy is used to support 32 character long SSID name.
             */
            memcpy((char *)&wifi_config.ap.ssid, ntwk_param->ap.ssid, ntwk_param->ap.ssid_len);
            wifi_config.ap.ssid_len = ntwk_param->ap.ssid_len;
            wifi_config.ap.authmode = ntwk_param->ap.authmode;

            ret = esp_wifi_set_mode(WIFI_MODE_AP);
            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "%s: Failed to set wifi mode %s", __func__, esp_err_to_name(ret));
                xSemaphoreGive(s_wifi_sem);
                return ret;
            }

            if (ntwk_param->ap.authmode != WIFI_AUTH_OPEN)
            {
                strlcpy((char *)&wifi_config.ap.password, (const char *)&ntwk_param->ap.password,
                        sizeof(wifi_config.ap.password));
            }

            ret = esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config);
            memset(&wifi_config, 0, sizeof(wifi_config_t));

            if (ret != ESP_OK)
            {
                ESP_LOGE(TAG, "%s: Failed to set wifi config %s", __func__, esp_err_to_name(ret));
                xSemaphoreGive(s_wifi_sem);
                return ret;
            }

            ret = ESP_OK;
            /* Return the semaphore. */
            xSemaphoreGive(s_wifi_sem);
        }

    return ret;
}
/*-----------------------------------------------------------*/

esp_err_t wifim_set_pm_mode(wifi_ps_type_t wifi_pm_mode)
{
    esp_err_t ret = ESP_FAIL;

    if (!s_wifi_sem)
    {
        ESP_LOGI(TAG, "%d:%s s_wifi_sem null", __LINE__, __func__);
    }
    if (xSemaphoreTake(s_wifi_sem, sem_wait_ticks) == pdTRUE)
    {
        ret = esp_wifi_set_ps(wifi_pm_mode);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "%s: Failed to set wifi power mode %s", __func__, esp_err_to_name(ret));
            xSemaphoreGive(s_wifi_sem);
            return ret;
        }
        ret = ESP_OK;
        xSemaphoreGive(s_wifi_sem);
    }
    else
    {
        ret = ESP_ERR_TIMEOUT;
    }
    return ret;
}
/*-----------------------------------------------------------*/

esp_err_t wifim_get_pm_mode(wifi_ps_type_t *wifi_pm_mode)
{
    if (!wifi_pm_mode)
    {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t ret = ESP_FAIL;

    if (!s_wifi_sem)
    {
        ESP_LOGI(TAG, "%d:%s s_wifi_sem null", __LINE__, __func__);
    }
    if (xSemaphoreTake(s_wifi_sem, sem_wait_ticks) == pdTRUE)
    {
        ret = esp_wifi_get_ps(wifi_pm_mode);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "%s: Failed to set wifi power mode %s", __func__, esp_err_to_name(ret));
            xSemaphoreGive(s_wifi_sem);
            return ret;
        }
        else
        {
            ret = ESP_OK;
            xSemaphoreGive(s_wifi_sem);
        }
    }
    else
    {
        ret = ESP_ERR_TIMEOUT;
    }
    return ret;
}
/*-----------------------------------------------------------*/
esp_err_t wifim_get_connection_info(wifi_config_t *connection_info)
{
    if (!connection_info)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!wifi_conn_state)
    {
        return ESP_ERR_WIFI_NOT_INIT;
    }
    memcpy(&connection_info, &cur_connected_wifi_ntwrk, sizeof(wifi_config_t));

    return ESP_OK;
}
/*-----------------------------------------------------------*/
esp_err_t wifim_get_rssi(int *rssi)
{
    if (!rssi)
    {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = ESP_FAIL;

    if (!s_wifi_sem)
    {
        ESP_LOGI(TAG, "%d:%s s_wifi_sem null", __LINE__, __func__);
    }
    if (xSemaphoreTake(s_wifi_sem, sem_wait_ticks) == pdTRUE)
    {
        if (wifi_conn_state)
        {
            wifi_ap_record_t ap_info;

            // Get the RSSI value of the currently connected Wi-Fi network
            ret = esp_wifi_sta_get_ap_info(&ap_info);
            if (ret == ESP_OK)
            {
                *rssi = ap_info.rssi;
            }
            else
            {
                ESP_LOGE(TAG, "Failed to get RSSI value: %s\n", esp_err_to_name(ret));
            }
        }
    }
    else
    {
        ret = ESP_ERR_TIMEOUT;
    }
    return ret;
}
/*-----------------------------------------------------------*/
esp_err_t wifim_register_event_cb(wifi_event_t event_type, wifi_event_handler_t handler)
{
    esp_err_t ret = ESP_OK;

    if (event_type > WIFI_EVENT_MAX)
    {
        return ESP_ERR_INVALID_ARG;
    }

    g_wifi_event_cb[event_type] = handler;

    return ret;
}
/*-----------------------------------------------------------*/

void wifim_task(void *parameter)
{
    (void)parameter;
    EventBits_t Bits;
    wifi_event_handler_t handler = NULL;

    ESP_LOGI(TAG, "wifi manager task started.");
    for (;;)
    {
        Bits = xEventGroupWaitBits(s_wifi_event_group,
                                   CONNECTED_BIT || DISCONNECTED_BIT || STARTED_BIT || STOPPED_BIT || AUTH_CHANGE_BIT, false, false,
                                   pdMS_TO_TICKS(portMAX_DELAY));
        if (Bits & STARTED_BIT)
        {
            if (g_wifi_event_cb[WIFI_EVENT_STA_START] != NULL)
            {
                handler = g_wifi_event_cb[WIFI_EVENT_STA_START];
                handler(WIFI_EVENT_STA_START, info);
            }
        }
        else if (Bits & STOPPED_BIT)
        {
            if (g_wifi_event_cb[WIFI_EVENT_STA_STOP] != NULL)
            {
                handler = g_wifi_event_cb[WIFI_EVENT_STA_STOP];
                handler(WIFI_EVENT_STA_STOP, info);
            }
        }
        else if (Bits & AUTH_CHANGE_BIT)
        {
            if (g_wifi_event_cb[WIFI_EVENT_STA_AUTHMODE_CHANGE] != NULL)
            {
                handler = g_wifi_event_cb[WIFI_EVENT_STA_AUTHMODE_CHANGE];
                handler(WIFI_EVENT_STA_AUTHMODE_CHANGE, info);
            }
        }
        else if (Bits & CONNECTED_BIT)
        {
            if (g_wifi_event_cb[WIFI_EVENT_STA_CONNECTED] != NULL)
            {
                handler = g_wifi_event_cb[WIFI_EVENT_STA_CONNECTED];
                handler(WIFI_EVENT_STA_CONNECTED, info);
            }
        }
        else if (Bits & DISCONNECTED_BIT)
        {
            if (g_wifi_event_cb[WIFI_EVENT_STA_DISCONNECTED] != NULL)
            {
                handler = g_wifi_event_cb[WIFI_EVENT_STA_DISCONNECTED];
                handler(WIFI_EVENT_STA_DISCONNECTED, info);
            }
            /* reconnect the wifi */
            wifim_reconnect();
        }
        else
        {
            /* nothing to do */
        }
    }
}
