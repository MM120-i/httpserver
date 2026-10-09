#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include "../http/parser.h"

struct HttpRequest {
    std::string method;
    std::string target;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
};

namespace detail {
    inline std::string to_lower(std::string_view s){
        std::string out(s);

        for(char &c : out)
            if(c >= 'A' && c <= 'Z')
                c = static_cast<char>(c + ('a' - 'A'));
        
        return out;
    }
}

inline std::optional<HttpRequest> parse_http(std::string_view raw, http_parse_error_t *err = nullptr){
    http_request_t c_req;
    const http_parse_error_t rc = http_parse(raw.data(), raw.size(), &c_req);

    if(err != nullptr)
        *err = rc;

    if(rc != HTTP_PARSE_OK)
        return std::nullopt;

    HttpRequest req;

    req.method.assign(c_req.method, c_req.method_len);
    req.target.assign(c_req.target, c_req.target_len);
    req.version.assign(c_req.version, c_req.version_len);

    for(size_t i = 0; i < c_req.header_count; i++){
        const http_header_t *h = &c_req.headers[i];
        req.headers[detail::to_lower(std::string_view(h->name, h->name_len))] = std::string(h->value, h->value_len);
    }

    if(c_req.body != nullptr)
        req.body.assign(c_req.body, c_req.body_len);

    return req;
}