
#include "vscp_led.h"
#include "led_indicator.h"


/**
 * @brief The blink type with smaller index has the higher priority
 * eg. BLINK_FACTORY_RESET priority is higher than BLINK_UPDATING
 */
enum {
    BLINK_DEVICE_STARTUP,       /*!< device/node started */
    BLINK_DEVICE_FACTORY_RESET, /*!< restoring factory settings */
    BLINK_OTA_UPDATING,         /*!< updating software */
    BLINK_WIFI_PROVISIONING,    /*!< wifi provisioning */
    BLINK_WIFI_CONNECTED,       /*!< wifi connected */
    BLINK_WIFI_RECONNECTING,    /*!< wifi reconnected */
    BLINK_CONNECTED_TO_SERVER,  /*!< connected to AP (or Cloud) succeed */
    BLINK_ESPNOW_SECURE_PAIRED, /*!< securely paired with other nodes */
    BLINK_MAX,                  /*!< INVALID type */
};

/*********************************** Config Blink List in Different Conditions ***********************************/
/**
 * @brief connecting to AP (or Cloud)
 *
 */
static const blink_step_t deivce_start[] = {
    { LED_BLINK_HOLD, LED_STATE_ON, 50 },   // step1: turn on LED 50 ms
    { LED_BLINK_HOLD, LED_STATE_OFF, 100 }, // step2: turn off LED 100 ms
    { LED_BLINK_HOLD, LED_STATE_ON, 150 },  // step3: turn on LED 150 ms
    { LED_BLINK_HOLD, LED_STATE_OFF, 100 }, // step4: turn off LED 100 ms
    { LED_BLINK_STOP, 0, 0 },               // step5: stop blink (off)
};

/**
 * @brief restoring factory settings
 *
 */
static const blink_step_t factory_reset[] = {
    { LED_BLINK_HOLD, LED_STATE_ON, 200 },
    { LED_BLINK_HOLD, LED_STATE_OFF, 200 },
    { LED_BLINK_LOOP, 0, 0 },
};

/**
 * @brief updating software
 *
 */
static const blink_step_t ota_updating[] = {
    { LED_BLINK_HOLD, LED_STATE_ON, 50 },
    { LED_BLINK_HOLD, LED_STATE_OFF, 100 },
    { LED_BLINK_HOLD, LED_STATE_ON, 50 },
    { LED_BLINK_HOLD, LED_STATE_OFF, 800 },
    { LED_BLINK_LOOP, 0, 0 },
};

/**
 * @brief provisioning
 *
 */
static const blink_step_t wifi_provisioning[] = {
    { LED_BLINK_HOLD, LED_STATE_ON, 500 },
    { LED_BLINK_HOLD, LED_STATE_OFF, 500 },
    { LED_BLINK_LOOP, 0, 0 },
};

/**
 * @brief connected to wifi succeed
 *
 */
static const blink_step_t wifi_connected[] = {
    { LED_BLINK_HOLD, LED_STATE_ON, 1000 },
    { LED_BLINK_LOOP, 0, 0 },
};

/**
 * @brief reconnecting to wifi, if lose connection
 *
 */
static const blink_step_t wifi_reconnecting[] = {
    { LED_BLINK_HOLD, LED_STATE_ON, 100 },
    { LED_BLINK_HOLD, LED_STATE_OFF, 300 },
    { LED_BLINK_LOOP, 0, 0 },
};

/**
 * @brief connected to server
 *
 */
static const blink_step_t connected_to_server[] = {
    { LED_BLINK_HOLD, LED_STATE_ON, 1000 },
    { LED_BLINK_HOLD, LED_STATE_OFF, 1000 },
    { LED_BLINK_LOOP, 0, 0 },
};

/**
 * @brief espnow secure paired
 *
 */
static const blink_step_t espnow_secure_paired[] = {
    { LED_BLINK_HOLD, LED_STATE_OFF, 1000 },
    { LED_BLINK_STOP, 0, 0 },
};

/**
 * @brief LED indicator blink lists, the index like BLINK_FACTORY_RESET defined the priority of the blink
 *
 */
static blink_step_t const *vscp_led_indicator_blink_lists[] = {
    [BLINK_DEVICE_STARTUP] = deivce_start,
    [BLINK_DEVICE_FACTORY_RESET] = factory_reset,
    [BLINK_OTA_UPDATING] = ota_updating,
    [BLINK_WIFI_PROVISIONING] = wifi_provisioning,
    [BLINK_WIFI_CONNECTED] = wifi_connected,
    [BLINK_WIFI_RECONNECTING] = wifi_reconnecting,
    [BLINK_CONNECTED_TO_SERVER] = connected_to_server,
    [BLINK_ESPNOW_SECURE_PAIRED] = espnow_secure_paired,
    [BLINK_MAX] = NULL,
};

/* LED blink_steps handling machine implementation */
const int VSCP_BLINK_LIST_NUM
    = (sizeof(default_led_indicator_blink_lists) / sizeof(default_led_indicator_blink_lists[0]));

static led_indicator_handle_t led_handle = NULL;

esp_err_t vscp_led_init(uint32_t led_gpio_num)
{
    led_indicator_config_t config = {
    .mode = LED_GPIO_MODE,
    .led_gpio_config = {
        .active_level = 1,
        .gpio_num = led_gpio_num,
    },
    .blink_lists = vscp_led_indicator_blink_lists,
    .blink_list_num = VSCP_BLINK_LIST_NUM,
    };

    led_handle = led_indicator_create(&config);
    if (led_handle == NULL) { // @suppress("Symbol is not resolved")
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t vscp_led_deinit(void) // @suppress("Type cannot be resolved")
{
    if (led_handle == NULL) { // @suppress("Symbol is not resolved")
        return ESP_FAIL; // @suppress("Symbol is not resolved")
    }

    return led_indicator_delete(led_handle);
}

esp_err_t vscp_led_startup_blink(void)
{
    if (led_handle == NULL) {
        return ESP_FAIL;
    }

    esp_err_t err = led_indicator_start(led_handle, BLINK_DEVICE_STARTUP);
    if (err != ESP_OK) {
        return err;
    }

    return led_indicator_stop(led_handle, BLINK_DEVICE_STARTUP);
}

esp_err_t vscp_led_factory_reset_blink_start(void)
{
    if (led_handle == NULL) {
        return ESP_FAIL;
    }

    return led_indicator_start(led_handle, BLINK_DEVICE_FACTORY_RESET);
}

esp_err_t vscp_led_factory_reset_blink_stop(void)
{
    if (led_handle == NULL) {
        return ESP_FAIL;
    }

    return led_indicator_stop(led_handle, BLINK_DEVICE_FACTORY_RESET);
}

esp_err_t vscp_led_ota_update_blink_start(void)
{
    if (led_handle == NULL) {
        return ESP_FAIL;
    }

    return led_indicator_preempt_start(led_handle, BLINK_OTA_UPDATING);
}

esp_err_t vscp_led_ota_update_blink_stop(void)
{
    if (led_handle == NULL) {
        return ESP_FAIL;
    }

    return led_indicator_preempt_stop(led_handle, BLINK_OTA_UPDATING);
}

esp_err_t vscp_led_wifi_provisioning_blink_start(void)
{
    if (led_handle == NULL) {
        return ESP_FAIL;
    }

    return led_indicator_start(led_handle, BLINK_WIFI_PROVISIONING);
}

esp_err_t vscp_led_wifi_provisioning_blink_stop(void)
{
    if (led_handle == NULL) {
        return ESP_FAIL;
    }

    return led_indicator_stop(led_handle, BLINK_WIFI_PROVISIONING);
}

esp_err_t vscp_led_wifi_connected_blink_start(void)
{
    if (led_handle == NULL) {
        return ESP_FAIL;
    }

    return led_indicator_start(led_handle, BLINK_WIFI_CONNECTED);
}

esp_err_t vscp_led_wifi_connected_blink_stop(void)
{
    if (led_handle == NULL) {
        return ESP_FAIL;
    }

    return led_indicator_stop(led_handle, BLINK_WIFI_CONNECTED);
}

esp_err_t vscp_led_wifi_reconnecting_blink_start(void)
{
    if (led_handle == NULL) {
        return ESP_FAIL;
    }

    return led_indicator_start(led_handle, BLINK_WIFI_RECONNECTING);
}

esp_err_t vscp_led_wifi_reconnecting_blink_stop(void)
{
    if (led_handle == NULL) {
        return ESP_FAIL;
    }

    return led_indicator_stop(led_handle, BLINK_WIFI_RECONNECTING);
}

esp_err_t vscp_led_connected_to_server_blink_start(void)
{
    if (led_handle == NULL) {
        return ESP_FAIL;
    }

    return led_indicator_start(led_handle, BLINK_CONNECTED_TO_SERVER);
}

esp_err_t vscp_led_connected_to_server_blink_stop(void)
{
    if (led_handle == NULL) {
        return ESP_FAIL;
    }

    return led_indicator_stop(led_handle, BLINK_CONNECTED_TO_SERVER);
}

esp_err_t vscp_led_espnow_secure_paired_blink_start(void)
{
    if (led_handle == NULL) {
        return ESP_FAIL;
    }

    return led_indicator_start(led_handle, BLINK_ESPNOW_SECURE_PAIRED);
}

esp_err_t vscp_led_espnow_secure_paired_blink_stop(void)
{
    if (led_handle == NULL) {
        return ESP_FAIL;
    }

    return led_indicator_stop(led_handle, BLINK_ESPNOW_SECURE_PAIRED);
}
