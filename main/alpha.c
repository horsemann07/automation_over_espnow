

/* freertos headers */
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

/* esp-idf headers */
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
#include "esp_mac.h"
#include "esp_random.h"
#endif
#include "cJSON.h"
#include "esp_timer.h"
#include "esp_wifi.h"

/* espnow headers */
#include "espnow.h"
#include "espnow_security.h"
#include "espnow_security_handshake.h"
#include "espnow_storage.h"
#include "espnow_utils.h"

/* iot-button headers */
#include "iot_button.h"
#include "wifim.h"

/* backoff algorithm header.*/
#include "backoff_algorithm.h"

/* vscp headers */
#include "vscp.h"

static const char *TAG = "alpha.c";

/**
 * @brief The maximum number of retries for connecting to the server.
 */
#define CONNECTION_RETRY_MAX_ATTEMPTS (5U)

/**
 * @brief The maximum back-off delay (in milliseconds) for retrying connection to the server.
 */
#define CONNECTION_RETRY_MAX_BACKOFF_DELAY_MS (pdTICKS_TO_MS(5000))

/**
 * @brief The base back-off delay (in milliseconds) to use for connection retry attempts.
 */
#define CONNECTION_RETRY_BACKOFF_BASE_MS (500U)

/**
 * @brief The threshold to reset the alpha/beta/gamma.
 */
#define BUTTON_LONG_PRESS_TIME_MS CONFIG_BUTTON_LONG_PRESS_TIME_MS

/**
 * @brief The threshold to register the alpha/beta/gamma.
 */
#define BUTTON_SHORT_PRESS_TIME_MS CONFIG_BUTTON_SHORT_PRESS_TIME_MS

/**
 * @brief The VSCP receiver queue size.
 */
#define CONFIG_VSCP_RECV_QUEUE_SIZE (10)

/**
 * @brief The maximum wait timeout (in milliseconds) for VSCP operations.
 */
#define VSCP_MAX_WAIT_TIMEOUT_MS (pdTICKS_TO_MS(5000))

#define ESP_TIMER_TIMEOUT_MU (30U * 60U * 1000U * 1000U)
/**
 * @brief Structure representing received ESP-NOW data.
 *
 */
typedef struct {
    espnow_addr_t addr;
    size_t size;
    vscp_data_t *data;
} espnow_data_receiver_t;

enum {
    ALPHA_STATE_WIFI_PROV = 1, //
    ALPHA_STATE_MQTT_MNGR,     //
    ALPHA_STATE_ESPNOW,        //
    ALPHA_STATE_MAX
};

/**
 * @brief Backoff algorithm context for reconnection attempts.
 */
static BackoffAlgorithmContext_t g_backoff_reconnect_params;

/**
 * @brief VSCP receiver queue handle.
 */
static QueueHandle_t g_vscp_recv_queue = NULL;

/**
 * @brief Semaphore handle for VSCP synchronization.
 */
static SemaphoreHandle_t g_vscp_recv_sem = NULL;

/**
 * @brief Variable to store the current cluster size.
 */
static uint8_t g_cluster_size = 0;

/**
 * @brief Current state of the alpha node.
 */
static uint8_t g_current_alpha_state = ALPHA_STATE_ESPNOW;

/* Task handle for the alpha task. */
TaskHandle_t alpha_task_handle = NULL;

esp_timer_handle_t esp_periodic_timer;

esp_err_t vscp_send_data(const void *data, size_t size);

/**
 * @brief Event callback function for VSCP_TYPE_PROTOCOL_SET_NICKNAME event.
 *
 * This function is called when a VSCP_TYPE_PROTOCOL_SET_NICKNAME event is received.
 * It handles the event by setting the nickname for the alpha node.
 *
 * @param[in] src_addr Source address of the event.
 * @param[in] data Pointer to the data of the event.
 * @param[in] size Size of the data.
 * @return ESP_OK if the event is handled successfully, otherwise an error code.
 */
static esp_err_t vscp_nickname_set_evt_cb(uint8_t *src_addr, void *data, size_t size)
{
    (void)src_addr;
    (void)data;
    (void)size;

    vscp_data_t vscp_data = { 0 };
    size_t total_size = 0;

    cJSON *pJsonObj = cJSON_CreateObject();
    ESP_ALLOC_CHECK(pJsonObj);

    if (++g_cluster_size == UINT8_MAX) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    cJSON_AddNumberToObject(pJsonObj, "nickname", g_cluster_size);
    const char *pJsonStr = cJSON_PrintUnformatted(pJsonObj);
    ESP_ALLOC_CHECK(pJsonStr);

    // prepare registration message
    (void)helper_prepare_vscp_nodes_message((void *)&vscp_data, VSCP_MSG_TYPE_RESPONSE, VSCP_PRIORITY_NORMAL,
        VSCP_CLASS1_PROTOCOL, VSCP_TYPE_PROTOCOL_ENROLL_ACK, pJsonStr, &total_size);

    esp_err_t ret = vscp_send_data((void *)&vscp_data, total_size);
    ESP_ERROR_RETURN(ret != ESP_OK, ret, "<%s> vscp_send_data", esp_err_to_name(ret));

    return ESP_OK;
}

/**
 * @brief Event callback function for VSCP_TYPE_PROTOCOL_ENROLL event.
 *
 * This function is called when a VSCP_TYPE_PROTOCOL_ENROLL event is received.
 * It handles the event by enrolling the alpha node.
 *
 * @param[in] src_addr Source address of the event.
 * @param[in] data Pointer to the data of the event.
 * @param[in] size Size of the data.
 * @return ESP_OK if the event is handled successfully, otherwise an error code.
 */
static esp_err_t vscp_enroll_evt_cb(uint8_t *src_addr, void *data, size_t size)
{
    (void)src_addr;
    (void)data;
    (void)size;

    vscp_data_t vscp_data = { 0 };
    size_t total_size = 0;

    // prepare registration message
    (void)helper_prepare_vscp_nodes_message((void *)&vscp_data, VSCP_MSG_TYPE_REQUEST, VSCP_PRIORITY_NORMAL,
        VSCP_CLASS1_PROTOCOL, VSCP_TYPE_PROTOCOL_ENROLL, NULL, &total_size);

    esp_err_t ret = vscp_send_data((void *)&vscp_data, total_size);
    ESP_ERROR_RETURN(ret != ESP_OK, ret, "<%s> vscp_send_data", esp_err_to_name(ret));

    return ret;
}

static esp_err_t vscp_nickname_dropped_evnt_cb(uint8_t *src_addr, void *data, size_t size)
{
    (void)src_addr;
    (void)data;
    (void)size;

    vscp_data_t vscp_data = { 0 };
    size_t total_size = 0;

    // prepare registration message
    (void)helper_prepare_vscp_nodes_message((void *)&vscp_data, VSCP_MSG_TYPE_REQUEST, VSCP_PRIORITY_NORMAL,
        VSCP_CLASS1_PROTOCOL, VSCP_TYPE_PROTOCOL_ENROLL, NULL, &total_size);

    esp_err_t ret = vscp_send_data((void *)&vscp_data, total_size);
    ESP_ERROR_RETURN(ret != ESP_OK, ret, "<%s> vscp_send_data", esp_err_to_name(ret));

    return ret;
}

static esp_err_t vscp_protocol_class_evnt(uint8_t *src_addr, void *data, size_t size)
{
    ESP_PARAM_CHECK(src_addr);
    ESP_PARAM_CHECK(data);
    ESP_PARAM_CHECK(size != 0);

    esp_err_t ret;
    vscp_data_t *vscp = (vscp_data_t *)data;
    switch (vscp->vscp_type) {
        case VSCP_TYPE_PROTOCOL_NEW_NODE_ONLINE:
        case VSCP_TYPE_PROTOCOL_SET_NICKNAME:
            ret = vscp_nickname_set_evt_cb(src_addr, (void *)vscp, size);
            ESP_ERROR_RETURN(ret != ESP_OK, ret, "vscp_nickname_set_evt_cb <%s>", esp_err_to_name(ret));
            break;

        case VSCP_TYPE_PROTOCOL_NICKNAME_ACCEPTED:
        case VSCP_TYPE_PROTOCOL_ENROLL:
            ret = vscp_enroll_evt_cb(src_addr, (void *)vscp, size);
            ESP_ERROR_RETURN(ret != ESP_OK, ret, "vscp_enroll_evt_cb <%s>", esp_err_to_name(ret));
            break;

        case VSCP_TYPE_PROTOCOL_DROP_NICKNAME:
            ret = vscp_nickname_dropped_evnt_cb(src_addr, (void *)vscp, size);
            ESP_ERROR_RETURN(ret != ESP_OK, ret, "vscp_nickname_dropped_evnt_cb <%s>", esp_err_to_name(ret));
            break;

        default:
            break;
    }
    return ESP_OK;
}

static void periodic_timer_callback(void *arg)
{
    int64_t time_since_boot = esp_timer_get_time();
    ESP_LOGI(TAG, "Periodic timer called, time since boot: %lld us", time_since_boot);
}

static void button_single_click_cb(void *arg, void *usr_data)
{
    ESP_LOGI(TAG, "BUTTON_SINGLE_CLICK");
}

static void button_double_click_cb(void *arg, void *usr_data)
{
    ESP_LOGI(TAG, "BUTTON_SINGLE_CLICK");
}

static void button_long_pressed_cb(void *arg, void *usr_data)
{
    ESP_LOGI(TAG, "BUTTON_LONG_PRESS_HOLD");
}

static esp_err_t vscp_is_node_register(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(STORAGE_NVS, NVS_READONLY, &nvs_handle);

    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t status = 0;
    ret = nvs_get_u8(nvs_handle, ENROL_STATUS_NVS, &status);

    nvs_close(nvs_handle);

    if (ret == ESP_OK && status == 1) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

/**
 * @brief VSCP data receive callback function.
 *
 * This function is the callback for receiving VSCP data.
 *
 * @param src_addr Pointer to the source MAC address.
 * @param data Pointer to the received data.
 * @param size Size of the received data.
 * @param rx_ctrl Pointer to the received packet control information.
 * @return
 *     - ESP_OK if the data is successfully received.
 *     - ESP_ERR_NO_MEM if memory allocation fails.
 *     - Other error codes if there is an error in processing the received data.
 */
static esp_err_t vscp_data_receive_cb(uint8_t *src_addr, void *data, size_t size, wifi_pkt_rx_ctrl_t *rx_ctrl)
{
    ESP_PARAM_CHECK(src_addr);
    ESP_PARAM_CHECK(data);
    ESP_PARAM_CHECK(size);
    ESP_PARAM_CHECK(rx_ctrl);
    esp_err_t ret = ESP_FAIL;
    /* Wait for previous received message should processed */
    if (xSemaphoreTake(g_vscp_recv_sem, VSCP_MAX_WAIT_TIMEOUT_MS * 4) == pdPASS) {
        espnow_data_receiver_t vscp_recv_data = { 0 };

        memcpy(&vscp_recv_data.addr, src_addr, 6);
        if (!vscp_recv_data.data) {
            return ESP_ERR_NO_MEM; // Memory allocation failed
        }
        if (xQueueSend(g_vscp_recv_queue, (void *)&vscp_recv_data, VSCP_MAX_WAIT_TIMEOUT_MS) != pdFAIL) {
            ret = ESP_OK; // Data successfully sent to the queue
        }
    }
    return ret;
}

static void vscp_recv_task(void *arg)
{
    (void)arg;
    esp_err_t ret;
    g_vscp_recv_queue = xQueueCreate(CONFIG_VSCP_RECV_QUEUE_SIZE, sizeof(espnow_data_receiver_t));
    ESP_ERROR_GOTO(!g_vscp_recv_queue, EXIT, "failed to create vscp receive queue.");

    for (;;) {
        espnow_data_receiver_t espnow_recv = { 0 };
        vscp_event_handler_t evt_handler = NULL;

        if (xQueueReceive(g_vscp_recv_queue, &espnow_recv, portMAX_DELAY)) {
            vscp_data_t *data = (vscp_data_t *)espnow_recv.data;
            /* first verify crc */
            ret = helper_verify_crc((const vscp_data_t *)data);
            ESP_ERROR_GOTO(ret != ESP_OK, OUTLOOP, "helper_verify_crc <%s>", esp_err_to_name(ret));

            /* get event callback */
            evt_handler = vscp_get_event_handler_by_class(data->vscp_class);
            ESP_ERROR_GOTO(!evt_handler, OUTLOOP, "event handler not register for class %d", data->vscp_class);

            /* call event handler */
            if (evt_handler) {
                ret = evt_handler((uint8_t *)&espnow_recv.addr, (void *)espnow_recv.data, espnow_recv.size);
                if (ret != ESP_OK) {
                    ESP_LOGE(TAG, "evt_handler of class<%d> type<%d>, <%s>", data->vscp_class, data->vscp_type,
                        esp_err_to_name(ret));
                }
            }
        }

    OUTLOOP:
        xSemaphoreGive(g_vscp_recv_sem);
    }

EXIT:
    xSemaphoreGive(g_vscp_recv_sem);

    if (g_vscp_recv_queue) {
        vQueueDelete(g_vscp_recv_queue);
        g_vscp_recv_queue = NULL;
    }
    vTaskDelete(NULL);
}

static esp_err_t init_register_button(void)
{
    // create gpio button
    button_config_t gpio_btn_cfg = {
    .type = BUTTON_TYPE_GPIO,
    .long_press_time = CONFIG_BUTTON_LONG_PRESS_TIME_MS,
    .short_press_time = CONFIG_BUTTON_SHORT_PRESS_TIME_MS,
    .gpio_button_config = {
        .gpio_num = 0,
        .active_level = 0,
        },
    };
    button_handle_t gpio_btn = iot_button_create(&gpio_btn_cfg);
    if (NULL == gpio_btn) {
        ESP_LOGE(TAG, "Button create failed");
    }

    iot_button_register_cb(gpio_btn, BUTTON_SINGLE_CLICK, button_single_click_cb, NULL);
    iot_button_register_cb(gpio_btn, BUTTON_LONG_PRESS_HOLD, button_long_pressed_cb, NULL);
    iot_button_register_cb(gpio_btn, BUTTON_DOUBLE_CLICK, button_double_click, NULL);
    // TODO: blink led fast

    return ESP_OK;
}

esp_err_t vscp_alpha_start_responder(void)
{
    esp_err_t ret = ESP_OK;
    BackoffAlgorithmStatus_t backoffAlgStatus = BackoffAlgorithmSuccess;
    uint16_t nextRetryBackOff;
    uint8_t retryCount = 0;

    /* Initialize reconnect attempts and interval */
    g_backoff_reconnect_params.attemptsDone = 0;

    do {
        ret = espnow_security_inteface_responder_start();

        if (ret != ESP_OK) {
            /* Generate a random number and get back-off value (in milliseconds)
             * for the next connection retry. */
            backoffAlgStatus
                = BackoffAlgorithm_GetNextBackoff(&g_backoff_reconnect_params, (uint32_t)rand(), &nextRetryBackOff);

            if (backoffAlgStatus == BackoffAlgorithmRetriesExhausted) {
                ESP_LOGE(TAG, "Connection to the broker failed, all attempts exhausted.");
                ret = ESP_FAIL;
            } else if (backoffAlgStatus == BackoffAlgorithmSuccess) {
                ESP_LOGW(TAG, "Connection to the broker failed.");
                vTaskDelay(pdMS_TO_TICKS(nextRetryBackOff));

                ESP_LOGI(TAG, "Retrying connection <%d/%d> after %hu ms backoff.", ++retryCount,
                    CONNECTION_RETRY_MAX_ATTEMPTS, (unsigned short)nextRetryBackOff);
            }
        }
    } while ((ret != ESP_OK) && (backoffAlgStatus == BackoffAlgorithmSuccess));
}

static esp_err_t alpha_start_mqtt_manager(void)
{
    esp_err_t ret = esp_timer_restart(esp_periodic_timer, ESP_TIMER_TIMEOUT_MU);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "esp_timer_restart <%d:%s>", __LINE__, (ret));
    }

    // TODO:
    return ESP_OK;
}

static esp_err_t vscp_alpha_start_level2_connection(void)
{
    esp_err_t ret = wifim_prov_init();
    if (ret != ESP_OK) {
        // skipped mqtt connection call
        g_current_alpha_state = ALPHA_STATE_ESPNOW;
        return ret;
    }

    ret = wifim_start_provisioning();
    if (ret != ESP_OK) {
        // skipped mqtt connection call
        g_current_alpha_state = ALPHA_STATE_ESPNOW;
        return ret;
    }
    ret = alpha_start_mqtt_manager();
    if (ret != ESP_OK) {
        // failed to connect mqtt manager will try later
        return ret;
    }
    return ESP_OK;
}
static void vscp_alpha_start(void *parameters)
{
    (void)parameters;
    esp_err_t ret = ESP_OK;

    for (;;) {
        switch (g_current_alpha_state) {
            case ALPHA_STATE_WIFI_PROV:
                ret = wifim_prov_init();
                if (ret == ESP_OK) {
                    ret = wifim_start_provisioning();
                    if (ret != ESP_OK) {
                        wifi_prov_deinit();
                        // skipped mqtt connection call
                        g_current_alpha_state = ALPHA_STATE_ESPNOW;
                    } else {
                        g_current_alpha_state = ALPHA_STATE_MQTT_MNGR;
                    }
                } else {
                    // skipped mqtt connection call
                    g_current_alpha_state = ALPHA_STATE_ESPNOW;
                }
                break;

            case ALPHA_STATE_MQTT_MNGR:
                ret = alpha_start_mqtt_manager();
                ESP_ERROR_PRINT(ret != ESP_OK, ret, "alpha_start_mqtt_manager <%s>", esp_err_to_name(ret));
                break;

            case ALPHA_STATE_ESPNOW:
                ret = vscp_alpha_start_responder();
                ESP_ERROR_CONTINUE(ret != ESP_OK, "vscp_alpha_start_responder <%s>", esp_err_to_name(ret));
                break;

            default:
                break;
        }

        // Wait for connection close
        // Check message priority
        // If high priority message present, jump to MQTT manager
        // Else, wait for timer event
    }

    // This code is never reached
    alpha_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t vscp_alpha_init_app(void)
{
    /* Initialize reconnect attempts and interval */
    BackoffAlgorithm_InitializeParams(&g_backoff_reconnect_params, CONNECTION_RETRY_BACKOFF_BASE_MS,
        CONNECTION_RETRY_MAX_BACKOFF_DELAY_MS, CONNECTION_RETRY_MAX_ATTEMPTS);

    /* callback for receive espnow */
    espnow_set_config_for_data_type(ESPNOW_DATA_TYPE_DATA, true, vscp_data_receive_cb);

    esp_err_t ret = vscp_alpha_start_level2_connection();

    if (ret != ESP_OK) {
        g_current_alpha_state = ALPHA_STATE_WIFI_PROV;
    }
    if (!alpha_task_handle) {
        xTaskCreate(&vscp_alpha_start, "alpha", 1024 * 2, NULL, &alpha_task_handle);
    }

    // const esp_timer_create_args_t periodic_timer_args = { .callback = &periodic_timer_callback, .name = "periodic" };

    // /* Start the timers */vscp_alpha_start_level2_connection
    // ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &esp_periodic_timer));
    // ESP_ERROR_CHECK(esp_timer_start_periodic(esp_periodic_timer, ESP_TIMER_TIMEOUT_MU));
    return ESP_OK;
}
