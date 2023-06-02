

#include <stdio.h>
#include <string.h>

/* freertos headers */
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>

/* esp-idf headers */
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

/* wifi prov headers */
#include "wifi_provisioning/manager.h"
#ifdef CONFIG_WIFIM_PROV_TRANSPORT_BLE
/* wifi prov ble headers */
#include "wifi_provisioning/scheme_ble.h"
#endif /* CONFIG_WIFIM_PROV_TRANSPORT_BLE */

#ifdef CONFIG_WIFIM_PROV_TRANSPORT_SOFTAP
/* wifi prov softap headers */
#include "wifi_provisioning/scheme_softap.h"
#endif /* CONFIG_WIFIM_PROV_TRANSPORT_SOFTAP */
/* qr code headers */
#ifdef WIFIM_PROV_SHOW_QR
#include "qrcode.h"
#endif /*WIFIM_PROV_SHOW_QR*/
#include "espnow_utils.h"

#include "wifim.h"

static const char *TAG = "app";

#define PROV_QR_VERSION       ("v1")
#define PROV_TRANSPORT_SOFTAP ("softap")
#define PROV_TRANSPORT_BLE    ("ble")
#define QRCODE_BASE_URL       ("https://espressif.github.io/esp-jumpstart/qrcode.html")

/**
 * @brief Semaphore for WiFI module.
 */
static SemaphoreHandle_t s_wifi_sem; /**< WiFi module semaphore. */
static EventGroupHandle_t s_wifi_event_group;
/**
 * @brief Maximum time to wait in ticks for obtaining the WiFi semaphore
 * before failing the operation.
 */
static const TickType_t sem_wait_ticks = pdMS_TO_TICKS(WIFIM_MAX_SEMAPHORE_WAIT_TIME_MS);
static esp_netif_t *esp_netif_info;
static esp_netif_t *esp_softap_netif_info;
static bool wifi_conn_state = false;
static bool wifi_ap_state = false;
static bool wifi_auth_failure = false;
static bool wifi_started = false;
static bool wifi_prov_started = false;

/* Event handler for catching system events */
static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_PROV_EVENT)
    {
        switch (event_id)
        {
            case WIFI_PROV_START:
                ESP_LOGI(TAG, "Provisioning started");
                /* Signal main application to continue execution */
                xEventGroupClearBits(s_wifi_event_group, WIFI_PROV_STOP_BIT);
                xEventGroupSetBits(s_wifi_event_group, WIFI_PROV_START_BIT);
                break;

            case WIFI_PROV_CRED_RECV:
            {
                wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
                ESP_LOGI(TAG,
                    "Received Wi-Fi credentials"
                    "\n\tSSID     : %s\n\tPassword : %s",
                    (const char *)wifi_sta_cfg->ssid, (const char *)wifi_sta_cfg->password);
                break;
            }

            case WIFI_PROV_CRED_FAIL:
            {
                wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
                ESP_LOGE(TAG,
                    "Provisioning failed!\n\tReason : %s"
                    "\n\tPlease reset to factory and retry provisioning",
                    (*reason == WIFI_PROV_STA_AUTH_ERROR) ? "Wi-Fi station authentication failed"
                                                          : "Wi-Fi access-point not found");

                int retries = 0;
                if (retries >= CONFIG_WIFIM_PROV_MGR_MAX_RETRY_CNT)
                {
                    ESP_LOGI(TAG, "Failed to connect with provisioned AP, resetting provisioned credentials");
                    wifi_prov_mgr_reset_sm_state_on_failure();
                    retries = 0;
                }
                break;
            }

            case WIFI_PROV_CRED_SUCCESS:
                ESP_LOGI(TAG, "Provisioning successful");
                break;

            case WIFI_PROV_END:
                /* De-initialize manager once provisioning is finished */
                wifi_prov_mgr_deinit();
                xEventGroupClearBits(s_wifi_event_group, WIFI_STARTED_BIT);
                xEventGroupSetBits(s_wifi_event_group, WIFI_STOPPED_BIT);
                break;

            default:
                break;
        }
    }
    else if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
                /* Signal main application to continue execution */
                xEventGroupClearBits(s_wifi_event_group, WIFI_STOPPED_BIT);
                xEventGroupSetBits(s_wifi_event_group, WIFI_STARTED_BIT);
                break;

            case WIFI_EVENT_STA_STOP:
                ESP_LOGI(TAG, "WIFI_EVENT_STA_STOP");
                /* Signal main application to continue execution */
                xEventGroupClearBits(s_wifi_event_group, WIFI_STARTED_BIT);
                xEventGroupSetBits(s_wifi_event_group, WIFI_STOPPED_BIT);
                break;

            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "WIFI_EVENT_STA_CONNECTED");
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
            {
                wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
                ESP_LOGE(TAG,
                    "Provisioning failed!\n\tReason : %s"
                    "\n\tPlease reset to factory and retry provisioning",
                    (*reason == WIFI_PROV_STA_AUTH_ERROR) ? "Wi-Fi station authentication failed"
                                                          : "Wi-Fi access-point not found");

                /* Signal main application to continue execution */
                xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                xEventGroupSetBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT);

                /* Set code corresponding to the reason for disconnection */
                switch (*reason)
                {
                    // TODO: Restart provisioning after some time
                    case WIFI_PROV_STA_AUTH_ERROR:
                        break;

                    case WIFI_PROV_STA_AP_NOT_FOUND:
                        break;

                    default:
                        break;
                }
                break;
            }

            case WIFI_EVENT_STA_AUTHMODE_CHANGE:
                ESP_LOGI(TAG, "WIFI_EVENT_STA_AUTHMODE_CHANGE");
                /* Signal main application to continue execution */
                xEventGroupSetBits(s_wifi_event_group, AUTH_CHANGE_BIT);
                break;

            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "WIFI_EVENT_AP_START");
                wifi_ap_state = true;
                /* Signal main application to continue execution */
                xEventGroupClearBits(s_wifi_event_group, AP_STOPPED_BIT);
                xEventGroupSetBits(s_wifi_event_group, AP_STARTED_BIT);
                break;

            case WIFI_EVENT_AP_STOP:
                ESP_LOGI(TAG, "WIFI_EVENT_AP_STOP");
                wifi_ap_state = false;
                /* Signal main application to continue execution */
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
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        switch (event_id)
        {
            case IP_EVENT_STA_GOT_IP:
                ESP_LOGI(TAG, "Connected with IP Address:" IPSTR, IP2STR(&event->ip_info.ip));
                wifi_conn_state = true;
                /* Signal main application to continue execution */
                xEventGroupClearBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT);
                xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
                break;

            default:
                break;
        }
    }
}

#ifdef WIFIM_PROV_SHOW_QR
static void wifi_prov_print_qr(const char *name, const char *pop, const char *transport)
{
    if (!name || !transport)
    {
        ESP_LOGW(TAG, "Cannot generate QR code payload. Data missing.");
        return;
    }
    char payload[150] = { 0 };
    if (pop)
    {
        snprintf(payload, sizeof(payload),
            "{\"ver\":\"%s\",\"name\":\"%s\""
            ",\"pop\":\"%s\",\"transport\":\"%s\"}",
            PROV_QR_VERSION, name, pop, transport);
    }
    else
    {
        snprintf(payload, sizeof(payload),
            "{\"ver\":\"%s\",\"name\":\"%s\""
            ",\"transport\":\"%s\"}",
            PROV_QR_VERSION, name, transport);
    }
    ESP_LOGI(TAG, "If QR code is not visible, copy paste the below URL in a browser.\n%s?data=%s", QRCODE_BASE_URL,
        payload);
    ESP_LOGI(TAG, "Scan this QR code from the provisioning application for Provisioning.");
    esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
    esp_qrcode_generate(&cfg, payload);
}
#endif /* WIFIM_PROV_SHOW_QR */

static void get_device_service_name(char *service_name, size_t max)
{
    uint8_t eth_mac[6];
    const char *ssid_prefix = "PROV_";
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    snprintf(service_name, max, "%s%02X%02X%02X", ssid_prefix, eth_mac[3], eth_mac[4], eth_mac[5]);
}

/* Handler for the optional provisioning endpoint registered by the application.
 * The data format can be chosen by applications. Here, we are using plain ascii text.
 * Applications can choose to use other formats like protobuf, JSON, XML, etc.
 */
static esp_err_t custom_prov_data_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen, uint8_t **outbuf,
    ssize_t *outlen, void *priv_data)
{
    if (inbuf)
    {
        ESP_LOGI(TAG, "Received data: %.*s", inlen, (char *)inbuf);
    }
    char response[] = "SUCCESS";
    *outbuf = (uint8_t *)strdup(response);
    if (*outbuf == NULL)
    {
        ESP_LOGE(TAG, "System out of memory");
        return ESP_ERR_NO_MEM;
    }
    *outlen = strlen(response) + 1; /* +1 for NULL terminating byte */

    return ESP_OK;
}

esp_err_t wifi_disconnect_wifi(void)
{
    if (xSemaphoreTake(s_wifi_sem, sem_wait_ticks) != pdPASS)
    {
        return ESP_ERR_TIMEOUT;
    }

    esp_err_t ret = ESP_OK;
    wifi_prov_sta_state_t state;
    wifi_prov_mgr_get_wifi_state(&state);

    if (wifi_conn_state || state == WIFI_PROV_STA_CONNECTED)
    {
        ret = esp_wifi_disconnect();
        if (ret == ESP_ERR_WIFI_NOT_INIT || ret == ESP_ERR_WIFI_NOT_STARTED)
        {
            // Release the semaphore before returning
            xSemaphoreGive(s_wifi_sem);
            return ESP_OK;
        }
        else if (ret == ESP_OK)
        {
            xEventGroupWaitBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
        }
    }

    // Release the semaphore before returning
    xSemaphoreGive(s_wifi_sem);
    return ret;
}

esp_err_t wifim_reconnect_wifi(void)
{
    if (xSemaphoreTake(s_wifi_sem, sem_wait_ticks) != pdPASS)
    {
        return ESP_ERR_TIMEOUT;
    }
    /* Start Wi-Fi in station mode */
    esp_err_t ret = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    ESP_ERROR_GOTO(ret != ESP_OK, EXIT, "esp_wifi_set_storage <%s>", esp_err_to_name(ret));

    ret = esp_wifi_start();
    ESP_ERROR_GOTO(ret != ESP_OK, EXIT, "esp_wifi_start <%s>", esp_err_to_name(ret));

    xEventGroupWaitBits(s_wifi_event_group, WIFI_STARTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

    ret = esp_wifi_connect();
    ESP_ERROR_GOTO(ret != ESP_OK, EXIT, "esp_wifi_connect <%s>", esp_err_to_name(ret));

    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

EXIT:
    xSemaphoreGive(s_wifi_sem);
    return ret;
}

esp_err_t wifim_reset(void)
{
    if (xSemaphoreTake(s_wifi_sem, sem_wait_ticks) != pdPASS)
    {
        return ESP_ERR_TIMEOUT;
    }
    esp_err_t ret = ESP_OK;
    bool provisioned;
    /* This checks if Wi-Fi credentials are present on the NVS */
    ret = wifi_prov_mgr_is_provisioned(&provisioned);
    ESP_ERROR_GOTO(ret != ESP_OK, EXIT, "wifi_prov_mgr_is_provisioned <%s>", esp_err_to_name(ret));

    if (provisioned)
    {
        ret = wifi_prov_mgr_reset_provisioning();
        ESP_ERROR_GOTO(ret != ESP_OK, EXIT, "wifi_prov_mgr_reset_provisioning <%s>", esp_err_to_name(ret));

        ret = wifi_prov_mgr_reset_sm_state_on_failure();
        ESP_ERROR_GOTO(ret != ESP_OK, EXIT, "wifi_prov_mgr_reset_sm_state_on_failure <%s>", esp_err_to_name(ret));
    }
EXIT:
    xSemaphoreGive(s_wifi_sem);
    return ret;
}
// esp_err_t wifim_restart_reprovisioning(void)
// {
//     esp_err_t ret = wifi_prov_mgr_reset_sm_state_on_failure();
//     ESP_ERROR_RETURN(ret != ESP_OK, ret, "wifi_prov_mgr_reset_sm_state_on_failure");
//     ret = wifim_init();
//     ESP_ERROR_RETURN(ret != ESP_OK, ret, "wifim_init");
//     ret = wifim_start_provisioning();
//     ESP_ERROR_RETURN(ret != ESP_OK, ret, "wifim_start_provisioning");
//     return ret;
// }

esp_err_t wifim_start_provisioning(void)
{
    if (xSemaphoreTake(s_wifi_sem, sem_wait_ticks) != pdPASS)
    {
        return ESP_ERR_TIMEOUT;
    }

    /* Let's find out if the device is provisioned */
    bool provisioned = false;
    esp_err_t ret = wifi_prov_mgr_is_provisioned(&provisioned);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "wifi_prov_mgr_is_provisioned failed: %s", esp_err_to_name(ret));
        xSemaphoreGive(s_wifi_sem);
        return ret;
    }

    /* If device is not yet provisioned start provisioning service */
    if (!provisioned)
    {
        ESP_LOGI(TAG, "Starting provisioning");

        /* What is the Device Service Name that we want
         * This translates to :
         *     - Wi-Fi SSID when scheme is wifi_prov_scheme_softap
         *     - device name when scheme is wifi_prov_scheme_ble
         */
        char service_name[12];
        get_device_service_name(service_name, sizeof(service_name));

        /* What is the security level that we want (0 or 1):
         *      - WIFI_PROV_SECURITY_0 is simply plain text communication.
         *      - WIFI_PROV_SECURITY_1 is secure communication which consists of secure handshake
         *          using X25519 key exchange and proof of possession (pop) and AES-CTR
         *          for encryption/decryption of messages.
         */
        wifi_prov_security_t security = WIFI_PROV_SECURITY_1;

        /* Do we want a proof-of-possession (ignored if Security 0 is selected):
         *      - this should be a string with length > 0
         *      - NULL if not used
         */
        const char *pop = "abcd1234";

        /* What is the service key (could be NULL)
         * This translates to :
         *     - Wi-Fi password when scheme is wifi_prov_scheme_softap
         *     - simply ignored when scheme is wifi_prov_scheme_ble
         */
        const char *service_key = NULL;
#ifdef CONFIG_WIFIM_PROV_TRANSPORT_BLE
        /* This step is only useful when scheme is wifi_prov_scheme_ble. This will
         * set a custom 128 bit UUID which will be included in the BLE advertisement
         * and will correspond to the primary GATT service that provides provisioning
         * endpoints as GATT characteristics. Each GATT characteristic will be
         * formed using the primary service UUID as base, with different auto assigned
         * 12th and 13th bytes (assume counting starts from 0th byte). The client side
         * applications must identify the endpoints by reading the User Characteristic
         * Description descriptor (0x2901) for each characteristic, which contains the
         * endpoint name of the characteristic */
        uint8_t custom_service_uuid[] = {
            /* LSB <---------------------------------------
             * ---------------------------------------> MSB */
            0xb4,
            0xdf,
            0x5a,
            0x1c,
            0x3f,
            0x6b,
            0xf4,
            0xbf,
            0xea,
            0x4a,
            0x82,
            0x03,
            0x04,
            0x90,
            0x1a,
            0x02,
        };

        /* If your build fails with linker errors at this point, then you may have
         * forgotten to enable the BT stack or BTDM BLE settings in the SDK (e.g. see
         * the sdkconfig.defaults in the example project) */
        wifi_prov_scheme_ble_set_service_uuid(custom_service_uuid);
#endif /* CONFIG_WIFIM_PROV_TRANSPORT_BLE */
        /* An optional endpoint that applications can create if they expect to
         * get some additional custom data during provisioning workflow.
         * The endpoint name can be anything of your choice.
         * This call must be made before starting the provisioning.
         */
        wifi_prov_mgr_endpoint_create("custom-data");
        /* Start provisioning service */
        ret = wifi_prov_mgr_start_provisioning(security, pop, service_name, service_key);
        ESP_ERROR_GOTO(ret != ESP_OK, EXIT, "wifi_prov_mgr_start_provisioning <%s>", esp_err_to_name(ret));

        xEventGroupWaitBits(s_wifi_event_group, WIFI_PROV_START_BIT, pdTRUE, pdFALSE, portMAX_DELAY);

        /* The handler for the optional endpoint created above.
         * This call must be made after starting the provisioning, and only if the endpoint
         * has already been created above.
         */
        wifi_prov_mgr_endpoint_register("custom-data", custom_prov_data_handler, NULL);

#ifdef WIFIM_PROV_SHOW_QR
        wifi_prov_print_qr(service_name, pop, PROV_TRANSPORT_BLE);
/* Print QR code for provisioning */
#ifdef CONFIG_WIFIM_PROV_TRANSPORT_SOFTAP
        wifi_prov_print_qr(service_name, pop, PROV_TRANSPORT_SOFTAP);
#endif /* CONFIG_WIFIM_PROV_TRANSPORT_SOFTAP*/
#endif /* CONFIG_WIFIM_PROV_TRANSPORT_BLE */
    }
    else
    {
        /* Start Wi-Fi in station mode */
        esp_err_t ret = esp_wifi_set_storage(WIFI_STORAGE_RAM);
        ESP_ERROR_GOTO(ret != ESP_OK, EXIT, "esp_wifi_set_storage <%s>", esp_err_to_name(ret));

        ret = esp_wifi_start();
        ESP_ERROR_GOTO(ret != ESP_OK, EXIT, "esp_wifi_start <%s>", esp_err_to_name(ret));

        xEventGroupWaitBits(s_wifi_event_group, WIFI_STARTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);

        ret = esp_wifi_connect();
        ESP_ERROR_GOTO(ret != ESP_OK, EXIT, "esp_wifi_connect <%s>", esp_err_to_name(ret));

        xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    }

EXIT:
    xSemaphoreGive(s_wifi_sem);
    return ret;
}

esp_err_t wifi_prov_deinit(void)
{
    if (xSemaphoreTake(s_wifi_sem, sem_wait_ticks) != pdPASS)
    {
        return ESP_ERR_TIMEOUT;
    }

    bool provisioned = false;
    wifi_prov_mgr_is_provisioned(&provisioned);

    if (provisioned || wifi_prov_started)
    {
        wifi_prov_mgr_deinit();
    }

    wifi_prov_sta_state_t state;
    esp_err_t ret = wifi_prov_mgr_get_wifi_state(&state);

    if (wifi_conn_state || state == WIFI_PROV_STA_CONNECTED)
    {
        ret = esp_wifi_disconnect();
        if (ret == ESP_ERR_WIFI_NOT_INIT || ret == ESP_ERR_WIFI_NOT_STARTED)
        {
            ret = ESP_OK;
        }
        else if (ret == ESP_OK)
        {
            xEventGroupWaitBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
        }
    }

    if (s_wifi_event_group)
    {
        vEventGroupDelete(s_wifi_event_group);
        s_wifi_event_group = NULL;
    }

    xSemaphoreGive(s_wifi_sem);
    vSemaphoreDelete(s_wifi_sem);
    s_wifi_sem = NULL;
    return ESP_OK;
}

esp_err_t wifim_prov_init(void)
{
    static bool event_loop_inited = false;
    esp_err_t ret = ESP_FAIL;

    if (!event_loop_inited)
    {
        ret = esp_netif_init();
        ESP_ERROR_RETURN(ret != ESP_OK, ret, "esp_netif_init");

        ret = esp_event_loop_create_default();
        ESP_ERROR_RETURN(ret != ESP_OK, ret, "esp_event_loop_create_default");

        /* Initialize Wi-Fi including netif with default config */
        esp_netif_info = esp_netif_create_default_wifi_sta();
        esp_softap_netif_info = esp_netif_create_default_wifi_ap();
#ifdef CONFIG_WIFIM_PROV_TRANSPORT_SOFTAP

#endif /* CONFIG_WIFIM_PROV_TRANSPORT_SOFTAP */

        if (esp_netif_info == NULL || esp_softap_netif_info == NULL)
        {
            ESP_LOGE(TAG, "%s: Failed to create default network interfaces", __func__);
            return ESP_FAIL;
        }

        /* Register our event handler for Wi-Fi, IP and Provisioning related events */
        esp_event_handler_instance_t instance_prov_id;
        esp_event_handler_instance_t instance_wifi_id;
        esp_event_handler_instance_t instance_got_ip;

        ret = esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &event_handler, &instance_prov_id);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "%s: Failed to register event handler for WIFI_PROV_EVENT", __func__);
            return ret;
        }

        ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, &instance_wifi_id);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "%s: Failed to register event handler for WIFI_EVENT", __func__);
            return ret;
        }
        ret = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler, &instance_got_ip);
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

    /* Configuration for the provisioning manager */
    wifi_prov_mgr_config_t config = {
    /* What is the Provisioning Scheme that we want ?
     * wifi_prov_scheme_softap or wifi_prov_scheme_ble */
#ifdef CONFIG_WIFIM_PROV_TRANSPORT_BLE
        .scheme = wifi_prov_scheme_ble,
#endif /* CONFIG_WIFIM_PROV_TRANSPORT_BLE */
#ifdef CONFIG_WIFIM_PROV_TRANSPORT_SOFTAP
        .scheme = wifi_prov_scheme_softap,
#endif /* CONFIG_WIFIM_PROV_TRANSPORT_SOFTAP */

       /* Any default scheme specific event handler that you would
        * like to choose. Since our example application requires
        * neither BT nor BLE, we can choose to release the associated
        * memory once provisioning is complete, or not needed
        * (in case when device is already provisioned). Choosing
        * appropriate scheme specific event handler allows the manager
        * to take care of this automatically. This can be set to
        * WIFI_PROV_EVENT_HANDLER_NONE when using wifi_prov_scheme_softap*/
#ifdef CONFIG_WIFIM_PROV_TRANSPORT_BLE
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
#endif /* CONFIG_WIFIM_PROV_TRANSPORT_BLE */
#ifdef CONFIG_WIFIM_PROV_TRANSPORT_SOFTAP
                                    .scheme_event_handler
        = WIFI_PROV_EVENT_HANDLER_NONE
#endif /* CONFIG_WIFIM_PROV_TRANSPORT_SOFTAP */
    };

    /* Initialize provisioning manager with the
     * configuration parameters set above */
    ret = wifi_prov_mgr_init(config);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "%s: Failed to initialize wifi_prov_mgr_init (%s)", __func__, esp_err_to_name(ret));
        return ret;
    }

    /* If device is not yet provisioned start provisioning service */
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