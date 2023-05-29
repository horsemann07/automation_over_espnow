
#include "vscp_helper.h"
#include "esp_err.h"
#include <esp_crc.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define TAG               "vscp_helper.c"
#define CURRENT_TIME_ZONE "IST"

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

static time_t get_epoch_timestamp(void)
{
    time_t epoch_time = time(NULL);
    return epoch_time;
}

static uint16_t calculate_crc(const vscp_data_t *data)
{
    // Calculate CRC on the structure fields
    const uint8_t *ptr = (const uint8_t *)&data->vscp_class;
    size_t dataSize = sizeof(data->vscp_class) + sizeof(data->vscp_type) + sizeof(data->guid) + sizeof(data->sizeData)
                      + data->sizeData;

    uint16_t crc = 0;
    crc = esp_crc16_be(crc, ptr, dataSize);

    return crc;
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
/*
This function takes the following parameters:

buffer is a pointer to the message buffer. This is a pointer to a char pointer so that the function can allocate memory
for the buffer and return it to the caller. size is a pointer to a uint16_t variable where the size of the message will
be returned. pjname is a pointer to a string that represents the project name. guid is an array of uint8_t values that
represents the guid. class is a uint16_t value that represents the VSCP class. type is a uint16_t value that represents
the VSCP type. nickname is a uint8_t value that represents the nickname.*/
esp_err_t helper_convert_to_mqtt_topics(char *buffer, uint16_t *size, char *pjname, uint8_t guid[6], uint16_t class,
    uint16_t type)
{
    // Calculate the size of the message
    *size = strlen(pjname) + 25; // 25 is the length of the guid, class, type fields

    // Allocate memory for the message buffer
    *buffer = malloc(*size + 1);
    if (*buffer == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    // Format the message
    sprintf(*buffer, "%s/%02x%02x%02x%02x%02x%02x/%04x/%04x", pjname, guid[0], guid[1], guid[2], guid[3], guid[4],
        guid[5], class, type);

    return ESP_OK;
}

esp_err_t helper_prepare_vscp_mqtt_message(void *buffer, size_t *size, uint8_t priority, long timestamp,
    uint8_t guid[6], uint16_t class, uint16_t type, const char *data)
{
    if (!(buffer) || !(size))
    {
        return ESP_ERR_INVALID_ARG;
    }
    vscp_mqtt_data_t *vscp = (vscp_mqtt_data_t *)buffer;

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
        vscp->guid[i] = guid[i];
    }
    vscp->sizeData = strlen(data);
    vscp->pdata = (uint8_t *)data;

    vscp->crc = calculate_crc(vscp);
    return ESP_OK;
}

esp_err_t helper_prepare_vscp_nodes_message(void *buffer, uint8_t nickname, uint8_t priority, uint8_t guid[6],
    uint16_t class, uint16_t type, const char *data, size_t *size)
{
    if (!(buffer))
    {
        return ESP_ERR_INVALID_ARG;
    }

    vscp_data_t *vscp = (vscp_data_t *)buffer;
    vscp->nickname = nickname;
    vscp->priority = priority;
    vscp->vscp_class = class;
    vscp->vscp_type = type;
    for (int i = 0; i < 6; i++)
    {
        vscp->guid[i] = guid[i];
    }

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
    *size = VSCP_TOTAL_DATA_SIZE(vscp->sizeData);

    // vscp->data_size_long = CHECK_ESPNOW_VSCP_DATA_MAX_LEN_EXCEED(vscp->sizeData);

    vscp->crc = calculate_crc(vscp);
    vscp->timestamp = get_epoch_timestamp();
    return ESP_OK;
}