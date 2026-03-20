#pragma once

#include <stdbool.h>
#include <stddef.h>

// Configure base URL, authorization token, and HTTPS usage
void http_client_init(const char* base_url, const char* token, bool use_https);
const char* http_client_get_base_url(void);
const char* http_client_get_token(void);
bool http_client_is_configured(void);

// Issue HTTP requests relative to the configured base URL
bool http_client_get(const char* path, char** out, size_t* out_len);
bool http_client_post(const char* path, const char* payload);
bool http_client_put(const char* path, const char* payload);
