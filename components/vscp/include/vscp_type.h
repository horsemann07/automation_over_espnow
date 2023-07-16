

#ifndef __VSCP_TYPE__
#define __VSCP_TYPE__

#define VSCP_TYPE_MAX 255

// #define VSCP_CLASS1_PROTOCOL             (0)  /* VSCP Protocol Functionality */

#define VSCP_TYPE_PROTOCOL_GENERAL           0 /* General event. */
#define VSCP_TYPE_PROTOCOL_NEW_NODE_ONLINE   1 /* New node on line / Probe. */
#define VSCP_TYPE_PROTOCOL_SET_NICKNAME      2 /* Set nickname-ID for node. */
#define VSCP_TYPE_PROTOCOL_NICKNAME_ACCEPTED 3 /* Nickname-ID accepted. */
#define VSCP_TYPE_PROTOCOL_ENROLL            4 /* Enroll event. */
#define VSCP_TYPE_PROTOCOL_ENROLL_ACK        5 /* Enroll ack event. */
#define VSCP_TYPE_PROTOCOL_DROP_NICKNAME     7 /* Drop nickname-ID / Reset Device. */
#define VSCP_TYPE_PROTOCOL_DROP_NICKNAME_ACK 8
#define VSCP_TYPE_PROTOCOL_MAX               9

// #define VSCP_CLASS1_SECURITY (2)                       /* Security */

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

// #define VSCP_CLASS1_MEASUREMENT (10)                 /* Measurement */

#define VSCP_TYPE_MEASUREMENT_GENERAL          0  /* General event */
#define VSCP_TYPE_MEASUREMENT_TEMPERATURE      1  /* Temperature */
#define VSCP_TYPE_MEASUREMENT_HUMIDITY         2  /* Temperature */
#define VSCP_TYPE_MEASUREMENT_PRESSURE         3  /* Pressure */
#define VSCP_TYPE_MEASUREMENT_ELECTRIC_CURRENT 4  /* Electric Current */
#define VSCP_TYPE_MEASUREMENT_ELECTRIC_VOLTAGE 5  /* Electric Current */
#define VSCP_TYPE_MEASUREMENT_LIGHT_INTENSITY  6  /* Luminous Intensity (Intensity of light) */
#define VSCP_TYPE_MEASUREMENT_FREQUENCY        7  /* Frequency */
#define VSCP_TYPE_MEASUREMENT_ENERGY           8  /* Energy */
#define VSCP_TYPE_MEASUREMENT_VOLUME           9  /* Volume */
#define VSCP_TYPE_MEASUREMENT_SOUND_INTENSITY  10 /* Sound intensity */
#define VSCP_TYPE_MEASUREMENT_POSITION         11 /* Position WGS 84 */
#define VSCP_TYPE_MEASUREMENT_SPEED            12 /* Speed */
#define VSCP_TYPE_MEASUREMENT_ACCELERATION     13 /* Acceleration */
#define VSCP_TYPE_MEASUREMENT_SOUND_LEVEL      14 /* Sound level */

// #define VSCP_CLASS1_INFORMATION (20)                 /* Information */

#define VSCP_TYPE_INFORMATION_GENERAL         0  /* General event */
#define VSCP_TYPE_INFORMATION_BUTTON          1  /* Button */
#define VSCP_TYPE_INFORMATION_ON              3  /* On */
#define VSCP_TYPE_INFORMATION_OFF             4  /* Off */
#define VSCP_TYPE_INFORMATION_OPENED          7  /* Opened */
#define VSCP_TYPE_INFORMATION_CLOSED          8  /* Closed */
#define VSCP_TYPE_INFORMATION_NODE_HEARTBEAT  9  /* Node Heartbeat */
#define VSCP_TYPE_INFORMATION_BELOW_LIMIT     10 /* Below limit */
#define VSCP_TYPE_INFORMATION_ABOVE_LIMIT     11 /* Above limit */
#define VSCP_TYPE_INFORMATION_STOP            12 /* Stop */
#define VSCP_TYPE_INFORMATION_START           13 /* Start */
#define VSCP_TYPE_INFORMATION_RESET_COMPLETED 14 /* ResetCompleted */

// #define VSCP_CLASS1_CONTROL (30)                     /* Control */

#define VSCP_TYPE_CONTROL_ALL_LAMPS             1  /* (All) Lamp(s) on/off */
#define VSCP_TYPE_CONTROL_OPEN                  2  /* Open */
#define VSCP_TYPE_CONTROL_CLOSE                 3  /* Close */
#define VSCP_TYPE_CONTROL_TURNON                4  /* TurnOn */
#define VSCP_TYPE_CONTROL_TURNOFF               5  /* TurnOff */
#define VSCP_TYPE_CONTROL_RESET                 6  /* Reset */
#define VSCP_TYPE_CONTROL_DIM_LAMPS             7  /* Dim lamp(s) */
#define VSCP_TYPE_CONTROL_CHANGE_CHANNEL        8  /* Change Channel */
#define VSCP_TYPE_CONTROL_CHANGE_LEVEL          9  /* Change Level */
#define VSCP_TYPE_CONTROL_TIME_SYNC             10 /* Time Sync */
#define VSCP_TYPE_CONTROL_LOCK                  11 /* Lock */
#define VSCP_TYPE_CONTROL_UNLOCK                12 /* Unlock */
#define VSCP_TYPE_CONTROL_SET_SECURITY_LEVEL    13 /* Set security level */
#define VSCP_TYPE_CONTROL_SET_SECURITY_PIN      14 /* Set security pin */
#define VSCP_TYPE_CONTROL_SET_SECURITY_PASSWORD 15 /* Set security password */
#define VSCP_TYPE_CONTROL_SET_SECURITY_TOKEN    16 /* Set security token */

// #define VSCP_CLASS1_DIAGNOSTIC (506)                 /* Diagnostic */

/*  CLASS1.DIAGNOSTIC = 506  -  Diagnostic */
#define VSCP_TYPE_DIAGNOSTIC_GENERAL       0 /* General event */
#define VSCP_TYPE_DIAGNOSTIC_OVERVOLTAGE   1 /* Overvoltage */
#define VSCP_TYPE_DIAGNOSTIC_UNDERVOLTAGE  2 /* Undervoltage */
#define VSCP_TYPE_DIAGNOSTIC_VBUS_LOW      3 /* USB VBUS low */
#define VSCP_TYPE_DIAGNOSTIC_BATTERY_LOW   4 /* Battery voltage low */
#define VSCP_TYPE_DIAGNOSTIC_BATTERY_FULL  5 /* Battery full voltage */
#define VSCP_TYPE_DIAGNOSTIC_BATTERY_ERROR 6 /* Battery error */
#define VSCP_TYPE_DIAGNOSTIC_BATTERY_OK    7 /* Battery OK */
#define VSCP_TYPE_DIAGNOSTIC_OVERCURRENT   8 /* Over current */

// #define VSCP_CLASS2_LEVEL1_INFORMATION1 (532)   /* Class2 Level 1I Information */

#define VSCP2_TYPE_INFORMATION_GENERAL        0 /* General event */
#define VSCP2_TYPE_INFORMATION_TOKEN_ACTIVITY 1 /* Token Activity */
#define VSCP2_TYPE_INFORMATION_HEART_BEAT     2 /* Level II Node Heartbeat */

#endif                                          /* __VSCP_TYPE__ */