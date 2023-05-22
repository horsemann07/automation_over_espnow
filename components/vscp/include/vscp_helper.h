#ifndef VSCP_HELPER_H
#define VSCP_HELPER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "vscp.h"

/*  byte swapping */

#define VSCP_UINT16_SWAP_ALWAYS(val)                                                                                   \
    ((uint16_t)((((uint16_t)(val) & (uint16_t)0x00ffU) << 8) | (((uint16_t)(val) & (uint16_t)0xff00U) >> 8)))

#define VSCP_INT16_SWAP_ALWAYS(val)                                                                                    \
    ((int16_t)((((uint16_t)(val) & (uint16_t)0x00ffU) << 8) | (((uint16_t)(val) & (uint16_t)0xff00U) >> 8)))

#define VSCP_UINT32_SWAP_ALWAYS(val)                                                                                   \
    ((uint32_t)((((uint32_t)(val) & (uint32_t)0x000000ffU) << 24) | (((uint32_t)(val) & (uint32_t)0x0000ff00U) << 8)   \
                | (((uint32_t)(val) & (uint32_t)0x00ff0000U) >> 8)                                                     \
                | (((uint32_t)(val) & (uint32_t)0xff000000U) >> 24)))

#define VSCP_INT32_SWAP_ALWAYS(val)                                                                                    \
    ((int32_t)((((uint32_t)(val) & (uint32_t)0x000000ffU) << 24) | (((uint32_t)(val) & (uint32_t)0x0000ff00U) << 8)    \
               | (((uint32_t)(val) & (uint32_t)0x00ff0000U) >> 8)                                                      \
               | (((uint32_t)(val) & (uint32_t)0xff000000U) >> 24)))
/*  machine specific byte swapping */

#define VSCP_UINT64_SWAP_ALWAYS(val)                                                                                   \
    ((uint64_t)((((uint64_t)(val) & (uint64_t)(0x00000000000000ff)) << 56)                                             \
                | (((uint64_t)(val) & (uint64_t)(0x000000000000ff00)) << 40)                                           \
                | (((uint64_t)(val) & (uint64_t)(0x0000000000ff0000)) << 24)                                           \
                | (((uint64_t)(val) & (uint64_t)(0x00000000ff000000)) << 8)                                            \
                | (((uint64_t)(val) & (uint64_t)(0x000000ff00000000)) >> 8)                                            \
                | (((uint64_t)(val) & (uint64_t)(0x0000ff0000000000)) >> 24)                                           \
                | (((uint64_t)(val) & (uint64_t)(0x00ff000000000000)) >> 40)                                           \
                | (((uint64_t)(val) & (uint64_t)(0xff00000000000000)) >> 56)))

#define VSCP_INT64_SWAP_ALWAYS(val)                                                                                    \
    ((int64_t)((((uint64_t)(val) & (uint64_t)(0x00000000000000ff)) << 56)                                              \
               | (((uint64_t)(val) & (uint64_t)(0x000000000000ff00)) << 40)                                            \
               | (((uint64_t)(val) & (uint64_t)(0x0000000000ff0000)) << 24)                                            \
               | (((uint64_t)(val) & (uint64_t)(0x00000000ff000000)) << 8)                                             \
               | (((uint64_t)(val) & (uint64_t)(0x000000ff00000000)) >> 8)                                             \
               | (((uint64_t)(val) & (uint64_t)(0x0000ff0000000000)) >> 24)                                            \
               | (((uint64_t)(val) & (uint64_t)(0x00ff000000000000)) >> 40)                                            \
               | (((uint64_t)(val) & (uint64_t)(0xff00000000000000)) >> 56)))

#ifdef __BIG_ENDIAN__
#define VSCP_UINT16_SWAP_ON_BE(val) VSCP_UINT16_SWAP_ALWAYS(val)
#define VSCP_INT16_SWAP_ON_BE(val)  VSCP_INT16_SWAP_ALWAYS(val)
#define VSCP_UINT16_SWAP_ON_LE(val) (val)
#define VSCP_INT16_SWAP_ON_LE(val)  (val)
#define VSCP_UINT32_SWAP_ON_BE(val) VSCP_UINT32_SWAP_ALWAYS(val)
#define VSCP_INT32_SWAP_ON_BE(val)  VSCP_INT32_SWAP_ALWAYS(val)
#define VSCP_UINT32_SWAP_ON_LE(val) (val)
#define VSCP_INT32_SWAP_ON_LE(val)  (val)
#define VSCP_UINT64_SWAP_ON_BE(val) VSCP_UINT64_SWAP_ALWAYS(val)
#define VSCP_UINT64_SWAP_ON_LE(val) (val)
#define VSCP_INT64_SWAP_ON_BE(val)  VSCP_INT64_SWAP_ALWAYS(val)
#define VSCP_INT64_SWAP_ON_LE(val)  (val)
#else
#define VSCP_UINT16_SWAP_ON_LE(val) VSCP_UINT16_SWAP_ALWAYS(val)
#define VSCP_INT16_SWAP_ON_LE(val)  VSCP_INT16_SWAP_ALWAYS(val)
#define VSCP_UINT16_SWAP_ON_BE(val) (val)
#define VSCP_INT16_SWAP_ON_BE(val)  (val)
#define VSCP_UINT32_SWAP_ON_LE(val) VSCP_UINT32_SWAP_ALWAYS(val)
#define VSCP_INT32_SWAP_ON_LE(val)  VSCP_INT32_SWAP_ALWAYS(val)
#define VSCP_UINT32_SWAP_ON_BE(val) (val)
#define VSCP_INT32_SWAP_ON_BE(val)  (val)
#define VSCP_UINT64_SWAP_ON_LE(val) VSCP_UINT64_SWAP_ALWAYS(val)
#define VSCP_UINT64_SWAP_ON_BE(val) (val)
#define VSCP_INT64_SWAP_ON_LE(val)  VSCP_INT64_SWAP_ALWAYS(val)
#define VSCP_INT64_SWAP_ON_BE(val)  (val)
#endif

#define Swap8Bytes(val)                                                                                                \
    ((((val) >> 56) & 0x00000000000000FF) | (((val) >> 40) & 0x000000000000FF00)                                       \
        | (((val) >> 24) & 0x0000000000FF0000) | (((val) >> 8) & 0x00000000FF000000)                                   \
        | (((val) << 8) & 0x000000FF00000000) | (((val) << 24) & 0x0000FF0000000000)                                   \
        | (((val) << 40) & 0x00FF000000000000) | (((val) << 56) & 0xFF00000000000000))

esp_err_t prepare_vscp_nodes_message(void *buffer, uint8_t nickname, uint8_t priority, uint8_t guid[6], uint16_t class,
    uint16_t type, const char *data, size_t *size);

esp_err_t prepare_vscp_mqtt_message(void *buffer, size_t *size, uint8_t priority, long timestamp, uint8_t guid[6],
    uint16_t class, uint16_t type, const char *data);
#define ALPHA

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* VSCP_HELPER_H */