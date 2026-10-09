#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <string>

#include "../src/http/parser.h"
#include "../src/http/request.hpp"
namespace {
    http_parse_error_t parse_str(const std::string &raw, http_request_t *out) {
        return http_parse(raw.data(), raw.size(), out);
    }

    std::string_view view_of(const http_header_t *h, bool name) {
        return name ? std::string_view(h->name, h->name_len) : std::string_view(h->value, h->value_len);
    }
}  

TEST_CASE("valid GET parses fully", "[parser]") {
    const std::string raw = "GET /hello HTTP/1.1\r\nHost: localhost:8000\r\nConnection: keep-alive\r\n\r\n";
    http_request_t req;
    REQUIRE(parse_str(raw, &req) == HTTP_PARSE_OK);
    CHECK(std::string_view(req.method, req.method_len) == "GET");
    CHECK(std::string_view(req.target, req.target_len) == "/hello");
    CHECK(std::string_view(req.version, req.version_len) == "HTTP/1.1");
    REQUIRE(req.header_count == 2);
    CHECK(view_of(&req.headers[0], true) == "Host");
    CHECK(view_of(&req.headers[0], false) == "localhost:8000");
    CHECK(req.body == nullptr);
    CHECK(req.body_len == 0);
}

TEST_CASE("POST with Content-Length body", "[parser]") {
    const std::string raw = "POST /users HTTP/1.1\r\nHost: x\r\nContent-Length: 11\r\n\r\nhello world";
    http_request_t req;
    REQUIRE(parse_str(raw, &req) == HTTP_PARSE_OK);
    CHECK(std::string_view(req.body, req.body_len) == "hello world");
}

TEST_CASE("header lookup is case-insensitive", "[parser]") {
    const std::string raw = "GET / HTTP/1.1\r\nHOST: example\r\n\r\n";
    http_request_t req;
    REQUIRE(parse_str(raw, &req) == HTTP_PARSE_OK);
    const http_header_t *h = http_header_get(&req, "host");
    REQUIRE(h != nullptr);
    CHECK(view_of(h, false) == "example");
    CHECK(http_header_get(&req, "missing") == nullptr);
}

TEST_CASE("unknown method token still parses (router decides 405 later)", "[parser]") {
    const std::string raw = "DELETE /x HTTP/1.1\r\nHost: x\r\n\r\n";
    http_request_t req;
    CHECK(parse_str(raw, &req) == HTTP_PARSE_OK);
    CHECK(std::string_view(req.method, req.method_len) == "DELETE");
}

TEST_CASE("malformed inputs are rejected", "[parser]") {
    http_request_t req;
    CHECK(parse_str("", &req) == HTTP_PARSE_EMPTY);
    CHECK(parse_str("GET /hello\r\n\r\n", &req) == HTTP_PARSE_BAD_REQUEST_LINE);       
    CHECK(parse_str("GET /hello HTTP/2\r\n\r\n", &req) == HTTP_PARSE_BAD_VERSION);
    CHECK(parse_str("GET  /hello HTTP/1.1\r\n\r\n", &req) == HTTP_PARSE_BAD_REQUEST_LINE);  
    CHECK(parse_str("GET / HTTP/1.1\r\nNoColonHere\r\n\r\n", &req) == HTTP_PARSE_BAD_HEADER);
    CHECK(parse_str("GET / HTTP/1.1\r\nBad : value\r\n\r\n", &req) == HTTP_PARSE_BAD_HEADER);
    CHECK(parse_str("GET / HTTP/1.1\r\nHost: x\r\nContent-Length: abc\r\n\r\n", &req) == HTTP_PARSE_BAD_CONTENT_LENGTH);
}

TEST_CASE("truncated inputs report INCOMPLETE", "[parser]") {
    http_request_t req;
    CHECK(parse_str("GET /hello HTTP/1.1\r\nHost: x\r\n", &req) == HTTP_PARSE_INCOMPLETE);
    CHECK(parse_str("POST /u HTTP/1.1\r\nHost: x\r\nContent-Length: 11\r\n\r\nhello", &req) == HTTP_PARSE_INCOMPLETE);
}

TEST_CASE("resource limits hold", "[parser]") {
    http_request_t req;
    CHECK(parse_str("GET /" + std::string(2048, 'a') + " HTTP/1.1\r\n\r\n", &req) == HTTP_PARSE_URI_TOO_LONG);
    CHECK(parse_str("GET / HTTP/1.1\r\nX: " + std::string(8192, 'y') + "\r\n\r\n", &req) == HTTP_PARSE_HEADER_TOO_LARGE);
    std::string many = "GET / HTTP/1.1\r\n";

    for (size_t i = 0; i < 101; ++i) 
        many += "X-H: v\r\n";

    many += "\r\n";
    CHECK(parse_str(many, &req) == HTTP_PARSE_TOO_MANY_HEADERS);
    CHECK(parse_str("POST /u HTTP/1.1\r\nHost: x\r\nContent-Length: 1048577\r\n\r\n", &req) == HTTP_PARSE_BODY_TOO_LARGE);
}

TEST_CASE("C++ wrapper owns its data", "[parser]") {
    const char *raw = "POST /u HTTP/1.1\r\nHOST: Example\r\nContent-Length: 3\r\n\r\nabc";
    http_parse_error_t err = HTTP_PARSE_OK;
    auto req = parse_http(raw, &err);
    REQUIRE(err == HTTP_PARSE_OK);
    REQUIRE(req.has_value());
    CHECK(req->method == "POST");
    CHECK(req->headers["host"] == "Example");  
    CHECK(req->body == "abc");
    auto bad = parse_http("garbage", nullptr);
    CHECK_FALSE(bad.has_value());
}

TEST_CASE("every truncation is safe (mini-fuzz)", "[parser][fuzz]") {
    const std::string full = "POST /users HTTP/1.1\r\nHost: localhost\r\nContent-Length: 5\r\n\r\nhello";

    for (size_t cut = 0; cut < full.size(); ++cut) {
        http_request_t req;
        const http_parse_error_t rc = parse_str(full.substr(0, cut), &req);
        CHECK(rc >= HTTP_PARSE_OK);
        CHECK(rc <= HTTP_PARSE_REQUEST_TOO_LARGE);
    }
    
    http_request_t req;
    CHECK(parse_str(full, &req) == HTTP_PARSE_OK);  
}