#pragma once

#include "http/parser.h"

#include <string>
#include <unordered_map>

struct HttpResponse {
    int status;
    std::string body;
    std::unordered_map<std::string, std::string> headers = {};
};

std::string reason_phrase(int status_code);
HttpResponse make_response(int status, std::string body, std::string content_type = "text/plain");
std::string serialize_response(const HttpResponse &response);
int http_status_for_error(http_parse_error_t error);
