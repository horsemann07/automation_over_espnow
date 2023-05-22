#ifndef ALPHA_H
#define ALPHA_H

#ifdef __cplusplus
extern "C" {
#endif

// alpha node states
typedef enum {
    NODE_STATE_IDLE,         /*!  Standard working state*/
    NODE_STATE_VIRGIN,       /*!  Node is uninitialized (needs provisioning)*/
    NODE_STATE_KEY_EXCHANGE, /*!  sec initiated*/
    NODE_STATE_PROVISIONING, /*!  Ble-Wifi provisioning in progress*/
    NODE_STATE_OTA,          /*!  OTA update in progress*/
    NODE_STATE_LOGGING,      /*!  Logging update in progress*/
    NODE_STATE_DEBUGGING,    /*!  Debugging in progress*/
    NODE_STATE_UPDATE,       /*!  Node update in progress*/
    NODE_STATE_MAX
} node_state_t;

typedef enum {
    ALPHA_MODE_NONE,   /*!< No log */
    ALPHA_MODE_ESPNOW, /*!< Standard ESPNOW */
    ALPHA_MODE_BLE,    /*!< BLE */
    ALPHA_MODE_UDP,    /*!< UDP */
    ALPHA_MODE_TCP,    /*!< TCP */
    ALPHA_MODE_HTTP,   /*!< HTTP */
    ALPHA_MODE_MQTT,   /*!< MQTT */
    ALPHA_MODE_VSCP,   /*!< VSCP */
    // ALPHA_MODE_ESPNOW /*!< ESPNOW */
} alpha_mode_t;

#define ALPHA

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* ALPHA_H */