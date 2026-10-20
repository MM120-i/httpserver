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
            return HttpResponse {
                .status = 200,
                .body = "home",
                .content_type = "text/plain",
            };
        });

        router.get("/users", [](const HttpRequest &){
            return HttpResponse {
                .status = 200,
                .body = "[]",
                .content_type = "application/json",
            };
        });

        router.post("/users", [](const HttpRequest &req) {
            return HttpResponse {
                .status = 201,
                .body = req.body,
                .content_type = "application/json",
            };
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
    HttpResponse bad_value{200, "hi", "text/plain"};
    bad_value.headers["X-Evil"] = "v\r\nInjected: yes";

    CHECK(serialize_response(bad_value).starts_with("HTTP/1.1 500"));

    HttpResponse bad_name{200, "hi", "text/plain"};
    bad_name.headers["X-Evil\r\nInjected"] = "v";

    CHECK(serialize_response(bad_name).starts_with("HTTP/1.1 500"));

    HttpResponse bad_type{200, "hi", "text/plain\nX: y"};

    CHECK(serialize_response(bad_type).starts_with("HTTP/1.1 500"));

    HttpResponse clean{200, "hi", "text/plain"};

    CHECK(serialize_response(clean).starts_with("HTTP/1.1 200"));
}

TEST_CASE("serialize_response frames correctly", "[router]") {
    const std::string raw = serialize_response(HttpResponse{.status=200, .body="hi", .content_type="text/plain"});
    CHECK(raw == "HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Type: text/plain\r\n" "Connection: close\r\n\r\nhi");
}
