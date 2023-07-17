
/*
STEPS AFTER START UP.
1. Check Device register or not.( any method retriving stats from NVM. )
    REGISTER ?
    NO:
    1. Register button and interrrupt callback and wait until button pressed.
    BUTTON PRESSED:
        1. Initiate espnow security connectin from espnow using default
certificate or any company secret key
        2. After security process, beta/gamma will receive Symmetric key app key
which is used for further data exchange. and also the nickname which is
registeration number of beta/gamma in between now ( 0-15) in future (0-255).
        3. Saved Symmetric key and nickname in NVM .

    YES:
    1. Retrive symmetric key and nickname from NVM.
    2. Initialize Sensor interrupt callback
    3. Send notification alpha that beta is online.
    4. Goto in responder mode.

2. If any interrupt happen from sensor
    1. Get sensor data.
    2. Prepare message
    3. Stop reponder mode if it is in.
    4. Initiate secure connection using espnow initiator
    5. Send data and wait for acknowledgement or send until receive
acknowledgement

/--------------------------------------------------------------------------------/
*/
/* freertos headers */
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

/* esp-idf headers */
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
#include "esp_mac.h"
#include "esp_random.h"
#endif
#include "cJSON.h"
#include "esp_wifi.h"

/* espnow headers */
#include "espnow.h"
#include "espnow_security.h"
#include "espnow_security_handshake.h"
#include "espnow_storage.h"
#include "espnow_utils.h"

/* iot-button headers */
#include "iot_button.h"

/* backoff algorithm header.*/
#include "backoff_algorithm.h"

/* vscp headers */
#include "vscp.h"
#include "vscp_led.h"

static const char *TAG = "beta.c";

/* The maximum number of retries for connecting to server.*/
#define CONNECTION_RETRY_MAX_ATTEMPTS (5U)

/* The maximum back-off delay (in milliseconds) for retrying connection to
 * server.*/
#define CONNECTION_RETRY_MAX_BACKOFF_DELAY_MS (5000U)

/* The base back-off delay (in milliseconds) to use for connection retry
 * attempts.*/
#define CONNECTION_RETRY_BACKOFF_BASE_MS (500U)

/* threshold to reset the beta */
#define BUTTON_LONG_PRESS_TIME_MS CONFIG_BUTTON_LONG_PRESS_TIME_MS

/* threshold to register the beta*/
#define BUTTON_SHORT_PRESS_TIME_MS CONFIG_BUTTON_SHORT_PRESS_TIME_MS

/* vscp receiver queue. */
#define CONFIG_VSCP_RECV_QUEUE_SIZE (10)

#define VSCP_LED_PIN_NO                    (08)
#define VSCP_QUEUE_MAX_WAIT_TIMEOUT_MS     (pdTICKS_TO_MS(5000))
#define VSCP_SEMAPHORE_MAX_WAIT_TIMEOUT_MS (pdTICKS_TO_MS(5000))

/**
 *
 */
static const char *ENROL_STATUS_NVS = "enroll_status"; //
static const char *STORAGE_NVS = "storage";            //
static const char *ALPHA_ADDR_NVS = "alpha_addr";      //
static const char *NICKNAME_NVS = "nickname";

typedef struct {
    espnow_addr_t addr;
    size_t size;
    vscp_data_t *data;
} espnow_data_receiver_t;

/* alpha node guid/addr */
static uint8_t global_alpha_addr[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

// static espnow_data_receiver_t g_vscp_receiver = { 0 };
static QueueHandle_t g_vscp_queue = NULL;
static SemaphoreHandle_t g_vscp_sem = NULL;

/* Set the system time to the received timestamp */
static struct timeval timestamp_tv;

static BackoffAlgorithmContext_t g_backoff_reconnect_params;

extern esp_err_t espnow_security_inteface_init(handler_for_data_t handle);
extern esp_err_t espnow_security_inteface_initiator_connect(espnow_addr_t addr, size_t addr_num, uint8_t *app_key);

static esp_err_t vscp_establish_secure_connection_with_backoff_delay(void);

esp_err_t vscp_send_data(const void *data, size_t size);

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
 * @brief Register a new VSCP node.
 *
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t vscp_register_new_node(uint8_t *src_addr, void *data, size_t size)
{
    /* established secure connection */
    esp_err_t ret = vscp_establish_secure_connection_with_backoff_delay();
    ESP_ERROR_RETURN(ret != ESP_OK, ret, "<%s> vscp_establish_secure_connection_with_backoff_delay",
        esp_err_to_name(ret));

    vscp_data_t vscp_data = { 0 };
    size_t total_size = 0;

    // prepare registration message
    (void)helper_prepare_vscp_nodes_message((void *)&vscp_data, VSCP_MSG_TYPE_REQUEST, VSCP_PRIORITY_NORMAL,
        VSCP_CLASS1_PROTOCOL, VSCP_TYPE_PROTOCOL_NEW_NODE_ONLINE, NULL, &total_size);

    ret = vscp_send_data((void *)&vscp_data, total_size);
    ESP_ERROR_RETURN(ret != ESP_OK, ret, "<%s> vscp_send_data", esp_err_to_name(ret));

    return ret;
}

/**
 * @brief Event handler for accepting the nickname.
 *
 * @return ESP_OK on success, error code otherwise.
 */
static esp_err_t vscp_nickname_accept_evt_cb(uint8_t *src_addr, void *data, size_t size)
{
    vscp_data_t vscp_data = { 0 };
    size_t total_size = 0;
    // prepare registration message
    (void)helper_prepare_vscp_nodes_message((void *)&vscp_data, VSCP_MSG_TYPE_RESPONSE, VSCP_PRIORITY_NORMAL,
        VSCP_CLASS1_PROTOCOL, VSCP_TYPE_PROTOCOL_NICKNAME_ACCEPTED, NULL, &total_size);

    esp_err_t ret = vscp_send_data((void *)&vscp_data, total_size);
    ESP_ERROR_RETURN(ret != ESP_OK, ret, "<%s> vscp_send_data", esp_err_to_name(ret));

    return ret;
}

/**
 * @brief Event handler for setting the nickname.
 *
 * @param src_addr Source address.
 * @param data Data.
 * @param size Size of the data.
 * @return ESP_OK on success, error code otherwise.
 */
static esp_err_t vscp_set_nickname_evt_cb(uint8_t *src_addr, void *data, size_t size)
{
    ESP_PARAM_CHECK(src_addr);
    ESP_PARAM_CHECK(data);
    ESP_PARAM_CHECK(size);
    esp_err_t ret = ESP_OK;
    vscp_data_t *vscp_data = (vscp_data_t *)data;
    const char *str_data = (const char *)vscp_data->pdata;

    cJSON *pJson = cJSON_Parse(str_data);
    ESP_ALLOC_CHECK(pJson);

    cJSON *nameObj = cJSON_GetObjectItemCaseSensitive(pJson, "nickname");
    if (cJSON_IsNumber(nameObj)) {
        global_self_nickname = (uint8_t)cJSON_GetNumberValue(nameObj);
        cJSON_Delete(pJson);

        ESP_LOGI(TAG, "got nickname: %u", global_self_nickname);

        nvs_handle_t nvs_handle;
        esp_err_t ret = nvs_open(STORAGE_NVS, NVS_READWRITE, &nvs_handle);
        if (ret != ESP_OK) {
            return ret;
        }
        ret = nvs_set_u8(nvs_handle, NICKNAME_NVS, global_self_nickname);
        if (ret != ESP_OK) {
            nvs_close(nvs_handle);
            return ret;
        }
    }
    ret = vscp_nickname_accept_evt_cb(src_addr, data, size);
    if (ret != ESP_OK) {
        return ret;
    }
    return ESP_OK;
}

/**
 * @brief Event handler for acknowledging enrollment.
 *
 * @return ESP_OK on success, error code otherwise.
 */
static esp_err_t vscp_enroll_ack_evt_cb(uint8_t *src_addr, void *data, size_t size)
{
    vscp_data_t vscp_data = { 0 };
    size_t total_size = 0;
    // prepare registration message
    (void)helper_prepare_vscp_nodes_message((void *)&vscp_data, VSCP_MSG_TYPE_RESPONSE, VSCP_PRIORITY_NORMAL,
        VSCP_CLASS1_PROTOCOL, VSCP_TYPE_PROTOCOL_ENROLL_ACK, NULL, &total_size);

    esp_err_t ret = vscp_send_data((void *)&vscp_data, total_size);
    ESP_ERROR_RETURN(ret != ESP_OK, ret, "<%s> vscp_send_data", esp_err_to_name(ret));

    return ret;
}

/**
 * @brief Event handler for enrolling.
 *
 * @param src_addr Source address.
 * @param data Data.
 * @param size Size of the data.
 * @return ESP_OK on success, error code otherwise.
 */
static esp_err_t vscp_enroll_evt_cb(uint8_t *src_addr, void *data, size_t size)
{
    ESP_PARAM_CHECK(src_addr);
    ESP_PARAM_CHECK(data);
    ESP_PARAM_CHECK(size);

    esp_err_t ret;
    vscp_data_t *vscp = (vscp_data_t *)data;

    if (memcmp(vscp->guid, global_alpha_addr, sizeof(vscp->guid)) == 0) {
        nvs_handle_t nvs_handle;
        esp_err_t ret = nvs_open(STORAGE_NVS, NVS_READONLY, &nvs_handle);

        if (ret != ESP_OK) {
            return ret;
        }

        ret = nvs_set_u8(nvs_handle, ENROL_STATUS_NVS, 1);
        nvs_close(nvs_handle);
        if (ret != ESP_OK) {
            return ret;
        }
    }

    ret = vscp_enroll_ack_evt_cb(src_addr, (void *)vscp, size);
    ESP_ERROR_RETURN(ret != ESP_OK, ret, "vscp_enroll_ack_evt_cb <%s>", esp_err_to_name(ret));

    return ESP_OK;
}

/**
 * @brief Event handler for general measurement events.
 *
 * @param src_addr Source address.
 * @param data Data.
 * @param size Size of the data.
 * @return ESP_OK on success, error code otherwise.
 */
static esp_err_t vscp_general_meas_evt_cb(uint8_t *src_addr, void *data, size_t size)
{
    ESP_PARAM_CHECK(src_addr);
    ESP_PARAM_CHECK(data);
    ESP_PARAM_CHECK(size);

    cJSON *pJson = cJSON_CreateObject();
    ESP_ALLOC_CHECK(pJson);

    int8_t temp = (int8_t)(rand() % 50);
    cJSON_AddNumberToObject(pJson, "temp", (double)temp);
    const char *json_str = cJSON_PrintUnformatted(pJson);
    if (!json_str) {
        return ESP_ERR_NO_MEM;
    }
    vscp_data_t vscp_data = { 0 };
    size_t total_size = 0;
    // prepare registration message
    (void)helper_prepare_vscp_nodes_message((void *)&vscp_data, VSCP_MSG_TYPE_RESPONSE, VSCP_PRIORITY_NORMAL,
        VSCP_CLASS1_MEASUREMENT, VSCP_TYPE_MEASUREMENT_TEMPERATURE, json_str, &total_size);

    esp_err_t ret = vscp_send_data(&vscp_data, total_size);

    cJSON_Delete(pJson);

    return ret;
}

static esp_err_t vscp_nickname_dropped_evnt_cb(uint8_t *src_addr, void *data, size_t size)
{
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(STORAGE_NVS, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = nvs_erase_key(nvs_handle, NICKNAME_NVS);
    nvs_close(nvs_handle);
    if (ret != ESP_OK) {
        return ret;
    }

    vscp_data_t vscp_data = { 0 };
    size_t total_size = 0;
    // prepare registration message
    (void)helper_prepare_vscp_nodes_message((void *)&vscp_data, VSCP_MSG_TYPE_RESPONSE, VSCP_PRIORITY_NORMAL,
        VSCP_CLASS1_PROTOCOL, VSCP_TYPE_PROTOCOL_DROP_NICKNAME_ACK, NULL, &total_size);

    ret = vscp_send_data((void *)&vscp_data, total_size);
    if (ret != ESP_OK) {
        return ret;
    }

    return ESP_OK;
}

/**
 * @brief Event handler for time synchronization events.
 *
 * @param src_addr Source address.
 * @param data Data.
 * @param size Size of the data.
 * @return ESP_OK on success, ESP_FAIL if the source address is not equal to the
 * expected address.
 */
static esp_err_t vscp_timesync_evt_cb(uint8_t *src_addr, void *data, size_t size)
{
    if (!ESPNOW_ADDR_IS_EQUAL(global_alpha_addr, src_addr)) {
        return ESP_FAIL;
    }
    vscp_data_t *vscp = (vscp_data_t *)data;
    timestamp_tv.tv_sec = vscp->timestamp;
    timestamp_tv.tv_usec = 0;
    settimeofday(&timestamp_tv, NULL);
    return ESP_OK;
}

static esp_err_t vscp_measurement_class_evnt(uint8_t *src_addr, void *data, size_t size)
{
    ESP_PARAM_CHECK(src_addr);
    ESP_PARAM_CHECK(data);
    ESP_PARAM_CHECK(size != 0);

    esp_err_t ret;
    vscp_data_t *vscp = (vscp_data_t *)data;

    switch (vscp->vscp_type) {
        case VSCP_TYPE_MEASUREMENT_GENERAL:
            ret = vscp_general_meas_evt_cb(src_addr, (void *)vscp, size);
            ESP_ERROR_RETURN(ret != ESP_OK, ret, "vscp_general_meas_evt_cb <%s>", esp_err_to_name(ret));
            break;

        default:
            break;
    }
    return ESP_OK;
}

static esp_err_t vscp_control_class_evnt(uint8_t *src_addr, void *data, size_t size)
{
    ESP_PARAM_CHECK(src_addr);
    ESP_PARAM_CHECK(data);
    ESP_PARAM_CHECK(size != 0);

    esp_err_t ret;
    vscp_data_t *vscp = (vscp_data_t *)data;
    switch (vscp->vscp_type) {
        case VSCP_TYPE_CONTROL_TIME_SYNC:
            ret = vscp_timesync_evt_cb(src_addr, (void *)vscp, size);
            ESP_ERROR_RETURN(ret != ESP_OK, ret, "vscp_timesync_evt_cb <%s>", esp_err_to_name(ret));
            break;

        default:
            break;
    }
    return ESP_OK;
}

static esp_err_t vscp_protocol_class_evnt(uint8_t *src_addr, void *data, size_t size)
{
    ESP_PARAM_CHECK(src_addr);
    ESP_PARAM_CHECK(data);
    ESP_PARAM_CHECK(size != 0);

    esp_err_t ret;

    vscp_data_t *vscp = (vscp_data_t *)data;
    switch (vscp->vscp_type) {
        /*case VSCP_TYPE_PROTOCOL_NEW_NODE_ONLINE:
            ret = vscp_register_new_node(src_addr, vscp, size);
            ESP_ERROR_RETURN(ret != ESP_OK, ret, "vscp_register_new_node <%s>", esp_err_to_name(ret));
            break;
        */
        case VSCP_TYPE_PROTOCOL_NICKNAME_ACCEPTED:
        case VSCP_TYPE_PROTOCOL_SET_NICKNAME:
            ret = vscp_set_nickname_evt_cb(src_addr, vscp, size);
            ESP_ERROR_RETURN(ret != ESP_OK, ret, "vscp_set_nickname_evt_cb <%s>", esp_err_to_name(ret));
            break;

        case VSCP_TYPE_PROTOCOL_ENROLL_ACK:
        case VSCP_TYPE_PROTOCOL_ENROLL:
            ret = vscp_enroll_evt_cb(src_addr, vscp, size);
            ESP_ERROR_RETURN(ret != ESP_OK, ret, "vscp_enroll_evt_cb <%s>", esp_err_to_name(ret));
            break;

        case VSCP_TYPE_PROTOCOL_DROP_NICKNAME_ACK:
        case VSCP_TYPE_PROTOCOL_DROP_NICKNAME:
            ret = vscp_nickname_dropped_evnt_cb(src_addr, vscp, size);
            ESP_ERROR_RETURN(ret != ESP_OK, ret, "vscp_nickname_dropped_evnt_cb <%s>", esp_err_to_name(ret));
            break;

        default:
            break;
    }
    return ESP_OK;
}

static void vscp_recv_task(void *arg)
{
    esp_err_t ret;
    g_vscp_queue = xQueueCreate(CONFIG_VSCP_RECV_QUEUE_SIZE, sizeof(espnow_data_receiver_t));
    ESP_ERROR_GOTO(!g_vscp_queue, EXIT, "failed to create vscp receive queue.");

    for (;;) {
        espnow_data_receiver_t espnow_recv = { 0 };
        vscp_event_handler_t evt_handler = NULL;

        if (xQueueReceive(g_vscp_queue, &espnow_recv, portMAX_DELAY)) {
            vscp_data_t *data = (vscp_data_t *)espnow_recv.data;
            /* first verify crc */
            ret = helper_verify_crc((const vscp_data_t *)data);
            ESP_ERROR_GOTO(ret != ESP_OK, OUTLOOP, "helper_verify_crc <%s>", esp_err_to_name(ret));

            /* get event callback */
            evt_handler = vscp_evnt_handler_get_class_handler(data->vscp_class);
            ESP_ERROR_GOTO(!evt_handler, OUTLOOP, "event handler not register for class %d", data->vscp_class);

            /* call event handler */
            ret = evt_handler((uint8_t *)&espnow_recv.addr, (void *)espnow_recv.data, espnow_recv.size);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "evt_handler of class<%d> type<%d>, <%s>", data->vscp_class, data->vscp_type,
                    esp_err_to_name(ret));
            }
        }

    OUTLOOP:
        xSemaphoreGive(g_vscp_sem);
    }

EXIT:
    if (g_vscp_queue) {
        vQueueDelete(g_vscp_queue);
        g_vscp_queue = NULL;
    }
    vTaskDelete(NULL);
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
    if (xSemaphoreTake(g_vscp_sem, VSCP_SEMAPHORE_MAX_WAIT_TIMEOUT_MS) == pdPASS) {
        espnow_data_receiver_t vscp_recv_data = { 0 };

        memcpy(&vscp_recv_data.addr, src_addr, 6);
        if (!vscp_recv_data.data) {
            return ESP_ERR_NO_MEM; // Memory allocation failed
        }
        if (xQueueSend(g_vscp_queue, (void *)&vscp_recv_data, VSCP_QUEUE_MAX_WAIT_TIMEOUT_MS) != pdFAIL) {
            ret = ESP_OK; // Data successfully sent to the queue
        }
    }

    return ret;
}

esp_err_t vscp_send_data(const void *data, size_t size)
{
    ESP_PARAM_CHECK(data);

    BackoffAlgorithmStatus_t backoffAlgStatus = BackoffAlgorithmSuccess;
    uint16_t nextRetryBackOff;
    uint8_t retryCount = 0;
    esp_err_t ret = ESP_OK;

    uint8_t *espnow_data = ESP_CALLOC(1, ESPNOW_SEC_PACKET_MAX_SIZE);

    if (!espnow_data) {
        return ESP_ERR_NO_MEM;
    }

    espnow_frame_head_t frame_head = { .retransmit_count = 2, .broadcast = true, .security = true, .ack = true };
    memcpy(espnow_data, data, size);

    /* init backoff attempt done */
    g_backoff_reconnect_params.attemptsDone = 0;
    do {
        ret = espnow_send(ESPNOW_DATA_TYPE_DATA, global_alpha_addr, espnow_data, size, &frame_head,
            VSCP_QUEUE_MAX_WAIT_TIMEOUT_MS);
        if (ret != ESP_OK) {
            /* Generate a random number and get back-off value (in milliseconds)
             * for the next connection retry. */
            backoffAlgStatus
                = BackoffAlgorithm_GetNextBackoff(&g_backoff_reconnect_params, (uint32_t)rand(), &nextRetryBackOff);

            if (backoffAlgStatus == BackoffAlgorithmRetriesExhausted) {
                ESP_LOGE(TAG, "espnow_send failed <%s>, all attempts exhausted.", esp_err_to_name(ret));
                ret = ESP_FAIL;
            } else if (backoffAlgStatus == BackoffAlgorithmSuccess) {
                ESP_LOGW(TAG, "espnow_send failed <%s>", esp_err_to_name(ret));
                vTaskDelay(pdMS_TO_TICKS(nextRetryBackOff));

                ESP_LOGI(TAG, "Sending again... <%d/%d> after %hu ms backoff.", ++retryCount,
                    CONNECTION_RETRY_MAX_ATTEMPTS, (unsigned short)nextRetryBackOff);
            }
        }
    } while ((ret != ESP_OK) && (backoffAlgStatus == BackoffAlgorithmSuccess));

    ESP_FREE(espnow_data);
    return ret;
}

static esp_err_t vscp_establish_secure_connection_with_backoff_delay(void)
{
    esp_err_t ret = ESP_OK;
    BackoffAlgorithmStatus_t backoffAlgStatus = BackoffAlgorithmSuccess;
    uint16_t nextRetryBackOff;
    uint8_t retryCount = 0;

    // TODO:
    ret = espnow_security_inteface_init(&vscp_data_receive_cb);
    ESP_ERROR_RETURN(ret != ESP_OK, ret, "<%s> espnow_app_init", esp_err_to_name(ret));

    uint8_t key_info[APP_KEY_LEN];

    if (espnow_get_key(key_info) != ESP_OK) {
        esp_fill_random(key_info, APP_KEY_LEN);
    }
    /* Initialize reconnect attempts and interval */
    g_backoff_reconnect_params.attemptsDone = 0;

    /* Attempt to connect to Alpha. If connection fails, retry after
     * a timeout. Timeout value will exponentially increase until maximum
     * attempts are reached.
     */
    do {
        // TODO:
        ret = espnow_security_inteface_initiator_connect(NULL, 0, (uint8_t *)&key_info); // send broadcast message

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

    return ret;
}

static void beta_get_alpha_addr(void)
{
    /* Retrieved alpha addr */
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(STORAGE_NVS, NVS_READONLY, &nvs_handle);
    if (ret == ESP_OK) {
        uint8_t alpha_mac[6] = { 0xFF };
        size_t size;
        ret = nvs_get_blob(nvs_handle, ALPHA_ADDR_NVS, alpha_mac, &size);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "alpha addr not stored");
        }
        memcpy((void *)&global_alpha_addr, (const void *)&alpha_mac, 6);
    }
    nvs_close(nvs_handle);
}

// buttons
static esp_err_t beta_button_init(void)
{
    // create gpio button
    button_config_t gpio_btn_cfg = {
    .type = BUTTON_TYPE_GPIO,
    .long_press_time = BUTTON_LONG_PRESS_TIME_MS,
    .short_press_time = BUTTON_SHORT_PRESS_TIME_MS,
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
    iot_button_register_cb(gpio_btn, BUTTON_DOUBLE_CLICK, button_double_click_cb, NULL);
    iot_button_register_cb(gpio_btn, BUTTON_LONG_PRESS_HOLD, button_long_pressed_cb, NULL);

    vscp_led_startup_blink();
    return ESP_OK;
}

esp_err_t vscp_init_beta_app(void)
{
    esp_err_t ret = ESP_OK;

    /* get self mac addr */
    esp_wifi_get_mac(WIFI_IF_STA, global_self_guid);

    /* get alpha addr */
    beta_get_alpha_addr();

#ifdef CONFIG_ENABLE_LED_INDICATORS
    ret = vscp_led_init(VSCP_LED_PIN_NO);
    if (ret != ESP_OK) {
        return ret;
    }
#endif /* CONFIG_ENABLE_LED_INDICATORS */

#ifdef CONFIG_ENABLE_BUTTON_SUPPORT
    ret = beta_button_init();
    if (ret != ESP_OK) {
        return ret;
    }
#endif /* CONFIG_ENABLE_BUTTON_SUPPORT */

    /*  event callback */
    vscp_evnt_handler_init();
    vscp_evnt_handler_register_event(VSCP_CLASS1_PROTOCOL, vscp_protocol_class_evnt, VSCP_TYPE_MAX, NULL);
    vscp_evnt_handler_register_event(VSCP_CLASS1_MEASUREMENT, vscp_measurement_class_evnt, VSCP_TYPE_MAX, NULL);
    vscp_evnt_handler_register_event(VSCP_CLASS1_CONTROL, vscp_control_class_evnt, VSCP_TYPE_MAX, NULL);

    /* Initialize reconnect attempts and interval */
    BackoffAlgorithm_InitializeParams(&g_backoff_reconnect_params, CONNECTION_RETRY_BACKOFF_BASE_MS,
        CONNECTION_RETRY_MAX_BACKOFF_DELAY_MS, CONNECTION_RETRY_MAX_ATTEMPTS);

    espnow_set_config_for_data_type(ESPNOW_DATA_TYPE_DATA, true, vscp_data_receive_cb);

    ret = vscp_is_node_register();
    if (ret != ESP_OK) {
        // initiate register process
        ret = vscp_register_new_node(NULL, NULL, 0);
        if (ret != ESP_OK) {
            ESP_LOGI(TAG, "vscp_register_new_node failed <%s>", esp_err_to_name(ret));
            esp_restart();
        }
    }

    xTaskCreate(&vscp_recv_task, "Beta Task", 1024 * 2, NULL, 5, NULL);

    return ret;
}
