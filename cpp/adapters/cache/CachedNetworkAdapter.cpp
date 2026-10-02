#include "CachedNetworkAdapter.h"
#include "../../core/utils/HttpDate.h"

namespace browser_network {

std::future<HttpResponse> CachedNetworkAdapter::send(const HttpRequest& request) {
    // Lógica assíncrona para não bloquear a thread de chamada
    return std::async(std::launch::async, [this, request]() -> HttpResponse {
        // 1. Consultar Cache
        auto resolution = cache_->resolve(request);

        if (resolution.status == CacheResponseStatus::Fresh) {
            // Cache Hit! Retornar resposta cached.
            // Precisamos converter CacheEntry -> HttpResponse
            // (Idealmente teríamos um construtor ou helper, faremos manual aqui)
            const auto& entry = resolution.entry.value();
            HttpResponse res(entry.status, entry.statusMessage);
            res.setHeaders(entry.responseHeaders);
            res.setBody(entry.responseBody);
            // res.setUrl(entry.url); // Se tivéssemos salvo URL final... CacheEntry tem 'url' (da chave).
            // Redirect info não está salvo no CacheEntry simplificado... TODO: Melhorar CacheEntry
            
            // Adicionar header indicando cache hit (opcional, bom para debug)
            HeaderMap headers = res.getHeaders();
            headers["X-Cache"] = "HIT";
            res.setHeaders(std::move(headers));
            
            return res;
        }

        // 2. Preparar Request de Rede (com validação se Stale)
        HttpRequest networkReq = request;
        if (resolution.status == CacheResponseStatus::Stale && resolution.entry.has_value()) {
            const auto& entry = resolution.entry.value();
            if (!entry.metadata.etag.empty()) {
                networkReq.addHeader("If-None-Match", entry.metadata.etag);
            }
            if (entry.metadata.lastModifiedValue.has_value()) {
                networkReq.addHeader("If-Modified-Since", HttpDate::format(entry.metadata.lastModifiedValue.value()));
            }
        }

        // 3. Executar Rede (bloqueante síncrono no contexto deste async)
        // inner_->send retorna future. Precisamos esperar (.get()).
        // Cuidado: .get() pode lançar exceção.
        try {
            auto futureRes = inner_->send(networkReq);
            HttpResponse networkRes = futureRes.get();

            // 4. Processar Resposta
            if (networkRes.getStatus().code == 304 && resolution.entry.has_value()) {
                // 304 Not Modified -> Atualizar cache e retornar entry atualizada
                cache_->update(request.getUrl(), resolution.entry.value(), networkRes);
                
                // Re-obter do cache para garantir frescor e metadados atualizados
                auto updatedResOpt = cache_->resolve(request); // Pode return Fresh agora
                if (updatedResOpt.entry.has_value()) {
                    const auto& entry = updatedResOpt.entry.value();
                    HttpResponse res(entry.status, entry.statusMessage);
                    res.setHeaders(entry.responseHeaders);
                    res.setBody(entry.responseBody);
                    
                    HeaderMap headers = res.getHeaders();
                    headers["X-Cache"] = "HIT (Validated)";
                    res.setHeaders(std::move(headers));
                    return res;
                }
            }

            // 200 OK ou outro -> Armazenar e retornar
            // Store verificará isCacheable internamente
            cache_->store(request, networkRes, resolution.entry);
            
            return networkRes;

        } catch (...) {
            // Tratamento de erro de rede
            // Se StaleIfError permitido...
            if (resolution.status == CacheResponseStatus::StaleIfError && resolution.entry.has_value()) {
                 const auto& entry = resolution.entry.value();
                 HttpResponse res(entry.status, entry.statusMessage);
                 res.setHeaders(entry.responseHeaders);
                 res.setBody(entry.responseBody);
                 
                 HeaderMap headers = res.getHeaders();
                 headers["X-Cache"] = "HIT (Stale-If-Error)";
                 res.setHeaders(std::move(headers));
                 return res;
            }
            throw; // Re-throw se não puder usar fallback
        }
    });
}

std::future<StreamingResponse> CachedNetworkAdapter::sendStreaming(const HttpRequest& request) {
    // Bypass de cache para streaming (MVP)
    return inner_->sendStreaming(request);
}

} // namespace browser_network
