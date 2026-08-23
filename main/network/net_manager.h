#pragma once
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NET_TYPE_WIFI,
    NET_TYPE_4G,
    NET_TYPE_ETHERNET,
    NET_TYPE_DISCONNECTED
} net_type_t;

esp_err_t net_manager_init(void);
net_type_t net_manager_get_current_type(void);

#ifdef __cplusplus
}
#endif
