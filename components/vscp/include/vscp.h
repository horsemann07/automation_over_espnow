/*
 FILE: vscp.h

 This file is part of the VSCP (https://www.vscp.org)

 The MIT License (MIT)

 Copyright © 2000-2022 Ake Hedman, the VSCP project
 <info@vscp.org>

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.

            !!!!!!!!!!!!!!!!!!!!  W A R N I N G  !!!!!!!!!!!!!!!!!!!!
 This file may be a copy of the original file. This is because the file is
 copied to other projects as a convenience. Thus editing the copy will not make
 it to the original and will be overwritten.
 The original file can be found in the vscp_softare source tree under
 src/vscp/common
*/

#ifndef _VSCP_H_
#define _VSCP_H_

/* standard headers */
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

/* esp-idf headers */
#include "esp_err.h"

/* vscp headers */
#include "vscp_class.h"
#include "vscp_type.h"

#ifdef __cplusplus
extern "C" {
#endif

/*     * * * General structure for VSCP * * *   */

/* This structure is for VSCP Level I   */

typedef struct
{
    uint32_t timestamp; /* Relative time stamp for package in microseconds */
    uint16_t crc;       /* crc checksum (calculated from here to end) */
                        /* Used for UDP/Ethernet etc */

    /* ----- CRC should be calculated from here to end + data block ----  */
    uint8_t nickname;     /* Node nickname */
    uint8_t priority : 4; /* 4-bit field representing the priority of the package */
    uint8_t msg_type : 1; /* 1-bit field to indicate message type i.e REQUEST or REQPONSE*/
    uint8_t : 3;          /* Reserved */
    uint8_t vscp_class;   /* VSCP class */
    uint8_t vscp_type;    /* VSCP type */
    uint8_t guid[6];      /* Node globally unique id MSB(0) -> LSB(15) */
    uint16_t sizeData;    /* Number of valid data bytes */
    uint8_t *pdata;       /* Pointer to data. Max 512 bytes */

} __attribute__((packed)) vscp_data_t;

/*
    This structure is for VSCP Level II with data embedded == big!!!
 */

typedef struct
{
    uint16_t crc; /* CRC checksum (calculated from here to end) */
                  /* Used for UDP/Ethernet etc */

    /* Time block - Always UTC time */
    uint16_t year;
    uint8_t month;      /* 1-12 */
    uint8_t day;        /* 1-31 */
    uint8_t hour;       /* 0-23 */
    uint8_t minute;     /* 0-59 */
    uint8_t second;     /* 0-59 */

    uint32_t timestamp; /* Relative time stamp for package in microseconds. */
                        /* ~71 minutes before roll over */

    /* CRC should be calculated from here to end + data block */
    uint16_t vscp_class; /* VSCP class   */
    uint16_t vscp_type;  /* VSCP type    */
    uint8_t guid[6];     /* Node globally unique id MSB(0) -> LSB(5)    */
    uint16_t sizeData;   /* Number of valid data bytes   */
    uint8_t *pdata;      /* Pointer to data. Max. 512 bytes     */

} __attribute__((packed)) vscp_mqtt_data_t;

/**
 * @brief VSCP event handler prototype.
 *
 * This typedef represents the function pointer type for a VSCP event handler.
 * The event handler is responsible for processing VSCP events.
 *
 * @param[in] src_addr Pointer to the source address of the event.
 * @param[in] data Pointer to the data payload of the event.
 * @param[in] size Size of the data payload.
 * @return ESP_OK if the event handling is successful, an error code otherwise.
 */
typedef esp_err_t (*vscp_event_handler_t)(uint8_t *src_addr, void *data, size_t size);

/**
 * @brief Maximum data transport protocol can send in a single packet.
 */
#define PROTOCOL_SUPPORT_MAX_DATA(max_len) (max_len - sizeof(vscp_data_t))

/**
 * @brief Verify if VSCP data length exceeds the maximum VSCP data that can be sent in a single packet over ESP-NOW.
 *
 * @param mlen Maximum length of the packet.
 * @param len Length of the VSCP data.
 * @return true if the capacity is exceeded, false otherwise.
 */
#define VERIFY_PROTOCOL_MAX_CAPACITY_NOT_EXCEED(mlen, len) ((len > PROTOCOL_SUPPORT_MAX_DATA(mlen)) ? true : false)

/**
 * @brief Calculate the total transmitting data size, including the VSCP data header.
 *
 * @param pdata_size Size of the VSCP data payload.
 * @return Total size of the transmitting data.
 */
#define VSCP_TOTAL_TRANSMITTING_DATA_SIZE(pdata_size) (sizeof(vscp_data_t) + (pdata_size))

/*--------------------------------------------------------------------------------------------------*/
/**
 * @brief VSCP message type: Request
 */
#define VSCP_MSG_TYPE_REQUEST (0)

/**
 * @brief VSCP message type: Response
 */
#define VSCP_MSG_TYPE_RESPONSE (1)

/**
 * @brief VSCP Level 1
 */
#define VSCP_LEVEL1 (0x00)

/**
 * @brief VSCP Level 2
 */
#define VSCP_LEVEL2 (0x01)

/* Priorities in the header byte as OR'ed values */
/* Priorities go from 0-7 where 3 is the highest. 4-7 are unused. */
#define VSCP_PRIORITY_0 (0)
#define VSCP_PRIORITY_1 (1)
#define VSCP_PRIORITY_2 (2)
#define VSCP_PRIORITY_3 (3)

#define VSCP_PRIORITY_HIGH   (0x02)
#define VSCP_PRIORITY_LOW    (0x00)
#define VSCP_PRIORITY_NORMAL (0x01)

/**
 * @brief Global variable representing the self nickname.
 *
 * This variable holds the self nickname value used in the context of the application.
 * It is typically used to identify the local device or node within the VSCP network.
 */
uint8_t global_self_nickname;

/**
 * @brief Global array representing the self GUID.
 *
 * This array holds the self GUID (Globally Unique Identifier) value used in the context of the application.
 * It is typically used to uniquely identify the local device or node within the VSCP network.
 * The GUID consists of 6 bytes.
 */
uint8_t global_self_guid[6];

/************************** Helper api *****************************/
/**
 * @brief Prepare VSCP nodes message.
 *
 * This function prepares a VSCP nodes message in the specified buffer.
 *
 * @param[out] buffer Pointer to the buffer to store the prepared message.
 * @param[in] msg_type Flag indicating if the message is an request or response type.
 * @param[in] priority Priority of the message.
 * @param[in] class VSCP class of the message.
 * @param[in] type VSCP type of the message.
 * @param[in] data Pointer to the data to be included in the message.
 * @param[out] size Pointer to store the size of the prepared message.
 * @return ESP_OK if the message preparation is successful, an error code otherwise.
 */
esp_err_t helper_prepare_vscp_nodes_message(void *buffer, uint8_t msg_type, uint8_t priority, uint16_t class,
    uint16_t type, const char *data, size_t *size);

/**
 * @brief Prepare VSCP MQTT message.
 *
 * This function prepares a VSCP MQTT message in the specified buffer.
 *
 * @param[in,out] buffer Pointer to the buffer to store the prepared message.
 * @param[out] size Pointer to store the size of the prepared message.
 * @param[in] priority Priority of the message.
 * @param[in] timestamp Timestamp of the message.
 * @param[in] class VSCP class of the message.
 * @param[in] type VSCP type of the message.
 * @param[in] data Pointer to the data to be included in the message.
 * @return ESP_OK if the message preparation is successful, an error code otherwise.
 */
esp_err_t helper_prepare_vscp_mqtt_message(void *buffer, size_t *size, uint8_t priority, long timestamp, uint16_t class,
    uint16_t type, const char *data);

/**
 * @brief Verify VSCP data CRC.
 *
 * This function verifies the CRC of the VSCP data.
 *
 * @param[in] data Pointer to the VSCP data.
 * @return ESP_OK if the CRC verification is successful, an error code otherwise.
 */
esp_err_t helper_verify_crc(const vscp_data_t *data);

/************************** event handlers api *****************************/
esp_err_t vscp_evnt_handler_register_event(uint8_t class, vscp_event_handler_t class_handler, uint8_t type,
    vscp_event_handler_t type_handler);

esp_err_t vscp_evnt_handler_unregister_event(uint8_t class);
vscp_event_handler_t vscp_evnt_handler_get_class_handler(uint8_t class);
vscp_event_handler_t vscp_evnt_handler_get_cb_by_type(uint8_t class, uint8_t type);
size_t vscp_evnt_handler_get_num_classes(void);
size_t vscp_evnt_handler_get_num_types(uint8_t class);
esp_err_t vscp_evnt_handler_init(void);


#ifdef __cplusplus
}
#endif

#endif /* _VSCP_H_ */
