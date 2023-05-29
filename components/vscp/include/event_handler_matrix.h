#ifndef __DECISION_MATRIX__
#define __DECISION_MATRIX__

#include <stdbool.h>
#include <stdio.h>

#include "esp_err.h"
#include "espnow_utils.h"
#include "vscp.h"

#ifdef __cpluspus
extern "C" {
#endif

/**
 * @brief Registers an event handler for a specific VSCP class and type.
 *
 * @param class    The VSCP class.
 * @param type     The VSCP type.
 * @param callback The callback function to be registered.
 * @return         ESP_OK on success, or an error code if registration fails.
 */
esp_err_t vscp_register_event_handler(uint16_t class, uint16_t type, vscp_event_handler_t callback);

/**
 * @brief Unregisters the event handler for a specific VSCP class and type.
 *
 * @param class The VSCP class.
 * @param type  The VSCP type.
 * @return      ESP_OK on success, or an error code if unregistration fails.
 */
esp_err_t vscp_unregister_event_handler(uint16_t class, uint16_t type);

/**
 * @brief Retrieves the event handler matrix for a specific VSCP class and type.
 *
 * @param class  The VSCP class.
 * @param type   The VSCP type.
 * @param matrix Pointer to a variable that will hold the event handler matrix.
 * @return       ESP_OK if the matrix is found, or ESP_ERR_NOT_FOUND if not found.
 */
esp_err_t vscp_get_event_handler(uint16_t class, uint16_t type, vscp_event_handler_t *callback);

/**
 * @brief 
 * 
 * @return size_t 
 */
size_t vscp_get_num_register_event_handler(void);
#ifdef __cplusplus
}
#endif

#endif /*__DECISION_MATRIX__ */