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

// #include <canal.h>
#include "esp_err.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <vscp_class.h>
#include <vscp_type.h>

#ifdef __cplusplus
extern "C" {
#endif

/*          * * * General structure for VSCP * * *   */

/* This structure is for VSCP Level II   */

typedef struct
{
    uint32_t timestamp; /* Relative time stamp for package in microseconds */
                        /* ~71 minutes before roll over */

    uint16_t crc;       /* crc checksum (calculated from here to end) */
                        /* Used for UDP/Ethernet etc */

    /* ----- CRC should be calculated from here to end + data block ----  */

    uint8_t nickname : 4; /* Node nickname */
    uint8_t priority : 2; /*  2-bit field representing the priority of the package (0-3, where 3 is the highest). */
    uint8_t data_size_long : 1; /* 1-bit field indicating if the size of the VSCP data exceeds the maximum allowed size
     of (ESPNOW_DATA_MAX(230) - sizeof(vscp_data_t)(34 + sizeof(pointer))))*/
    uint8_t : 1;                /* reserved */

    uint16_t vscp_class;        /* VSCP class */
    uint16_t vscp_type;         /* VSCP type */
    uint8_t guid[6];            /* Node globally unique id MSB(0) -> LSB(15) */
    uint16_t sizeData;          /* Number of valid data bytes */
    uint8_t *pdata;             /* Pointer to data. Max 512 bytes */

} __attribute__((packed)) vscp_data_t;

/*
    WARNING!!!
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

/* vscp event handler prototype */
typedef esp_err_t (*vscp_event_handler_t)(uint8_t *, uint8_t *, size_t);

/**
 * @brief maxmimum vscp data can be send in single packet over espnow.
 *
 */
#define MAX_VDATA_SEND_IN_SINGLE_PACKET(espnow_max_data_len) (espnow_max_data_len - sizeof(vscp_data_t))

/**
 * @brief verify vscp data len exceed the maxmimum vscp data can send in single packet over espnow.
 *
 */
#define CHECK_ESPNOW_VSCP_MAX_DATA_LEN_EXCEED(elen, len) ((len > MAX_SEND_DATA_SINGLE_PACKET(elen)) ? true : false)
#define VSCP_TOTAL_DATA_SIZE(pdata_size)                 (sizeof(vscp_data_t) + (pdata_size))
/*--------------------------------------------------------------------------------------------------*/
#define VSCP_ADDRESS_NEW_NODE (0xff)

#define VSCP_LEVEL1_MAXDATA (200)
#define VSCP_LEVEL2_MAXDATA (512)

#define VSCP_LEVEL1 (0x00)
#define VSCP_LEVEL2 (0x01)

/* Priorities in the header byte as or'in values */
/* Priorities goes from 0-7 where 3 is highest   */
#define VSCP_PRIORITY_0 (0)
#define VSCP_PRIORITY_1 (1)
#define VSCP_PRIORITY_2 (2)
#define VSCP_PRIORITY_3 (3)
#define VSCP_PRIORITY_4 (4)
#define VSCP_PRIORITY_5 (5)
#define VSCP_PRIORITY_6 (6)
#define VSCP_PRIORITY_7 (7)

#define VSCP_PRIORITY_HIGH   (0x02)
#define VSCP_PRIORITY_LOW    (0x00)
#define VSCP_PRIORITY_NORMAL (0x01)

#define VSCP_CLASS1_PROTOCOL (0)               /* VSCP Protocol Functionality */

#define VSCP_TYPE_PROTOCOL_GENERAL           0 /* General event. */
#define VSCP_TYPE_PROTOCOL_NEW_NODE_ONLINE   1 /* New node on line / Probe. */
#define VSCP_TYPE_PROTOCOL_SET_NICKNAME      2 /* Set nickname-ID for node. */
#define VSCP_TYPE_PROTOCOL_NICKNAME_ACCEPTED 3 /* Nickname-ID accepted. */
#define VSCP_TYPE_PROTOCOL_ENROLL            4 /* Enroll event. */
#define VSCP_TYPE_PROTOCOL_ENROLL_ACK        5 /* Enroll ack event. */
#define VSCP_TYPE_PROTOCOL_DROP_NICKNAME     7 /* Drop nickname-ID / Reset Device. */
#define VSCP_TYPE_PROTOCOL_MAX               8

#define VSCP_CLASS1_SECURITY (2)                     /* Security */

#define VSCP_TYPE_SECURITY_GENERAL                0  /* General event */
#define VSCP_TYPE_SECURITY_MOTION                 1  /* Motion Detect */
#define VSCP_TYPE_SECURITY_GLASS_BREAK            2  /* Glass break */
#define VSCP_TYPE_SECURITY_BEAM_BREAK             3  /* Beam break */
#define VSCP_TYPE_SECURITY_SENSOR_TAMPER          4  /* Sensor tamper */
#define VSCP_TYPE_SECURITY_SHOCK_SENSOR           5  /* Shock sensor */
#define VSCP_TYPE_SECURITY_SMOKE_SENSOR           6  /* Smoke sensor */
#define VSCP_TYPE_SECURITY_HEAT_SENSOR            7  /* Heat sensor */
#define VSCP_TYPE_SECURITY_DOOR_OPEN              9  /* Door open */
#define VSCP_TYPE_SECURITY_DOOR_CLOSE             10 /* Door Contact */
#define VSCP_TYPE_SECURITY_WINDOW_OPEN            11 /* Window open */
#define VSCP_TYPE_SECURITY_WINDOW_CLOSE           12 /* Window Contact */
#define VSCP_TYPE_SECURITY_AUTHENTICATED          21 /* Authenticated */
#define VSCP_TYPE_SECURITY_UNAUTHENTICATED        22 /* Unauthenticated */
#define VSCP_TYPE_SECURITY_AUTHORIZED             23 /* Authorized */
#define VSCP_TYPE_SECURITY_UNAUTHORIZED           24 /* Unauthorized */
#define VSCP_TYPE_SECURITY_ID_CHECK               25 /* ID check */
#define VSCP_TYPE_SECURITY_PIN_OK                 26 /* Valid pin */
#define VSCP_TYPE_SECURITY_PIN_FAIL               27 /* Invalid pin */
#define VSCP_TYPE_SECURITY_PIN_WARNING            28 /* Pin warning */
#define VSCP_TYPE_SECURITY_PIN_ERROR              29 /* Pin error */
#define VSCP_TYPE_SECURITY_PASSWORD_OK            30 /* Valid password */
#define VSCP_TYPE_SECURITY_PASSWORD_FAIL          31 /* Invalid password */
#define VSCP_TYPE_SECURITY_PASSWORD_WARNING       32 /* Password warning */
#define VSCP_TYPE_SECURITY_PASSWORD_ERROR         33 /* Password error */
#define VSCP_TYPE_SECURITY_GAS_SENSOR             34 /* Gas */
#define VSCP_TYPE_SECURITY_IN_MOTION_DETECTED     35 /* In motion */
#define VSCP_TYPE_SECURITY_NOT_IN_MOTION_DETECTED 36 /* Not in motion */
#define VSCP_TYPE_SECURITY_VIBRATION_DETECTED     37 /* Vibration */
#define VSCP_CLASS1_MEASUREMENT (10)                 /* Measurement */

#define VSCP_TYPE_MEASUREMENT_GENERAL          0     /* General event */
#define VSCP_TYPE_MEASUREMENT_TEMPERATURE      1     /* Temperature */
#define VSCP_TYPE_MEASUREMENT_HUMIDITY         2     /* Temperature */
#define VSCP_TYPE_MEASUREMENT_PRESSURE         3     /* Pressure */
#define VSCP_TYPE_MEASUREMENT_ELECTRIC_CURRENT 4     /* Electric Current */
#define VSCP_TYPE_MEASUREMENT_ELECTRIC_VOLTAGE 5     /* Electric Current */
#define VSCP_TYPE_MEASUREMENT_LIGHT_INTENSITY  6     /* Luminous Intensity (Intensity of light) */
#define VSCP_TYPE_MEASUREMENT_FREQUENCY        7     /* Frequency */
#define VSCP_TYPE_MEASUREMENT_ENERGY           8     /* Energy */
#define VSCP_TYPE_MEASUREMENT_VOLUME           9     /* Volume */
#define VSCP_TYPE_MEASUREMENT_SOUND_INTENSITY  10    /* Sound intensity */
#define VSCP_TYPE_MEASUREMENT_POSITION         11    /* Position WGS 84 */
#define VSCP_TYPE_MEASUREMENT_SPEED            12    /* Speed */
#define VSCP_TYPE_MEASUREMENT_ACCELERATION     13    /* Acceleration */
#define VSCP_TYPE_MEASUREMENT_SOUND_LEVEL      14    /* Sound level */

#define VSCP_CLASS1_INFORMATION (20)                 /* Information */

#define VSCP_TYPE_INFORMATION_GENERAL         0      /* General event */
#define VSCP_TYPE_INFORMATION_BUTTON          1      /* Button */
#define VSCP_TYPE_INFORMATION_ON              3      /* On */
#define VSCP_TYPE_INFORMATION_OFF             4      /* Off */
#define VSCP_TYPE_INFORMATION_OPENED          7      /* Opened */
#define VSCP_TYPE_INFORMATION_CLOSED          8      /* Closed */
#define VSCP_TYPE_INFORMATION_NODE_HEARTBEAT  9      /* Node Heartbeat */
#define VSCP_TYPE_INFORMATION_BELOW_LIMIT     10     /* Below limit */
#define VSCP_TYPE_INFORMATION_ABOVE_LIMIT     11     /* Above limit */
#define VSCP_TYPE_INFORMATION_STOP            24     /* Stop */
#define VSCP_TYPE_INFORMATION_START           25     /* Start */
#define VSCP_TYPE_INFORMATION_RESET_COMPLETED 26     /* ResetCompleted */

#define VSCP_CLASS1_CONTROL (30)                     /* Control */

#define VSCP_TYPE_CONTROL_ALL_LAMPS             2    /* (All) Lamp(s) on/off */
#define VSCP_TYPE_CONTROL_OPEN                  3    /* Open */
#define VSCP_TYPE_CONTROL_CLOSE                 4    /* Close */
#define VSCP_TYPE_CONTROL_TURNON                5    /* TurnOn */
#define VSCP_TYPE_CONTROL_TURNOFF               6    /* TurnOff */
#define VSCP_TYPE_CONTROL_RESET                 9    /* Reset */
#define VSCP_TYPE_CONTROL_DIM_LAMPS             20   /* Dim lamp(s) */
#define VSCP_TYPE_CONTROL_CHANGE_CHANNEL        21   /* Change Channel */
#define VSCP_TYPE_CONTROL_CHANGE_LEVEL          22   /* Change Level */
#define VSCP_TYPE_CONTROL_TIME_SYNC             26   /* Time Sync */
#define VSCP_TYPE_CONTROL_LOCK                  42   /* Lock */
#define VSCP_TYPE_CONTROL_UNLOCK                43   /* Unlock */
#define VSCP_TYPE_CONTROL_SET_SECURITY_LEVEL    47   /* Set security level */
#define VSCP_TYPE_CONTROL_SET_SECURITY_PIN      48   /* Set security pin */
#define VSCP_TYPE_CONTROL_SET_SECURITY_PASSWORD 49   /* Set security password */
#define VSCP_TYPE_CONTROL_SET_SECURITY_TOKEN    50   /* Set security token */

#define VSCP_CLASS1_DIAGNOSTIC (506)                 /* Diagnostic */

/*  CLASS1.DIAGNOSTIC = 506  -  Diagnostic */
#define VSCP_TYPE_DIAGNOSTIC_GENERAL       0    /* General event */
#define VSCP_TYPE_DIAGNOSTIC_OVERVOLTAGE   1    /* Overvoltage */
#define VSCP_TYPE_DIAGNOSTIC_UNDERVOLTAGE  2    /* Undervoltage */
#define VSCP_TYPE_DIAGNOSTIC_VBUS_LOW      3    /* USB VBUS low */
#define VSCP_TYPE_DIAGNOSTIC_BATTERY_LOW   4    /* Battery voltage low */
#define VSCP_TYPE_DIAGNOSTIC_BATTERY_FULL  5    /* Battery full voltage */
#define VSCP_TYPE_DIAGNOSTIC_BATTERY_ERROR 6    /* Battery error */
#define VSCP_TYPE_DIAGNOSTIC_BATTERY_OK    7    /* Battery OK */
#define VSCP_TYPE_DIAGNOSTIC_OVERCURRENT   8    /* Over current */

#define VSCP_CLASS2_LEVEL1_INFORMATION1 (532)   /* Class2 Level 1I Information */

#define VSCP2_TYPE_INFORMATION_GENERAL        0 /* General event */
#define VSCP2_TYPE_INFORMATION_TOKEN_ACTIVITY 1 /* Token Activity */
#define VSCP2_TYPE_INFORMATION_HEART_BEAT     2 /* Level II Node Heartbeat */

#ifdef __cplusplus
}
#endif

#endif /* _VSCP_H_ */
