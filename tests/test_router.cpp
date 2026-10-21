#include <catch2/catch_test_macros.hpp>

#include "http/request.hpp"
#include "http/response.hpp"
#include "routing/router.hpp"

namespace {
    HttpRequest make_req(const std::string &method, const std::string &target, const std::string &body = ""){
        HttpRequest req;

        req.method = method;
        req.target = target;
        req.version = "HTTP/1.1";
        req.body = body;

        return req;
    }

    Router make_router(){
        Router router;

        router.get("/", [](const HttpRequest &){
            return make_response(200, "home");
        });

        router.get("/users", [](const HttpRequest &){
            return make_response(200, "[]", "application/json");
        });

        router.post("/users", [](const HttpRequest &request){
            return make_response(201, request.body, "application/json");
        });

        return router;
    }
}

TEST_CASE("exact routes dispatch", "[router]") {
    const Router router = make_router();

    CHECK(router.route(make_req("GET", "/")).status == 200);
    CHECK(router.route(make_req("GET", "/")).body == "home");
    CHECK(router.route(make_req("GET", "/users")).status == 200);

    const HttpResponse created = router.route(make_req("POST", "/users", R"({"name":"ada"})"));

    CHECK(created.status == 201);
    CHECK(created.body == "{\"name\":\"ada\"}");  
}

TEST_CASE("unknown target is 404", "[router]") {
    const Router router = make_router();
    const HttpResponse res = router.route(make_req("GET", "/nope"));
    CHECK(res.status == 404);
}

TEST_CASE("known target, wrong method is 405 with Allow", "[router]") {
    const Router router = make_router();
    const HttpResponse res = router.route(make_req("POST", "/"));

    CHECK(res.status == 405);
    CHECK(res.headers.at("Allow") == "GET");

    const HttpResponse both = router.route(make_req("DELETE", "/users"));
    
    CHECK(both.status == 405);
    CHECK(both.headers.at("Allow") == "GET, POST");
}

TEST_CASE("serialize_response rejects header injection", "[router]") {
    HttpResponse bad_value = make_response(200, "hi");
    bad_value.headers["X-Evil"] = "v\r\nInjected: yes";
    CHECK(serialize_response(bad_value).starts_with("HTTP/1.1 500"));

    HttpResponse bad_name = make_response(200, "hi");
    bad_name.headers["X-Evil\r\nInjected"] = "v";
    CHECK(serialize_response(bad_name).starts_with("HTTP/1.1 500"));

    HttpResponse bad_type = make_response(200, "hi");
    bad_type.headers["Content-Type"] = "text/plain\nX: y";
    CHECK(serialize_response(bad_type).starts_with("HTTP/1.1 500"));

    CHECK(serialize_response(make_response(200, "hi")).starts_with("HTTP/1.1 200"));
}

TEST_CASE("serialize_response frames correctly", "[router]") {
    const std::string raw = serialize_response(make_response(200, "hi"));

    CHECK(raw.starts_with("HTTP/1.1 200 OK\r\n"));
    CHECK(raw.find("Date: ") != std::string::npos);
    CHECK(raw.find(" GMT\r\n") != std::string::npos);
    CHECK(raw.find("Server: httpserver/0.1\r\n") != std::string::npos);
    CHECK(raw.find("Content-Type: text/plain\r\n") != std::string::npos);
    CHECK(raw.find("Content-Length: 2\r\n") != std::string::npos);
    CHECK(raw.find("Connection: close\r\n") != std::string::npos);
    CHECK(raw.ends_with("\r\n\r\nhi"));
}

TEST_CASE("reserved headers are computed by the serializer", "[router]"){
    HttpResponse response = make_response(200, "hi");

    response.headers["Content-Length"] = "999";
    response.headers["Content-Type"] = "application/json";

    const std::string raw = serialize_response(response);

    CHECK(raw.find("Content-Length: 2\r\n") != std::string::npos);
    CHECK(raw.find("Content-Length: 999") == std::string::npos);
    CHECK(raw.find("Content-Type: application/json\r\n") != std::string::npos);
    CHECK(raw.find("Content-Type: text/plain") == std::string::npos);
}
