

/* standard headers */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

/* esp-idf headers */
#include "esp_crc.h"
#include "esp_err.h"

/* vscp headers */
#include "vscp.h"

static const char *TAG = "vscp_helper.c";

#define CURRENT_TIME_ZONE "IST"

/* Set the system time to the received timestamp */
static struct timeval timestamp_tv;

/**
 * @brief Get the current date and time in string format.
 *
 * This function retrieves the current date and time and formats it as a string.
 *
 * @param buffer The buffer to store the date and time string. Must be at least 30 bytes.
 * @return esp_err_t Returns ESP_OK on success, ESP_FAIL on failure.
 */
static esp_err_t get_current_datetime_in_str(uint8_t buffer[30])
{
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    // Set timezone
    setenv("TZ", CURRENT_TIME_ZONE, 1);
    tzset();
    if (strftime((char *)buffer, 30, "%Y-%m-%d:%Z:%H:%M:%S", &timeinfo) > 0)
    {
        return ESP_OK;
    }

    return ESP_FAIL;
}

/**
 * @brief Get the epoch timestamp.
 *
 * This function retrieves the current epoch timestamp.
 *
 * @return time_t The epoch timestamp.
 */
static time_t get_epoch_timestamp(void)
{
    time_t epoch_time = time(NULL);
    return epoch_time;
}

esp_err_t helper_verify_crc(const vscp_data_t *data)
{
    const uint8_t *str_ptr = (const uint8_t *)&data->vscp_class;
    size_t dataSize = sizeof(data->vscp_class) + sizeof(data->vscp_type) + sizeof(data->guid) + sizeof(data->sizeData)
                      + data->sizeData;
    uint16_t cal_crc = 0;
    cal_crc = esp_crc16_be(cal_crc, str_ptr, dataSize);

    return (cal_crc != data->crc) ? ESP_FAIL : ESP_OK;
}

esp_err_t helper_convert_to_mqtt_topics(char *buffer, uint16_t *size, char *pjname, uint8_t guid[6], uint16_t class,
    uint16_t type)
{
    // Calculate the size of the message
    *size = strlen(pjname) + 25; // 25 is the length of the guid, class, type fields

    // Allocate memory for the message buffer
    buffer = malloc(*size + 1);
    if (!buffer)
    {
        return ESP_ERR_NO_MEM;
    }

    // Format the message
    sprintf(buffer, "%s/%02x%02x%02x%02x%02x%02x/%04x/%04x", pjname, guid[0], guid[1], guid[2], guid[3], guid[4],
        guid[5], class, type);

    return ESP_OK;
}

esp_err_t helper_prepare_vscp_mqtt_message(void *buffer, size_t *size, uint8_t priority, long timestamp, uint16_t class,
    uint16_t type, const char *data)
{
    if (!(buffer) || !(size))
    {
        return ESP_ERR_INVALID_ARG;
    }
    vscp_mqtt_data_t *vscp = (vscp_mqtt_data_t *)buffer;

    timestamp_tv.tv_sec = timestamp;
    timestamp_tv.tv_usec = 0;
    settimeofday(&timestamp_tv, NULL);
    time_t currentTime = time(NULL);               // Get current time
    struct tm *timeInfo = localtime(&currentTime); // Convert to local time

    vscp->timestamp = timestamp;
    vscp->year = timeInfo->tm_year + 1900;
    vscp->month = timeInfo->tm_mon + 1;
    vscp->day = timeInfo->tm_mday;
    vscp->hour = timeInfo->tm_hour;
    vscp->minute = timeInfo->tm_min;
    vscp->second = timeInfo->tm_sec;
    vscp->vscp_class = class;
    vscp->vscp_type = type;

    for (int i = 0; i < 6; i++)
    {
        vscp->guid[i] = global_self_guid[i];
    }
    vscp->sizeData = strlen(data);
    vscp->pdata = (uint8_t *)data;

    // Calculate CRC on the structure fields
    const uint8_t *ptr = (const uint8_t *)&vscp->vscp_class;
    size_t dataSize = sizeof(vscp->vscp_class) + sizeof(vscp->vscp_type) + sizeof(vscp->guid) + sizeof(vscp->sizeData)
                      + vscp->sizeData;

    uint16_t crc = 0;
    crc = esp_crc16_be(crc, ptr, dataSize);
    vscp->crc = crc;
    return ESP_OK;
}

esp_err_t helper_prepare_vscp_nodes_message(void *buffer, uint8_t priority, uint16_t class, uint16_t type,
    const char *data, size_t *size)
{
    if (!(buffer))
    {
        return ESP_ERR_INVALID_ARG;
    }

    vscp_data_t *vscp = (vscp_data_t *)buffer;

    vscp->nickname = global_self_nickname;
    vscp->priority = priority;
    for (int i = 0; i < 6; i++)
    {
        vscp->guid[i] = global_self_guid[i];
    }

    vscp->vscp_class = class;
    vscp->vscp_type = type;

    if (data != NULL)
    {
        vscp->sizeData = strlen(data);
        vscp->pdata = (uint8_t *)data;
    }
    else
    {
        vscp->sizeData = 0;
        vscp->pdata = (uint8_t *)data;
    }
    *size = VSCP_TOTAL_TRANSMITTING_DATA_SIZE(vscp->sizeData);

    // Calculate CRC on the structure fields
    const uint8_t *ptr = (const uint8_t *)&vscp->vscp_class;
    size_t dataSize = sizeof(vscp->vscp_class) + sizeof(vscp->vscp_type) + sizeof(vscp->guid) + sizeof(vscp->sizeData)
                      + vscp->sizeData;

    uint16_t crc = 0;
    crc = esp_crc16_be(crc, ptr, dataSize);
    vscp->crc = crc;
    vscp->timestamp = get_epoch_timestamp();
    return ESP_OK;
}