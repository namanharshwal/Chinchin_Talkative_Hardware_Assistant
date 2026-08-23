#pragma once
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t mcp_router_init(void);
esp_err_t mcp_router_process_msg(const char *json_msg);

#ifdef __cplusplus
}
#endif
