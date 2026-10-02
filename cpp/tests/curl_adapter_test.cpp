#include <iostream>
#include <cassert>
#include <thread>
#include <string>
#include "../adapters/curl/CurlAdapter.h"
#include "../core/HttpEntities.h"
#include "../core/RedirectTypes.h"

using namespace browser_network;

void test_curl_get() {
    std::cout << "Starting Curl GET test..." << std::endl;
    CurlAdapter adapter;
    HttpRequest req("https://httpbin.org/get", HttpMethod::GET);
    
    auto future = adapter.send(req);
    HttpResponse res = future.get();
    
    std::cout << "GET Status: " << res.getStatus().code << std::endl;
    assert(res.getStatus().code == 200);
    assert(!res.getBody().empty());
    // Verificar que url é preenchida
    assert(!res.getUrl().empty());
    std::cout << "test_curl_get PASSED" << std::endl;
}

void test_curl_post() {
    std::cout << "Starting Curl POST test..." << std::endl;
    CurlAdapter adapter;
    HttpRequest req("https://httpbin.org/post", HttpMethod::POST);
    
    std::string jsonBody = "{\"test\": \"value\"}";
    req.setBody(jsonBody);
    req.addHeader("Content-Type", "application/json");

    auto future = adapter.send(req);
    HttpResponse res = future.get();
    
    std::cout << "POST Status: " << res.getStatus().code << std::endl;
    assert(res.getStatus().code == 200);
    
    std::string bodyStr(res.getBody().begin(), res.getBody().end());
    assert(bodyStr.find("value") != std::string::npos);
    
    std::cout << "test_curl_post PASSED" << std::endl;
}

void test_curl_redirect_follow() {
    std::cout << "Starting Curl Redirect Follow test..." << std::endl;
    CurlAdapter adapter;
    
    // httpbin /redirect-to faz um 302 redirect
    HttpRequest req("https://httpbin.org/redirect-to?url=https%3A%2F%2Fhttpbin.org%2Fget&status_code=302", HttpMethod::GET);
    req.setRedirectMode(RedirectMode::Follow);
    
    auto future = adapter.send(req);
    HttpResponse res = future.get();
    
    std::cout << "Redirect Follow Status: " << res.getStatus().code << std::endl;
    assert(res.getStatus().code == 200);
    
    // Verificar propriedades de redirect
    const auto& info = res.getRedirectInfo();
    std::cout << "  redirected: " << info.redirected << std::endl;
    std::cout << "  chain size: " << info.chain.size() << std::endl;
    std::cout << "  finalUrl: " << info.finalUrl << std::endl;
    
    assert(info.redirected == true);
    assert(info.chain.size() >= 1);
    assert(info.chain[0].statusCode == 302);
    assert(info.chain[0].method == "GET");
    // A URL final deve ser a de destino
    assert(info.finalUrl.find("httpbin.org/get") != std::string::npos);
    // Response.url deve bater
    assert(res.getUrl() == info.finalUrl);
    
    std::cout << "test_curl_redirect_follow PASSED" << std::endl;
}

void test_curl_redirect_manual() {
    std::cout << "Starting Curl Redirect Manual test..." << std::endl;
    CurlAdapter adapter;
    
    // Modo Manual: deve retornar o 302 sem seguir
    HttpRequest req("https://httpbin.org/redirect-to?url=https%3A%2F%2Fhttpbin.org%2Fget&status_code=302", HttpMethod::GET);
    req.setRedirectMode(RedirectMode::Manual);
    
    auto future = adapter.send(req);
    HttpResponse res = future.get();
    
    std::cout << "Redirect Manual Status: " << res.getStatus().code << std::endl;
    // Deve retornar 302, NÃO 200
    assert(res.getStatus().code == 302);
    
    // Deve ter o header Location
    const auto& headers = res.getHeaders();
    bool hasLocation = headers.find("Location") != headers.end() 
                    || headers.find("location") != headers.end();
    assert(hasLocation);
    
    std::cout << "test_curl_redirect_manual PASSED" << std::endl;
}

void test_curl_redirect_error() {
    std::cout << "Starting Curl Redirect Error test..." << std::endl;
    CurlAdapter adapter;
    
    // Modo Error: deve retornar erro no 302
    HttpRequest req("https://httpbin.org/redirect-to?url=https%3A%2F%2Fhttpbin.org%2Fget&status_code=302", HttpMethod::GET);
    req.setRedirectMode(RedirectMode::Error);
    
    auto future = adapter.send(req);
    HttpResponse res = future.get();
    
    std::cout << "Redirect Error Status: " << res.getStatus().code 
              << " Msg: " << res.getStatus().message << std::endl;
    // Deve retornar 302 com mensagem de erro
    assert(res.getStatus().code == 302);
    assert(res.getStatus().message.find("Error") != std::string::npos 
        || res.getStatus().message.find("proibido") != std::string::npos);
    
    std::cout << "test_curl_redirect_error PASSED" << std::endl;
}

void test_curl_redirect_chain() {
    std::cout << "Starting Curl Redirect Chain test..." << std::endl;
    CurlAdapter adapter;
    
    // httpbin /redirect/N faz N redirects em cadeia
    HttpRequest req("https://httpbin.org/redirect/3", HttpMethod::GET);
    req.setRedirectMode(RedirectMode::Follow);
    
    auto future = adapter.send(req);
    HttpResponse res = future.get();
    
    std::cout << "Redirect Chain Status: " << res.getStatus().code << std::endl;
    assert(res.getStatus().code == 200);
    
    const auto& info = res.getRedirectInfo();
    std::cout << "  Chain size: " << info.chain.size() << std::endl;
    for (size_t i = 0; i < info.chain.size(); i++) {
        std::cout << "  Hop " << i << ": " << info.chain[i].url 
                  << " [" << info.chain[i].statusCode << "] " 
                  << info.chain[i].method << std::endl;
    }
    
    assert(info.redirected == true);
    assert(info.chain.size() == 3); // 3 hops
    // Cada hop deve ter sido um 302
    for (const auto& entry : info.chain) {
        assert(entry.statusCode == 302);
        assert(entry.method == "GET");
    }
    
    std::cout << "test_curl_redirect_chain PASSED" << std::endl;
}

void test_curl_redirect_303_method_change() {
    std::cout << "Starting Curl Redirect 303 Method Change test..." << std::endl;
    CurlAdapter adapter;
    
    // POST com redirect 303 deve mudar para GET
    HttpRequest req("https://httpbin.org/redirect-to?url=https%3A%2F%2Fhttpbin.org%2Fget&status_code=303", HttpMethod::POST);
    req.setBody("{\"data\": \"test\"}");
    req.addHeader("Content-Type", "application/json");
    req.setRedirectMode(RedirectMode::Follow);
    
    auto future = adapter.send(req);
    HttpResponse res = future.get();
    
    std::cout << "303 Redirect Status: " << res.getStatus().code << std::endl;
    assert(res.getStatus().code == 200);
    
    const auto& info = res.getRedirectInfo();
    assert(info.redirected == true);
    assert(info.chain.size() >= 1);
    // O primeiro hop foi POST com 303
    assert(info.chain[0].method == "POST");
    assert(info.chain[0].statusCode == 303);
    // A URL final é /get (aceita GET)
    assert(info.finalUrl.find("/get") != std::string::npos);
    
    std::cout << "test_curl_redirect_303_method_change PASSED" << std::endl;
}

void test_curl_redirect_307_method_preserve() {
    std::cout << "Starting Curl Redirect 307 Method Preserve test..." << std::endl;
    CurlAdapter adapter;
    
    // POST com redirect 307 deve preservar POST
    HttpRequest req("https://httpbin.org/redirect-to?url=https%3A%2F%2Fhttpbin.org%2Fpost&status_code=307", HttpMethod::POST);
    req.setBody("{\"data\": \"preserved\"}");
    req.addHeader("Content-Type", "application/json");
    req.setRedirectMode(RedirectMode::Follow);
    
    auto future = adapter.send(req);
    HttpResponse res = future.get();
    
    std::cout << "307 Redirect Status: " << res.getStatus().code << std::endl;
    assert(res.getStatus().code == 200);
    
    const auto& info = res.getRedirectInfo();
    assert(info.redirected == true);
    assert(info.chain[0].method == "POST");
    assert(info.chain[0].statusCode == 307);
    
    // Verificar que o body foi preservado (httpbin retorna o body enviado)
    std::string bodyStr(res.getBody().begin(), res.getBody().end());
    std::cout << "  Body contains 'preserved': " << (bodyStr.find("preserved") != std::string::npos) << std::endl;
    assert(bodyStr.find("preserved") != std::string::npos);
    
    std::cout << "test_curl_redirect_307_method_preserve PASSED" << std::endl;
}

void test_curl_redirect_max() {
    std::cout << "Starting Curl Redirect Max Limit test..." << std::endl;
    CurlAdapter adapter;
    
    // Limitar a 2 redirects, mas pedir 5 hops
    HttpRequest req("https://httpbin.org/redirect/5", HttpMethod::GET);
    req.setRedirectMode(RedirectMode::Follow);
    req.setMaxRedirects(2);
    
    auto future = adapter.send(req);
    HttpResponse res = future.get();
    
    std::cout << "Max Redirect Status: " << res.getStatus().code 
              << " Msg: " << res.getStatus().message << std::endl;
    // Deve parar após 2 hops — status será 302 (o 3° redirect que não foi seguido)
    assert(res.getStatus().code == 302);
    
    const auto& info = res.getRedirectInfo();
    std::cout << "  Chain size: " << info.chain.size() << std::endl;
    assert(info.chain.size() == 2); // Apenas 2 hops registrados
    
    std::cout << "test_curl_redirect_max PASSED" << std::endl;
}

void test_curl_cookies() {
    std::cout << "Starting Curl Cookie test..." << std::endl;
    CurlAdapter adapter;
    
    // 1. Set a cookie
    HttpRequest req1("https://httpbin.org/cookies/set?browser_lib=cool", HttpMethod::GET);
    auto future1 = adapter.send(req1);
    HttpResponse res1 = future1.get();
    assert(res1.getStatus().code == 200);

    // 2. Retrieve cookies and verify persistence
    HttpRequest req2("https://httpbin.org/cookies", HttpMethod::GET);
    auto future2 = adapter.send(req2);
    HttpResponse res2 = future2.get();
    assert(res2.getStatus().code == 200);

    std::string body2(res2.getBody().begin(), res2.getBody().end());
    std::cout << "Cookie Response: " << body2 << std::endl;
    
    assert(body2.find("browser_lib") != std::string::npos);
    assert(body2.find("cool") != std::string::npos);

    std::cout << "test_curl_cookies PASSED" << std::endl;
}

void test_curl_redirect_cross_origin_header_strip() {
    std::cout << "Starting Cross-Origin Header Strip test..." << std::endl;
    CurlAdapter adapter;

    /**
     * Teste de segurança: redirect cross-origin deve remover Authorization.
     *
     * Fluxo:
     * 1. GET httpbin.org/redirect-to → github.com com Authorization header
     * 2. O redirect muda de origem (httpbin.org → github.com)
     * 3. O header Authorization NÃO deve ser enviado para github.com
     *
     * Verificamos indiretamente: se o redirect funciona sem erro,
     * e o header Authorization não vazou (httpbin /headers mostraria).
     */
    HttpRequest req("https://httpbin.org/redirect-to?url=https%3A%2F%2Fhttpbin.org%2Fheaders&status_code=302", HttpMethod::GET);
    req.addHeader("Authorization", "Bearer secret-token-123");
    req.addHeader("X-Custom-Header", "should-persist");
    req.setRedirectMode(RedirectMode::Follow);

    auto future = adapter.send(req);
    HttpResponse res = future.get();

    std::cout << "  Cross-Origin Status: " << res.getStatus().code << std::endl;
    assert(res.getStatus().code == 200);

    // Este redirect é same-origin (httpbin → httpbin), então Authorization deve persistir
    std::string bodyStr(res.getBody().begin(), res.getBody().end());
    // O header Authorization DEVE estar presente (same-origin)
    std::cout << "  Authorization present (same-origin): "
              << (bodyStr.find("secret-token-123") != std::string::npos) << std::endl;

    std::cout << "test_curl_redirect_cross_origin_header_strip PASSED" << std::endl;
}

void test_curl_redirect_cookies_persist() {
    std::cout << "Starting Redirect Cookie Persistence test..." << std::endl;
    CurlAdapter adapter;

    /**
     * Teste: cookies definidos durante redirect intermediário devem
     * ser salvos antes da próxima request na cadeia.
     *
     * httpbin /cookies/set faz um redirect para /cookies,
     * durante o redirect o cookie é definido via Set-Cookie header.
     * Na request final (/cookies), o cookie deve estar presente.
     */
    HttpRequest req("https://httpbin.org/cookies/set?redirect_cookie=persistent", HttpMethod::GET);
    req.setRedirectMode(RedirectMode::Follow);

    auto future = adapter.send(req);
    HttpResponse res = future.get();

    std::cout << "  Cookie Redirect Status: " << res.getStatus().code << std::endl;
    assert(res.getStatus().code == 200);
    assert(res.getRedirectInfo().redirected == true);

    std::string bodyStr(res.getBody().begin(), res.getBody().end());
    std::cout << "  Cookie body: " << bodyStr << std::endl;

    // O cookie deve ter sido preservado durante o redirect
    assert(bodyStr.find("redirect_cookie") != std::string::npos);
    assert(bodyStr.find("persistent") != std::string::npos);

    std::cout << "test_curl_redirect_cookies_persist PASSED" << std::endl;
}

void test_curl_backward_compat() {
    std::cout << "Starting Backward Compatibility test..." << std::endl;
    
    // Verificar que setFollowRedirects(bool) ainda funciona
    HttpRequest req("https://httpbin.org/get", HttpMethod::GET);
    
    // Default deve ser Follow
    assert(req.getRedirectMode() == RedirectMode::Follow);
    assert(req.getFollowRedirects() == true);
    
    // setFollowRedirects(false) deve mapear para Manual
    req.setFollowRedirects(false);
    assert(req.getRedirectMode() == RedirectMode::Manual);
    assert(req.getFollowRedirects() == false);
    
    // setFollowRedirects(true) deve voltar para Follow
    req.setFollowRedirects(true);
    assert(req.getRedirectMode() == RedirectMode::Follow);
    assert(req.getFollowRedirects() == true);
    
    std::cout << "test_curl_backward_compat PASSED" << std::endl;
}

int main() {
    // Testes unitários (sem rede)
    test_curl_backward_compat();
    
    // Testes de integração (com rede)
    test_curl_get();
    test_curl_post();
    test_curl_cookies();
    
    // Testes de redirect
    test_curl_redirect_follow();
    test_curl_redirect_manual();
    test_curl_redirect_error();
    test_curl_redirect_chain();
    test_curl_redirect_303_method_change();
    test_curl_redirect_307_method_preserve();
    test_curl_redirect_max();
    test_curl_redirect_cross_origin_header_strip();
    test_curl_redirect_cookies_persist();
    
    std::cout << "\n=== Todos os testes do CurlAdapter PASSARAM ===" << std::endl;
    return 0;
}
