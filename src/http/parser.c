#include "../http/parser.h"

#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define EQUAL 1
#define NOT_EQUAL 0

static const char *find_crlf(const char *p, const char *end){
    while(p + 1 < end){
        if(p[0] == '\r' && p[1] == '\n')
            return p;

        p++;
    }

    return NULL;
}

static int ascii_case_equal(const char *a, size_t a_len, const char *b){
    const size_t b_len = strlen(b);

    if(a_len != b_len)
        return NOT_EQUAL;

    for(size_t i = 0; i < a_len; i++)
        if(tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
            return NOT_EQUAL;

    return EQUAL;
}

static http_parse_error_t parse_request_line(const char *buf, const char *end, http_request_t *out, const char **next){
    const char *line_end = find_crlf(buf, end);

    if(line_end == NULL)
        return HTTP_PARSE_INCOMPLETE;

    const char *sp1 = memchr(buf, ' ', (size_t)(line_end - buf));
    const char *sp2 = (sp1 != NULL) ? memchr(sp1 + 1, ' ', (size_t)(line_end - (sp1 + 1))) : NULL;

    if(sp1 == NULL || sp2 == NULL)
        return HTTP_PARSE_BAD_REQUEST_LINE;

    if(memchr(sp2 + 1, ' ', (size_t)(line_end - (sp2 + 1))) != NULL)
        return HTTP_PARSE_BAD_REQUEST_LINE;

    out->method = buf;
    out->method_len = (size_t)(sp1 - buf);
    out->target = sp1 + 1;
    out->target_len = (size_t)(sp2 - (sp1 + 1));
    out->version = sp2 + 1;
    out->version_len = (size_t)(line_end - (sp2 + 1));

    if(out->method_len == 0 || out->target_len == 0 || out->version_len == 0)
        return HTTP_PARSE_BAD_REQUEST_LINE;

    if(out->method_len > HTTP_MAX_METHOD_LENGTH)
        return HTTP_PARSE_BAD_REQUEST_LINE;

    if(out->target_len > HTTP_MAX_URI_LENGTH)
        return HTTP_PARSE_URI_TOO_LONG;

    if(out->target[0] != '/')
        return HTTP_PARSE_BAD_REQUEST_LINE;

    static const char k_version[] = "HTTP/1.1";

    if(out->version_len != sizeof(k_version) - 1 || memcmp(out->version, k_version, sizeof(k_version) - 1) != 0)
        return HTTP_PARSE_BAD_VERSION;

    *next = line_end + 2;
    return HTTP_PARSE_OK;
}

// Parses header lines until the empty line. On success *next is the body start.
static http_parse_error_t parse_headers(const char *p, const char *end, http_request_t *out, const char **next){
    while(p < end){
        const char *h_end = find_crlf(p, end);

        if(h_end == NULL)
            return HTTP_PARSE_INCOMPLETE;

        if(h_end == p){
            *next = p + 2;
            return HTTP_PARSE_OK;
        }

        const size_t h_len = (size_t)(h_end - p);

        if(h_len > HTTP_MAX_HEADER_SIZE)
            return HTTP_PARSE_HEADER_TOO_LARGE;

        if(out->header_count >= HTTP_MAX_HEADERS)
            return HTTP_PARSE_TOO_MANY_HEADERS;

        const char *colon = memchr(p, ':', h_len);

        if(colon == NULL || colon == p)
            return HTTP_PARSE_BAD_HEADER;

        if(*(colon - 1) == ' ' || *(colon - 1) == '\t')
            return HTTP_PARSE_BAD_HEADER;

        const char *v = colon + 1;

        while(v < h_end && (*v == ' ' || *v == '\t'))
            v++;

        const char *v_end = h_end;

        while(v_end > v && (*(v_end - 1) == ' ' || *(v_end - 1) == '\t'))
            v_end--;

        http_header_t *h = &out->headers[out->header_count++];
        h->name = p;
        h->name_len = (size_t)(colon - p);
        h->value = v;
        h->value_len = (size_t)(v_end - v);
        p = h_end + 2;
    }

    return HTTP_PARSE_INCOMPLETE; 
}

// The body length comes entirely from Content-Length.
static http_parse_error_t parse_body(const char *p, const char *end, http_request_t *out){
    const http_header_t *cl = http_header_get(out, "Content-Length");

    if(cl == NULL)
        return HTTP_PARSE_OK;

    if(cl->value_len == 0)
        return HTTP_PARSE_BAD_CONTENT_LENGTH;

    size_t content_length = 0;

    for(size_t i = 0; i < cl->value_len; i++){
        const unsigned char c = (unsigned char)cl->value[i];

        if(c < '0' || c > '9')
            return HTTP_PARSE_BAD_CONTENT_LENGTH;

        content_length = content_length * 10 + (size_t)(c - '0');

        if(content_length > HTTP_MAX_BODY_SIZE)
            return HTTP_PARSE_BODY_TOO_LARGE;
    }

    if((size_t)(end - p) < content_length)
        return HTTP_PARSE_INCOMPLETE;

    out->body = (content_length == 0) ? NULL : p;
    out->body_len = content_length;

    return HTTP_PARSE_OK;
}

http_parse_error_t http_parse(const char *buf, size_t len, http_request_t *out){
    if(buf == NULL || out == NULL || len == 0)
        return HTTP_PARSE_EMPTY;

    if(len > HTTP_MAX_REQUEST_SIZE)
        return HTTP_PARSE_REQUEST_TOO_LARGE;

    memset(out, 0, sizeof(*out));
    const char *end = buf + len;
    const char *next = NULL;

    http_parse_error_t err = parse_request_line(buf, end, out, &next);
    if(err != HTTP_PARSE_OK)
        return err;

    err = parse_headers(next, end, out, &next);
    if(err != HTTP_PARSE_OK)
        return err;

    return parse_body(next, end, out);
}

const http_header_t *http_header_get(const struct http_request *req, const char *name){
    if(req == NULL || name == NULL)
        return NULL;

    for(size_t i = 0; i < req->header_count; i++)
        if(ascii_case_equal(req->headers[i].name, req->headers[i].name_len, name))
            return &req->headers[i];

    return NULL;
}
