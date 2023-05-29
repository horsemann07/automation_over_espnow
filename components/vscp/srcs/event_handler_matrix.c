
#include "sys/queue.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "esp_err.h"

#include "event_handler_matrix.h"

/**
 * @brief Structure representing a VSCP class and its corresponding type handler.
 */
typedef struct vscp_class_type
{
    uint16_t vscp_type;                  /**< VSCP type */
    bool enable;                         /**< Flag indicating if the type handler is enabled */
    vscp_event_handler_t handler;        /**< Event handler function */
    LIST_ENTRY(vscp_class_type) entries; /**< List entry */
} vscp_class_type_t;

/**
 * @brief Structure representing the decision matrix entry.
 */
typedef struct event_handler_matrix
{
    uint16_t vscp_class;                                            /**< VSCP class */
    LIST_HEAD(vscp_class_type_list, vscp_class_type) type_handlers; /**< List of type handlers for the class */
    LIST_ENTRY(event_handler_matrix) entries;                       /**< List entry */
} event_handler_matrix_t;

/**
 * @brief Define a structure to store the head of the event handler matrix linked list.
 */
typedef struct
{
    LIST_HEAD(event_handler_matrix_list, event_handler_matrix) head; /**< Head of the linked list */
    size_t size;                                                     /**< Size of the linked list */
} event_handler_matrix_head_t;

/**
 * @brief Define a dynamic array to store the event handler matrix.
 */
static event_handler_matrix_head_t event_handler_matrix = { LIST_HEAD_INITIALIZER(event_handler_matrix.head), 0 };

/**
 * @brief Initial capacity of the event handler matrix.
 */
const size_t INITIAL_CAPACITY = 10;

// Search for an event handler in the matrix
vscp_class_type_t *search_event_handler_matrix(uint16_t class, uint16_t type)
{
    event_handler_matrix_t *matrix;
    LIST_FOREACH(matrix, &event_handler_matrix.head, entries)
    {
        vscp_class_type_t *type_handler;
        LIST_FOREACH(type_handler, &matrix->type_handlers, entries)
        {
            if (type_handler->vscp_type == type)
            {
                return type_handler;
            }
        }
    }

    return NULL;
}

// Register an event handler
esp_err_t vscp_register_event_handler(uint16_t class, uint16_t type, vscp_event_handler_t callback)
{
    // Check if the event handler already exists in the matrix
    vscp_class_type_t *existing_handler = search_event_handler_matrix(class, type);
    if (existing_handler != NULL)
    {
        // Event handler already registered
        return ESP_ERR_INVALID_ARG;
    }

    event_handler_matrix_t *matrix;
    LIST_FOREACH(matrix, &event_handler_matrix.head, entries)
    {
        if (matrix->vscp_class == class)
        {
            // Add the new type handler entry to the existing matrix entry
            vscp_class_type_t *type_handler = malloc(sizeof(vscp_class_type_t));
            if (type_handler == NULL)
            {
                // Failed to allocate memory
                return ESP_ERR_NO_MEM;
            }
            type_handler->vscp_type = type;
            type_handler->enable = true;
            type_handler->handler = callback;
            LIST_INSERT_HEAD(&matrix->type_handlers, type_handler, entries);

            // Increment the size
            event_handler_matrix.size++;

            return ESP_OK;
        }
    }

    // Create a new matrix entry
    matrix = malloc(sizeof(event_handler_matrix_t));
    if (matrix == NULL)
    {
        // Failed to allocate memory
        return ESP_ERR_NO_MEM;
    }
    matrix->vscp_class = class;
    LIST_INIT(&matrix->type_handlers);
    vscp_class_type_t *type_handler = malloc(sizeof(vscp_class_type_t));
    if (type_handler == NULL)
    {
        // Failed to allocate memory
        free(matrix);
        return ESP_ERR_NO_MEM;
    }
    type_handler->vscp_type = type;
    type_handler->enable = true;
    type_handler->handler = callback;
    LIST_INSERT_HEAD(&matrix->type_handlers, type_handler, entries);
    LIST_INSERT_HEAD(&event_handler_matrix.head, matrix, entries);

    // Increment the size
    event_handler_matrix.size++;

    return ESP_OK;
}

// Unregister an event handler
esp_err_t vscp_unregister_event_handler(uint16_t class, uint16_t type)
{
    event_handler_matrix_t *matrix;
    LIST_FOREACH(matrix, &event_handler_matrix.head, entries)
    {
        if (matrix->vscp_class == class)
        {
            vscp_class_type_t *type_handler;
            LIST_FOREACH(type_handler, &matrix->type_handlers, entries)
            {
                if (type_handler->vscp_type == type)
                {
                    // Remove the type handler entry from the matrix entry
                    LIST_REMOVE(type_handler, entries);
                    free(type_handler);

                    // Decrement the size
                    event_handler_matrix.size--;

                    if (LIST_EMPTY(&matrix->type_handlers))
                    {
                        // Remove the matrix entry if it has no type handlers
                        LIST_REMOVE(matrix, entries);
                        free(matrix);
                    }

                    return ESP_OK;
                }
            }
        }
    }

    return ESP_ERR_NOT_FOUND;
}

// Get an event handler for a given class and type
esp_err_t vscp_get_event_handler(uint16_t class, uint16_t type, vscp_event_handler_t *callback)
{
    vscp_class_type_t *type_handler = search_event_handler_matrix(class, type);
    if (type_handler != NULL)
    {
        if (!type_handler->enable)
        {
            return ESP_ERR_NOT_SUPPORTED; // Handler is not enabled
        }

        // Set the callback function
        *callback = type_handler->handler;

        return ESP_OK;
    }

    return ESP_ERR_NOT_FOUND;
}

size_t vscp_get_num_register_event_handler(void)
{
    return event_handler_matrix.size;
}