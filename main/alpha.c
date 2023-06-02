

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
#include "wifim.h"

/* backoff algorithm header.*/
#include "backoff_algorithm.h"

/* vscp headers */
#include "vscp.h"

static const char *TAG = "alpha.c";

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

esp_err_t vscp_alpha_init_app(void)
{
    /* wifim start return if wifi connected */
    esp_err_t ret = wifim_init();

    return ret;
}