#include "http/response.hpp"

#include <cctype>
#include <cstddef>
#include <ctime>
#include <string>
#include <string_view>
#include <utility>

namespace {
    constexpr const char *k_server_name = "httpserver/0.1";
    constexpr const char *k_default_content_type = "text/plain";

    bool has_crlf(std::string_view text) {
        return text.find('\r') != std::string_view::npos || text.find('\n') != std::string_view::npos;
    }

    bool header_name_equals(std::string_view a, std::string_view b){
        if(a.size() != b.size())
            return false;

        for(size_t i = 0; i < a.size(); i++){
            const auto ca = static_cast<unsigned char>(a[i]);
            const auto cb = static_cast<unsigned char>(b[i]);

            if(std::tolower(ca) != std::tolower(cb))
                return false;
        }

        return true;
    }

    bool is_reserved_header(std::string_view name){
        return header_name_equals(name, "Content-Type") ||
            header_name_equals(name, "Content-Length") ||
            header_name_equals(name, "Connection") ||
            header_name_equals(name, "Date") ||
            header_name_equals(name, "Server");
    }

    std::string http_date(){
        const time_t now = time(nullptr);
        tm tm{};
        gmtime_r(&now, &tm);
        char buf[32] = {};
        strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", &tm);
        return buf;
    }
}

std::string reason_phrase(int status_code) {
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

HttpResponse make_response(int status, std::string body, std::string content_type) {
    return HttpResponse{
        .status = status,
        .body = std::move(body),
        .headers = {{"Content-Type", std::move(content_type)}},
    };
}

int http_status_for_error(http_parse_error_t error) {
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

std::string serialize_response(const HttpResponse &response){
    for(const auto &[name, value] : response.headers)
        if(has_crlf(name) || has_crlf(value))
            return serialize_response(make_response(500, "Internal Server Error\n"));
    
    std::string content_type = k_default_content_type;

    for(const auto &[name, value] : response.headers)
        if(header_name_equals(name, "Content-Type"))
            content_type = value;
    
    std::string out;

    out += "HTTP/1.1 " + std::to_string(response.status) + " " + reason_phrase(response.status) + "\r\n";
    out += "Date: " + http_date() + "\r\n";
    out += "Server: ";
    out += k_server_name;
    out += "\r\n";
    out += "Content-Type: " + content_type + "\r\n";
    out += "Content-Length: " + std::to_string(response.body.size()) + "\r\n";
    out += "Connection: close\r\n";
    
    for(const auto &[name, value] : response.headers){
        if(is_reserved_header(name))
            continue;

        out.append(name).append(": ").append(value).append("\r\n");
    }

    out += "\r\n";
    out += response.body;

    return out;
}
