#pragma once

#include "http/parser.h"
#include <string>
#include <unordered_map>

struct HttpResponse {
    int status;
    std::string body;
    std::string content_type = "text/plain";
    std::unordered_map<std::string, std::string> headers;
};

inline std::string reason_phrase(int status_code){
    switch (status_code) {
        case 200: return "OK";
        case 201: return "Created";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 413: return "Content Too Large";
        case 500: return "Internal Server Error";
        default: return "Unknown";
    }
}

// This will grow later in phase 5
inline std::string serialize_response(const HttpResponse &response){
    std::string out;
    out += "HTTP/1.1 " + std::to_string(response.status) + " " + reason_phrase(response.status) + "\r\n";
    out += "Content-Length: " + std::to_string(response.body.size()) + "\r\n";
    out += "Content-Type: " + response.content_type + "\r\n";

    for(const auto &[name, value] : response.headers)
        out.append(name).append(": ").append(value).append("\r\n");

    out += "Connection: close\r\n\r\n";
    out += response.body;

    return out;
}

// Parse error -> status code
inline int http_status_for_error(http_parse_error_t error){
    switch (error) {
        case HTTP_PARSE_BODY_TOO_LARGE:
        case HTTP_PARSE_REQUEST_TOO_LARGE:
        case HTTP_PARSE_HEADER_TOO_LARGE:
        case HTTP_PARSE_URI_TOO_LONG:
            return 413;
        default:
            return 400;
    }
}