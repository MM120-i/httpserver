#include "routing/router.hpp"
#include "http/response.hpp"
#include <string>
#include <unordered_map>

void Router::get(const std::string &target, Handler handler){
    get_handlers[target] = std::move(handler);
}

void Router::post(const std::string &target, Handler handler){
    post_handlers[target] = std::move(handler);
}

HttpResponse Router::route(const HttpRequest &req) const {
    const std::unordered_map<std::string, Handler> *table = nullptr;

    if(req.method == "GET")
        table = &get_handlers;
    else if(req.method == "POST")
        table = &post_handlers;

    if(table != nullptr){
        const auto it = table->find(req.target);

        if(it != table->end())
            return it->second(req);
    }

    std::string allow;

    if(get_handlers.contains(req.target))
        allow = "GET";

    if(post_handlers.contains(req.target))
        allow += allow.empty() ? "POST" : ", POST";

    if(!allow.empty()){
        HttpResponse res = make_response(405, reason_phrase(405) + "\n");
        res.headers["Allow"] = allow;
        return res;
    }

    return make_response(404, reason_phrase(404) + "\n");
}

