#pragma once

#include <functional>
#include <string>
#include <unordered_map>

#include "http/request.hpp"
#include "http/response.hpp"

using Handler = std::function<HttpResponse(const HttpRequest &)>;

class Router {
private:
    std::unordered_map<std::string, Handler> get_handlers;
    std::unordered_map<std::string, Handler> post_handlers;

public:
    void get(const std::string &target, Handler handler);
    void post(const std::string &target, Handler handler);

    [[nodiscard]] HttpResponse route(const HttpRequest &req) const;
};