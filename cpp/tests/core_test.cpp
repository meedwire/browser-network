#include <iostream>
#include <cassert>
#include "../core/HttpRequest.h"
#include "../core/HttpResponse.h"

using namespace browser_network;

void test_request() {
    HttpRequest req("https://example.com", HttpMethod::GET);
    req.addHeader("User-Agent", "BrowserNetworkLib/1.0");
    
    assert(req.getUrl() == "https://example.com");
    assert(req.getMethod() == HttpMethod::GET);
    assert(req.getHeaders().at("User-Agent") == "BrowserNetworkLib/1.0");
    
    std::cout << "test_request PASSED" << std::endl;
}

void test_response() {
    HttpResponse res(200, "OK");
    res.setBody(ByteBuffer{'H', 'e', 'l', 'l', 'o'});
    
    assert(res.getStatus().code == 200);
    assert(res.getBody().size() == 5);
    
    std::cout << "test_response PASSED" << std::endl;
}

int main() {
    test_request();
    test_response();
    std::cout << "All Core Tests PASSED" << std::endl;
    return 0;
}
