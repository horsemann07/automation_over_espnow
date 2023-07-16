#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>

#include "esp_err.h"

typedef struct vscp_type_node
{
    uint8_t type;
    vscp_event_handler_t type_evnt_handler;
    LIST_ENTRY(vscp_type_node) entries;
} vscp_type_node_t;

typedef struct vscp_class
{
    uint8_t class;
    vscp_event_handler_t class_evnt_handler;
    size_t type_count;
    LIST_HEAD(vscp_type_list, vscp_type_node) type_list;
    LIST_ENTRY(vscp_class) entries;
} vscp_class_t;

LIST_HEAD(vscp_class_list, vscp_class) class_list;

static vscp_class_t *find_class(uint8_t class)
{
    vscp_class_t *entry;
    LIST_FOREACH(entry, &class_list, entries)
    {
        if (entry->class == class)
        {
            return entry;
        }
    }
    return NULL;
}

esp_err_t vscp_evnt_handler_register_event(uint8_t class, vscp_event_handler_t class_handler, uint8_t type,
    vscp_event_handler_t type_handler)
{
    vscp_class_t *class_entry = find_class(class);
    if (class_entry == NULL)
    {
        // Class not found, create a new class entry
        class_entry = malloc(sizeof(vscp_class_t));
        if (class_entry == NULL)
        {
            // Failed to allocate memory
            return ESP_ERR_NO_MEM;
        }
        class_entry->class = class;
        class_entry->class_evnt_handler = class_handler;
        class_entry->type_count = 0;
        LIST_INIT(&class_entry->type_list);
        LIST_INSERT_HEAD(&class_list, class_entry, entries);
    }

    if (type_handler != NULL)
    {
        // Check if the type is already registered
        vscp_type_node_t *type_entry;
        LIST_FOREACH(type_entry, &class_entry->type_list, entries)
        {
            if (type_entry->type == type)
            {
                // Type already registered, update the event handler
                type_entry->type_evnt_handler = type_handler;
                return ESP_OK;
            }
        }

        // Register the type event handler
        type_entry = malloc(sizeof(vscp_type_node_t));
        if (type_entry == NULL)
        {
            // Failed to allocate memory
            return ESP_ERR_NO_MEM;
        }
        type_entry->type = type;
        type_entry->type_evnt_handler = type_handler;
        LIST_INSERT_HEAD(&class_entry->type_list, type_entry, entries);
        class_entry->type_count++;
    }

    return ESP_OK;
}

static void unregister_type(vscp_class_t *class_entry, vscp_type_node_t *type_entry)
{
    LIST_REMOVE(type_entry, entries);
    free(type_entry);
    class_entry->type_count--;
}

esp_err_t vscp_evnt_handler_unregister_event(uint8_t class)
{
    vscp_class_t *class_entry = find_class(class);
    if (class_entry == NULL)
    {
        // Class not found, nothing to unregister
        return ESP_ERR_NOT_FOUND;
    }

    while (!LIST_EMPTY(&class_entry->type_list))
    {
        // Unregister all types for the class
        vscp_type_node_t *type_entry;
        type_entry = LIST_FIRST(&class_entry->type_list);
        unregister_type(class_entry, type_entry);
    }

    // Remove the class entry
    LIST_REMOVE(class_entry, entries);
    free(class_entry);

    return ESP_OK;
}

vscp_event_handler_t vscp_evnt_handler_get_event_handler_by_class(uint8_t class)
{
    vscp_class_t *class_entry = find_class(class);
    if (class_entry != NULL)
    {
        return class_entry->class_evnt_handler;
    }
    return NULL;
}

vscp_event_handler_t vscp_evnt_handler_get_cb_by_type(uint8_t class, uint8_t type)
{
    vscp_class_t *class_entry = find_class(class);
    if (class_entry != NULL)
    {
        vscp_type_node_t *type_entry;
        LIST_FOREACH(type_entry, &class_entry->type_list, entries)
        {
            if (type_entry->type == type)
            {
                return type_entry->type_evnt_handler;
            }
        }
    }
    return NULL;
}
// Function to return the number of registered classes
size_t vscp_evnt_handler_get_num_classes(void)
{
    int count = 0;
    struct vscp_class *class_entry;
    LIST_FOREACH(class_entry, &class_list, entries)
    {
        count++;
    }
    return count;
}

// Function to return the number of types for a specific class
size_t vscp_evnt_handler_get_num_types(uint8_t class)
{
    struct vscp_class *class_entry;
    LIST_FOREACH(class_entry, &class_list, entries)
    {
        if (class_entry->class == class)
        {
            return class_entry->type_count;
        }
    }
    return 0; // Class not found
}

esp_err_t vscp_evnt_handler_init(void)
{
    LIST_INIT(&class_list);
    return ESP_OK;
}