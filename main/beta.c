
/*
STEPS AFTER START UP.
1. Check Device register or not.( any method retriving stats from NVM. )
    REGISTER ?
    NO:
    1. Register button and interrrupt callback and wait until button pressed.
    BUTTON PRESSED:
        1. Initiate espnow security connectin from espnow using default certificate or any company secret key
        2. After security process, beta/gamma will receive Symmetric key app key which is used for further data
exchange. and also the nickname which is registeration number of beta/gamma in between now ( 0-15) in future (0-255).
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
    5. Send data and wait for acknowledgement or send until receive acknowledgement

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

static const char *TAG = "beta.c";

/* The maximum number of retries for connecting to server.*/
#define CONNECTION_RETRY_MAX_ATTEMPTS (5U)

/* The maximum back-off delay (in milliseconds) for retrying connection to server.*/
#define CONNECTION_RETRY_MAX_BACKOFF_DELAY_MS (5000U)

/* The base back-off delay (in milliseconds) to use for connection retry attempts.*/
#define CONNECTION_RETRY_BACKOFF_BASE_MS (500U)

/* threshold to reset the beta */
#define BUTTON_LONG_PRESS_TIME_MS CONFIG_BUTTON_LONG_PRESS_TIME_MS

/* threshold to register the beta*/
#define BUTTON_SHORT_PRESS_TIME_MS CONFIG_BUTTON_SHORT_PRESS_TIME_MS

/* vscp receiver queue. */
#define CONFIG_VSCP_RECV_QUEUE_SIZE (10)

#define VSCP_QUEUE_MAX_WAIT_TIMEOUT_MS     (pdTICKS_TO_MS(5000))
#define VSCP_SEMAPHORE_MAX_WAIT_TIMEOUT_MS (pdTICKS_TO_MS(5000))

#define EVENT_GOT_NICKNAME (1 << 1)
#define EVENT_ENROLLED     (1 << 2)

typedef struct
{
    espnow_addr_t addr;
    size_t size;
    vscp_data_t *data;
} espnow_data_receiver_t;

/* alpha node guid/addr */
static uint8_t global_alpha_addr[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

static uint8_t g_beta_state = VSCP_TYPE_PROTOCOL_GENERAL;

// static espnow_data_receiver_t g_vscp_receiver = { 0 };
static QueueHandle_t g_vscp_queue = NULL;
static SemaphoreHandle_t g_vscp_sem = NULL;
static EventGroupHandle_t g_vscp_event_grp = NULL;

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

static void button_long_pressed_cb(void *arg, void *usr_data)
{
    ESP_LOGI(TAG, "BUTTON_LONG_PRESS_HOLD");
}

static esp_err_t vscp_is_node_register(void)
{
    /* return ESP_OK if register */
    return ESP_OK;
}

/**
 * @brief Register a new VSCP node.
 *
 * @return ESP_OK on success, error code otherwise.
 */
esp_err_t vscp_register_new_node(void)
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

    vscp_data_t *vscp_data = (vscp_data_t *)data;
    const char *str_data = (const char *)vscp_data->pdata;

    cJSON *pJson = cJSON_Parse(str_data);
    ESP_ALLOC_CHECK(pJson);

    cJSON *nameObj = cJSON_GetObjectItemCaseSensitive(pJson, "nickname");
    if (cJSON_IsNumber(nameObj))
    {
        global_self_nickname = (uint8_t)cJSON_GetNumberValue(nameObj);

        // TODO: Write nickname in NVS
        ESP_LOGI(TAG, "got nickname: %u", global_self_nickname);
    }
    cJSON_Delete(pJson);
    xEventGroupSetBits(g_vscp_event_grp, EVENT_GOT_NICKNAME);
    return ESP_OK;
}

/**
 * @brief Event handler for accepting the nickname.
 *
 * @return ESP_OK on success, error code otherwise.
 */
static esp_err_t vscp_nickname_accept_evt_cb(void)
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

    // TODO: Write the register status as success in NVS
    xEventGroupSetBits(g_vscp_event_grp, EVENT_ENROLLED);
    return ESP_OK;
}

/**
 * @brief Event handler for acknowledging enrollment.
 *
 * @return ESP_OK on success, error code otherwise.
 */
static esp_err_t vscp_enroll_ack_evt_cb(void)
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
    if (!json_str)
    {
        return ESP_ERR_NO_MEM;
    }
    vscp_data_t vscp_data = { 0 };
    size_t total_size = 0;
    // prepare registration message
    (void)helper_prepare_vscp_nodes_message((void *)&vscp_data, VSCP_MSG_TYPE_RESPONSE, VSCP_PRIORITY_NORMAL,
        VSCP_CLASS1_MEASUREMENT, VSCP_TYPE_MEASUREMENT_TEMPERATURE, NULL, &total_size);

    cJSON_Delete(pJson);
    return ESP_OK;
}

/**
 * @brief Event handler for time synchronization events.
 *
 * @param src_addr Source address.
 * @param data Data.
 * @param size Size of the data.
 * @return ESP_OK on success, ESP_FAIL if the source address is not equal to the expected address.
 */
static esp_err_t vscp_timesync_evt_cb(uint8_t *src_addr, void *data, size_t size)
{
    if (!ESPNOW_ADDR_IS_EQUAL(global_alpha_addr, src_addr))
    {
        return ESP_FAIL;
    }
    vscp_data_t *vscp = (vscp_data_t *)data;
    timestamp_tv.tv_sec = vscp->timestamp;
    timestamp_tv.tv_usec = 0;
    settimeofday(&timestamp_tv, NULL);
    return ESP_OK;
}

// buttons
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
    if (NULL == gpio_btn)
    {
        ESP_LOGE(TAG, "Button create failed");
    }

    iot_button_register_cb(gpio_btn, BUTTON_SINGLE_CLICK, button_single_click_cb, NULL);
    iot_button_register_cb(gpio_btn, BUTTON_LONG_PRESS_HOLD, button_long_pressed_cb, NULL);
    // TODO: blink led fast

    return ESP_OK;
}

static void vscp_recv_task(void *arg)
{
    esp_err_t ret = ESP_OK;
    g_vscp_queue = xQueueCreate(CONFIG_VSCP_RECV_QUEUE_SIZE, sizeof(espnow_data_receiver_t));
    ESP_ERROR_GOTO(!g_vscp_queue, EXIT, "failed to create vscp receive queue.");

    for (;;)
    {
        espnow_data_receiver_t espnow_recv = { 0 };
        vscp_event_handler_t evt_handler = NULL;

        if (xQueueReceive(g_vscp_queue, &espnow_recv, portMAX_DELAY))
        {
            vscp_data_t *data = (vscp_data_t *)espnow_recv.data;
            /* first verify crc */
            ret = helper_verify_crc((const vscp_data_t *)data);
            ESP_ERROR_GOTO(ret != ESP_OK, OUTLOOP, "helper_verify_crc <%s>", esp_err_to_name(ret));

            /* get event callback */
            ret = vscp_get_event_handler(data->vscp_class, data->vscp_type, &evt_handler);
            ESP_ERROR_GOTO(ret != ESP_OK, OUTLOOP, "vscp_get_event_handler <%s>", esp_err_to_name(ret));

            /* call event handler */
            if (evt_handler)
            {
                ret = evt_handler((uint8_t *)&espnow_recv.addr, (void *)espnow_recv.data, espnow_recv.size);
                if (ret != ESP_OK)
                {
                    ESP_LOGE(TAG, "evt_handler of class<%d>type<%d>, <%s>", data->vscp_class, data->vscp_type,
                        esp_err_to_name(ret));
                }
            }
        }

    OUTLOOP:
        xSemaphoreGive(g_vscp_sem);
    }

EXIT:
    if (g_vscp_queue)
    {
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
    if (xSemaphoreTake(g_vscp_sem, VSCP_SEMAPHORE_MAX_WAIT_TIMEOUT_MS) == pdPASS)
    {
        espnow_data_receiver_t vscp_recv_data = { 0 };

        memcpy(&vscp_recv_data.addr, src_addr, 6);
        if (!vscp_recv_data.data)
        {
            return ESP_ERR_NO_MEM; // Memory allocation failed
        }
        if (xQueueSend(g_vscp_queue, (void *)&vscp_recv_data, VSCP_QUEUE_MAX_WAIT_TIMEOUT_MS) != pdFAIL)
        {
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

    if (!espnow_data)
    {
        return ESP_ERR_NO_MEM;
    }

    espnow_frame_head_t frame_head = { .retransmit_count = 2, .broadcast = true, .security = true, .ack = true };
    memcpy(espnow_data, data, size);

    /* init backoff attemp done */
    g_backoff_reconnect_params.attemptsDone = 0;
    do
    {
        ret = espnow_send(ESPNOW_DATA_TYPE_DATA, global_alpha_addr, espnow_data, size, &frame_head,
            VSCP_QUEUE_MAX_WAIT_TIMEOUT_MS);
        if (ret != ESP_OK)
        {
            /* Generate a random number and get back-off value (in milliseconds) for the next connection retry. */
            backoffAlgStatus
                = BackoffAlgorithm_GetNextBackoff(&g_backoff_reconnect_params, (uint32_t)rand(), &nextRetryBackOff);

            if (backoffAlgStatus == BackoffAlgorithmRetriesExhausted)
            {
                ESP_LOGE(TAG, "espnow_send failed <%s>, all attempts exhausted.", esp_err_to_name(ret));
                ret = ESP_FAIL;
            }
            else if (backoffAlgStatus == BackoffAlgorithmSuccess)
            {
                ESP_LOGW(TAG, "espnow_send failed <%s>", esp_err_to_name(ret));
                vTaskDelay(pdMS_TO_TICKS(nextRetryBackOff));

                ESP_LOGI(TAG, "Sending again... <%d/%d> after %hu ms backoff.", ++retryCount,
                    CONNECTION_RETRY_MAX_ATTEMPTS, (unsigned short)nextRetryBackOff);
            }
        }
    }
    while ((ret != ESP_OK) && (backoffAlgStatus == BackoffAlgorithmSuccess));

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

    if (espnow_get_key(key_info) != ESP_OK)
    {
        esp_fill_random(key_info, APP_KEY_LEN);
    }
    /* Initialize reconnect attempts and interval */
    g_backoff_reconnect_params.attemptsDone = 0;

    /* Attempt to connect to Alpha. If connection fails, retry after
     * a timeout. Timeout value will exponentially increase until maximum
     * attempts are reached.
     */
    do
    {
        // TODO:
        ret = espnow_security_inteface_initiator_connect(NULL, 0, (uint8_t *)&key_info); // send broadcast message

        if (ret != ESP_OK)
        {
            /* Generate a random number and get back-off value (in milliseconds) for the next connection retry. */
            backoffAlgStatus
                = BackoffAlgorithm_GetNextBackoff(&g_backoff_reconnect_params, (uint32_t)rand(), &nextRetryBackOff);

            if (backoffAlgStatus == BackoffAlgorithmRetriesExhausted)
            {
                ESP_LOGE(TAG, "Connection to the broker failed, all attempts exhausted.");
                ret = ESP_FAIL;
            }
            else if (backoffAlgStatus == BackoffAlgorithmSuccess)
            {
                ESP_LOGW(TAG, "Connection to the broker failed.");
                vTaskDelay(pdMS_TO_TICKS(nextRetryBackOff));

                ESP_LOGI(TAG, "Retrying connection <%d/%d> after %hu ms backoff.", ++retryCount,
                    CONNECTION_RETRY_MAX_ATTEMPTS, (unsigned short)nextRetryBackOff);
            }
        }
    }
    while ((ret != ESP_OK) && (backoffAlgStatus == BackoffAlgorithmSuccess));

    return ret;
}

static void vscp_enrolling_process_task(void *paramters)
{
    (void)paramters;
    esp_err_t ret = ESP_OK;
    g_vscp_event_grp = xEventGroupCreate();
    ESP_ERROR_GOTO(!g_vscp_event_grp, EXIT, "failed to create event group");

    EventBits_t eventBits = 0;
    for (;;)
    {
        switch (g_beta_state)
        {
            case VSCP_TYPE_PROTOCOL_GENERAL:
                ret = vscp_register_new_node();
                ESP_ERROR_CONTINUE(ret != ESP_OK, "vscp_register_new_node <%s>", esp_err_to_name(ret));
                // Wait for any event bit to be set
                eventBits = xEventGroupWaitBits(g_vscp_event_grp, EVENT_GOT_NICKNAME, pdTRUE, pdTRUE, portMAX_DELAY);
                g_beta_state = VSCP_TYPE_PROTOCOL_NICKNAME_ACCEPTED;
                break;

            case VSCP_TYPE_PROTOCOL_NICKNAME_ACCEPTED:
                ret = vscp_nickname_accept_evt_cb();
                ESP_ERROR_CONTINUE(ret != ESP_OK, "vvscp_nickname_accept_ack_evt <%s>", esp_err_to_name(ret));
                // Wait for any event bit to be set
                eventBits = xEventGroupWaitBits(g_vscp_event_grp, EVENT_ENROLLED, pdTRUE, pdTRUE, portMAX_DELAY);
                g_beta_state = VSCP_TYPE_PROTOCOL_ENROLL_ACK;
                break;

            case VSCP_TYPE_PROTOCOL_ENROLL_ACK:
                ret = vscp_enroll_ack_evt_cb();
                ESP_ERROR_CONTINUE(ret != ESP_OK, "vscp_enroll_ack_evt_cb <%s>", esp_err_to_name(ret));

                g_beta_state = VSCP_TYPE_PROTOCOL_MAX;
                // Exit from loop

                break;
            default:
                goto EXIT;
                break;
        }
    }
EXIT:
    ESP_LOGI(TAG, "<%d:%s> deleting...", __LINE__, __func__);
    vTaskDelete(NULL);
}
esp_err_t vscp_init_beta_app(void)
{
    esp_err_t ret = ESP_OK;
    /* get self mac addr */
    esp_wifi_get_mac(WIFI_IF_STA, global_self_guid);

    // #ifdef CONFIG_USE_BUTTON_ENABLE
    ret = init_register_button();
    if (ret != ESP_OK)
    {
        return ret;
    }
    // #endif /* CONFIG_USE_BUTTON_ENABLE */

    /*  event callback */
    ESP_ERROR_CHECK_WITHOUT_ABORT(
        vscp_register_event_handler(VSCP_CLASS1_PROTOCOL, VSCP_TYPE_PROTOCOL_SET_NICKNAME, vscp_set_nickname_evt_cb));

    ESP_ERROR_CHECK_WITHOUT_ABORT(
        vscp_register_event_handler(VSCP_CLASS1_PROTOCOL, VSCP_TYPE_PROTOCOL_ENROLL, vscp_enroll_evt_cb));

    ESP_ERROR_CHECK_WITHOUT_ABORT(
        vscp_register_event_handler(VSCP_CLASS1_CONTROL, VSCP_TYPE_CONTROL_TIME_SYNC, vscp_timesync_evt_cb));

    /* meseasurement event calbacks */
    ESP_ERROR_CHECK_WITHOUT_ABORT(
        vscp_register_event_handler(VSCP_CLASS1_MEASUREMENT, VSCP_TYPE_MEASUREMENT_GENERAL, vscp_general_meas_evt_cb));

    /* Initialize reconnect attempts and interval */
    BackoffAlgorithm_InitializeParams(&g_backoff_reconnect_params, CONNECTION_RETRY_BACKOFF_BASE_MS,
        CONNECTION_RETRY_MAX_BACKOFF_DELAY_MS, CONNECTION_RETRY_MAX_ATTEMPTS);

    ret = vscp_is_node_register();
    if (ret != ESP_OK)
    {
        // initiate register proce
        xTaskCreate(&vscp_enrolling_process_task, "enroll process", 1024 * 3, NULL, 5, NULL);
        xTaskCreate(&vscp_recv_task, "Beta Task", 1024 * 2, NULL, 5, NULL);
    }
    return ret;
}