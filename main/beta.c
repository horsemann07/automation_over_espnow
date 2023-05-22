
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
#include "esp_mac.h"
#include "esp_random.h"
#endif

#include "espnow.h"
#include "espnow_security.h"
#include "espnow_security_handshake.h"
#include "espnow_storage.h"
#include "espnow_utils.h"
#include "iot_button.h"
#include "vscp_helper.h"

/*Include backoff algorithm header for retry logic.*/
#include "backoff_algorithm.h"

static const char *TAG = "beta.c";
/**
 * @brief The maximum number of retries for connecting to server.
 */
#define CONNECTION_RETRY_MAX_ATTEMPTS (5U)

/**
 * @brief The maximum back-off delay (in milliseconds) for retrying connection to server.
 */
#define CONNECTION_RETRY_MAX_BACKOFF_DELAY_MS (5000U)

/**
 * @brief The base back-off delay (in milliseconds) to use for connection retry attempts.
 */
#define CONNECTION_RETRY_BACKOFF_BASE_MS (500U)

/* threshold to reset the beta */
#define CONFIG_BUTTON_LONG_PRESS_TIME_MS (0)

/* threshold to register the beta*/
#define CONFIG_BUTTON_SHORT_PRESS_TIME_MS (0)

espnow_addr_t self_addr = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
espnow_addr_t alpha_node = { 0x00 };

static QueueHandle_t g_vscp_queue;
static uint8_t g_self_nickname = 0xFF;

typedef struct
{
    uint8_t packetData[ESPNOW_SEC_PACKET_MAX_SIZE];
    uint16_t packetSize;
} Packet_t;

static uint16_t vscp_max_data_received = 0;

uint8_t *g_receivedData = NULL;
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
# Pseudo code for beta device workflow

# Step 1: Check Device register or not
if not is_registered():
    # Step 2: NO - Register button and interrupt callback
    register_button_pressed = wait_for_button_press(timeout=10)  # Timeout after 10 seconds
    if register_button_pressed:
        # Step 2.1: Initiate ESP-NOW security
        symmetric_key, nickname = initiate_espnow_security()
        # Step 2.2: Save symmetric key and nickname in NVM
        save_to_nvm(symmetric_key, nickname)
    else:
        # Handle registration timeout or button not pressed
        handle_registration_timeout()
else:
    # Step 1: YES - Retrieve symmetric key and nickname from NVM
    symmetric_key, nickname = retrieve_from_nvm()

# Step 3: Initialize Sensor interrupt callback
initialize_sensor_interrupt_callback()

# Step 4: Send notification to alpha that beta is online
send_notification_to_alpha()

# Step 5: Go into responder mode
responder_mode = True

while True:
    # Step 2: If any interrupt happens from the sensor
    if interrupt_occurred():
        # Step 2.1: Get sensor data
        sensor_data = read_sensor_data()
        # Step 2.2: Prepare message
        message = prepare_message(sensor_data)
        # Step 2.3: Stop responder mode if it is active
        if responder_mode:
            stop_responder_mode()
        # Step 2.4: Initiate secure connection using ESP-NOW initiator
        success = initiate_secure_connection(symmetric_key)
        if success:
            # Step 2.5: Send data and wait for acknowledgement or retry until received
            send_data_with_acknowledgement(message)
        else:
            # Handle failed secure connection
            handle_failed_connection()

    # Other code and logic for the beta device

    # Sleep or delay for a certain period before repeating the loop
    sleep(1)
*/

static void button_single_click_cb(void *arg, void *usr_data)
{
    ESP_LOGI(TAG, "BUTTON_SINGLE_CLICK");
}

static void button_long_pressed_cb(void *arg, void *usr_data)
{
    ESP_LOGI(TAG, "BUTTON_LONG_PRESS_HOLD");
}

static void vscp_registeration_callback(uint8_t *src_addr, void *data, size_t size, wifi_pkt_rx_ctrl_t *rx_ctrl)
{
    // receive nickname and strore it.
    if (src_addr)
    {
        // this might be alpha node
        memcpy(alpha_node, src_addr, 6);
    }

    espnow_data_t *espnow_data = (espnow_data_t *)data;
    vscp_data_t *vscp_data = (vscp_data_t)espnow_data->payload;

    if (vscp_data->data_size_long) { }
}

static esp_err_t vscp_is_node_register(void)
{
    /* return ESP_OK if register */
    return ESP_OK;
}

static void receivePacket(uint8_t *packetData, uint16_t packetSize)
{
    static uint16_t receivedPacketCount = 0;
    if (receivedPacketCount < vscp_max_data_received / ESPNOW_SEC_PACKET_MAX_SIZE)
    {
        memcpy(receivedPackets[receivedPacketCount].packetData, packetData, packetSize);
        receivedPackets[receivedPacketCount].packetSize = packetSize;
        receivedPacketCount++;
    }
}

static vscp_establish_secure_connection(void)
{
    esp_err_t ret = ESP_OK;
    BackoffAlgorithmStatus_t backoffAlgStatus = BackoffAlgorithmSuccess;
    BackoffAlgorithmContext_t reconnectParams;
    uint16_t nextRetryBackOff;
    uint8_t retryCount = 0;

    ret = espnow_app_init(&vscp_registeration_callback);
    ESP_ERROR_RETURN(ret != ESP_OK, ret, "<%s> espnow_app_init", esp_err_to_name(ret));

    uint8_t key_info[APP_KEY_LEN];

    if (espnow_get_key(key_info) != ESP_OK)
    {
        esp_fill_random(key_info, APP_KEY_LEN);
    }
    /* Initialize reconnect attempts and interval */
    BackoffAlgorithm_InitializeParams(&reconnectParams, CONNECTION_RETRY_BACKOFF_BASE_MS,
        CONNECTION_RETRY_MAX_BACKOFF_DELAY_MS, CONNECTION_RETRY_MAX_ATTEMPTS);

    /* Attempt to connect to MQTT broker. If connection fails, retry after
     * a timeout. Timeout value will exponentially increase until maximum
     * attempts are reached.
     */
    do
    {
        ret = espnow_app_start_sec_initiator(NULL, 0, &key_info); // send broadcast message

        /* Generate a random number and get back-off value (in milliseconds) for the next connection retry. */
        backoffAlgStatus = BackoffAlgorithm_GetNextBackoff(&reconnectParams, (uint32_t)rand(), &nextRetryBackOff);

        if (backoffAlgStatus == BackoffAlgorithmRetriesExhausted)
        {
            ESP_LOGE(TAG, "Connection to the broker failed, all attempts exhausted.");
            returnStatus = EXIT_FAILURE;
        }
        else if (backoffAlgStatus == BackoffAlgorithmSuccess)
        {
            ESP_LOGW(TAG, "Connection to the broker failed.");
            vTaskDelay(pdMS_TO_TICKS(nextRetryBackOff));

            ESP_LOGI(TAG, "Retrying connection <%d/%d> after %hu ms backoff.", ++retryCount,
                CONNECTION_RETRY_MAX_ATTEMPTS, (unsigned short)nextRetryBackOff);
        }
    }
    while ((ret != ESP_OK) && (backoffAlgStatus == BackoffAlgorithmSuccess));

    return ret;
}

// registration button
esp_err_t init_register_button(void)
{
    // create gpio button
    button_config_t gpio_btn_cfg = {
    .type = BUTTON_TYPE_GPIO,
    .long_press_ticks = CONFIG_BUTTON_LONG_PRESS_TIME_MS,
    .short_press_ticks = CONFIG_BUTTON_SHORT_PRESS_TIME_MS,
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

    iot_button_register_cb(gpio_btn, BUTTON_SINGLE_CLICK, button_single_click_cb, BUTTON_SINGLE_CLICK);
    iot_button_register_cb(gpio_btn, BUTTON_LONG_PRESS_HOLD, button_single_click_cb, BUTTON_LONG_PRESS_HOLD);

    // TODO: blink led fast
}

esp_err_t vscp_prepare_node_registration_msg(vscp_data_t *buffer)
{
    ESP_PARAM_CHECK(buffer);

    prepare_vscp_nodes_message((void *)buffer, g_self_nickname, 3, VSCP_PRIORITY_3, self_addr, VSCP_CLASS1_PROTOCOL,
        VSCP_TYPE_PROTOCOL_NEW_NODE_ONLINE, NULL);
}

esp_err_t vscp_send_data_over_espnow(const void *data, size_t size)
{
    ESP_PARAM_CHECK(data);
    esp_err_t ret = ESP_OK;
    vscp_data_t *vscp = (vscp_data_t *)data;

    uint8_t *espnow_data = ESP_CALLOC(1, ESPNOW_SEC_PACKET_MAX_SIZE);

    if (!data)
    {
        return ESP_ERR_NO_MEM;
    }
    espnow_frame_head_t frame_head = { //
        .retransmit_count = 2,
        .broadcast = true,
        .security = true,
        .ack = true
    };
    vscp->data_size_long = CHECK_ESPNOW_VSCP_MAX_DATA_LEN_EXCEED(ESPNOW_SEC_PACKET_MAX_SIZE, vscp->sizeData);
    if (vscp->data_size_long)
    {
        uint16_t offset = 0;
        while (offset < size)
        {
            uint16_t remain_size
                = (size - offset) < ESPNOW_SEC_PACKET_MAX_SIZE ? (size - offset) : ESPNOW_SEC_PACKET_MAX_SIZE;

            // Copy in espnow daa buffer for the current data
            memcpy(espnow_data, vscp + offset, remain_size);

            ret = espnow_send(ESPNOW_DATA_TYPE_DATA, ESPNOW_ADDR_BROADCAST, espnow_data, remain_size, &frame_head,
                portMAX_DELAY);
            ESP_ERROR_GOTO(ret != ESP_OK, CLEANUP, "faild to send data afte %u offset", offset);
            offset += remain_size;
        }
    }
    else

    {
        memcpy(espnow_data, vscp, size);
        ret = espnow_send(ESPNOW_DATA_TYPE_DATA, ESPNOW_ADDR_BROADCAST, espnow_data, size, &frame_head, portMAX_DELAY);
    }

CLEANUP:
    ESP_FREE(espnow_data);
    return ret;
}

esp_err_t vscp_init_registration_process(void)
{
    esp_err_t ret = vscp_establish_secure_connection();
    ESP_ERROR_RETURN(ret != ESP_OK, ret, "<%s> vscp_establish_secure_connection", esp_err_to_name(ret));

    vscp_data_t vscp_data = { 0 };
    size_t total_size = 0;
    // prepare registration message
    (void)prepare_vscp_nodes_message((void *)&vscp_data, g_self_nickname, 3, VSCP_PRIORITY_3, self_addr,
        VSCP_CLASS1_PROTOCOL, VSCP_TYPE_PROTOCOL_NEW_NODE_ONLINE, NULL, &total_size);

    ret = vscp_send_data_over_espnow(&vscp_data, total_size);
    ESP_ERROR_RETURN(ret != ESP_OK, ret, "<%s> vscp_send_data_over_espnow", esp_err_to_name(ret));
}

static void espnow_main_task(void *arg)
{
    espnow_data_t espnow_data = { 0 };
    vscp_data_t *vscp_data = NULL;
    bool is_long_data_receiving = false;

    ESP_LOGI(TAG, "main task entry");

    g_vscp_queue = xQueueCreate(10, sizeof(espnow_data_t));
    ESP_ERROR_GOTO(!g_vscp_queue, EXIT, "Create espnow event queue fail");

    uint16_t totalReceivedSize = 0;
    uint16_t offset = 0;
    while (true)
    {
        if (xQueueReceive(g_vscp_queue, &espnow_data, portMAX_DELAY) != pdTRUE)
        {
            continue;
        }

        if (!is_long_data_receiving)
        {
            vscp_data = (vscp_data_t *)espnow_data->payload;
        }

        if (vscp_data.vscp_class == VSCP_CLASS1_PROTOCOL &&vscp_data.vscp_type = VSCP_TYPE_PROTOCOL_PROBE_ACK)
        {
            // symmetric key or app key is taken care by espnow components
            g_self_nickname = vscp_data.nickname;
        }

        /* nothing to receive */
        if (vscp_data.sizeData == 0)
        {
            continue;
        }

        if (vscp_data.data_size_long)
        {
            is_long_data_receiving = true;
            if (!g_receivedData)
            {
                g_receivedData = ESP_CALLOC(1, VSCP_TOTAL_DATA_SIZE(vscp_data.sizeData));
                if (!g_receivedData)
                {
                    ESP_LOGE(TAG, "ESP_ERR_NO_MEM to receive data");
                    goto EXIT;
                }
            }
            memcpy(g_receivedData + offset, espnow_data.payload, espnow_data.size);
            // if (offset > 0)
            // {
            //     memcpy(g_receivedData, espnow_data.payload, espnow_data.size);
            //     // offset += espnow_data.size
            // }
            // else
            // {
            //     // uint8_t filter_vscp_header = sizeof(vscp_data_t);
            //     memcpy(g_receivedData, espnow_data.payload, espnow_data.size);
            //     // offset += (espnow_data.size - filter_vscp_header);
            // }
            offset += espnow_data.size;

            if (offset >= VSCP_TOTAL_DATA_SIZE(vscp_data.sizeData))
            {
                offset = 0;
                is_long_data_receiving = false
            }
        }
    }

EXIT:
    if (g_vscp_queue)
    {
        while (xQueueReceive(g_vscp_queue, &vscp_data, 0))
        {
            ESP_FREE(vscp_data.pdata);
        }

        vQueueDelete(g_vscp_queue);
        g_vscp_queue = NULL;
    }

    // if (g_ack_queue)
    // {
    //     vQueueDelete(g_ack_queue);
    //     g_ack_queue = NULL;
    // }

    ESP_LOGI(TAG, "main task exit");
    vTaskDelete(NULL);
}

esp_err_t vscp_start_beta(void)
{
    esp_err_t ret = ESP_OK;

    xTaskCreate(&espnow_main_task, "Beta Task", 1024 * 2, NULL, 5, NULL);
    ret = vscp_is_node_register();
    if (ret != ESP_OK)
    {
        // initiate register process
        ret = vscp_init_registration_process();
        ESP_ERROR_RETURN(ret != ESP_OK, ret, "<%s> vscp_init_registration_process", esp_err_to_name(ret));
    }
}