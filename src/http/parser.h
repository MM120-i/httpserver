#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HTTP_MAX_REQUEST_SIZE 65536
#define HTTP_MAX_HEADER_SIZE 8192
#define HTTP_MAX_HEADERS 100
#define HTTP_MAX_BODY_SIZE 1048576
#define HTTP_MAX_URI_LENGTH 2048
#define HTTP_MAX_METHOD_LENGTH 16

typedef enum http_parse_error {
    HTTP_PARSE_OK = 0,
    HTTP_PARSE_EMPTY,
    HTTP_PARSE_INCOMPLETE,  
    HTTP_PARSE_BAD_REQUEST_LINE,
    HTTP_PARSE_BAD_VERSION,
    HTTP_PARSE_BAD_HEADER,
    HTTP_PARSE_TOO_MANY_HEADERS,
    HTTP_PARSE_HEADER_TOO_LARGE,
    HTTP_PARSE_URI_TOO_LONG,
    HTTP_PARSE_BAD_CONTENT_LENGTH,
    HTTP_PARSE_BODY_TOO_LARGE,
    HTTP_PARSE_REQUEST_TOO_LARGE
} http_parse_error_t;

typedef struct http_header {
    const char *name;
    size_t name_len;
    const char *value;
    size_t value_len;
} http_header_t;

typedef struct http_request {
    const char *method;
    size_t method_len;
    const char *target;
    size_t target_len;
    const char *version;
    size_t version_len;
    http_header_t headers[HTTP_MAX_HEADERS];
    size_t header_count;
    const char *body;
    size_t body_len;
} http_request_t;

http_parse_error_t http_parse(const char *buf, size_t len, http_request_t *out);
const http_header_t *http_header_get(const struct http_request *req, const char *name);

#ifdef __cplusplus
}
#endif
