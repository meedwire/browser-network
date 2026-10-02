#pragma once

#include "../../ports/INetworkAdapter.h"
#include <map>
#include <functional>

namespace browser_network {

class MockNetworkAdapter : public INetworkAdapter {
public:
    using Handler = std::function<HttpResponse(const HttpRequest&)>;

    std::future<HttpResponse> send(const HttpRequest& request) override {
        // Encontra handler ou retorna 404
        if (handler_) {
            return std::async(std::launch::async, [this, request]() {
                return handler_(request);
            });
        }
        return std::async(std::launch::async, []() {
            return HttpResponse(404, "Not Found (Mock)");
        });
    }

    std::future<StreamingResponse> sendStreaming(const HttpRequest& request) override {
        // Mock streaming se necessário
        return std::async(std::launch::async, []() {
            return StreamingResponse(200, "Stream Mock", HeaderMap{}, nullptr);
        });
    }

    void setHandler(Handler h) { handler_ = h; }
    
    // Contadores para spy
    int callCount = 0;

private:
    Handler handler_;
};

} // namespace browser_network
