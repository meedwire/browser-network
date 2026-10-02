#include "HttpCache.h"
#include "../utils/HttpDate.h"
#include <iostream>

namespace browser_network {

HttpCache::Resolution HttpCache::resolve(const HttpRequest& req) {
    // 1. Verificar se método permite lookup (GET/HEAD)
    if (req.getMethod() != HttpMethod::GET && req.getMethod() != HttpMethod::HEAD) {
        return { CacheResponseStatus::Miss, std::nullopt };
    }

    // 2. Lookup no storage (chave = URL por enquanto)
    auto entryOpt = storage_->get(req.getUrl());
    if (!entryOpt) {
        return { CacheResponseStatus::Miss, std::nullopt };
    }

    const auto& entry = entryOpt.value();

    // 3. Verificar status via CachePolicy
    auto now = Clock::now();
    CacheResponseStatus status = CachePolicy::checkStatus(entry, req, now);

    return { status, entry };
}

void HttpCache::store(const HttpRequest& req, const HttpResponse& res, 
                      const std::optional<CacheEntry>& staleEntry) {
    
    // 1. Verificar elegibilidade
    if (!CachePolicy::isCacheable(req, res)) {
        return;
    }

    // 2. Criar nova entrada
    CacheEntry newEntry;
    newEntry.url = req.getUrl();
    newEntry.method = req.getMethod();
    newEntry.status = res.getStatus().code;
    newEntry.statusMessage = res.getStatus().message;
    newEntry.responseHeaders = res.getHeaders(); // Headers completos
    newEntry.responseBody = res.getBody(); // Body completo (comunicação segura se pequeno, streams requerem cuidado)
    newEntry.originalRequestHeaders = req.getHeaders(); // Necessário para Vary check futuro

    // 3. Preencher Metadados
    auto now = Clock::now();
    newEntry.metadata.storedAt = now;
    newEntry.metadata.requestTime = now; // Simplificação: assumimos requestTime ~ storedAt se não monitorado
    newEntry.metadata.responseTime = now; 

    // Extração de headers específicos
    const auto& headers = res.getHeaders();
    
    // Date
    if (headers.count("Date")) newEntry.metadata.dateValue = HttpDate::parse(headers.at("Date"));
    else if (headers.count("date")) newEntry.metadata.dateValue = HttpDate::parse(headers.at("date"));
    
    // Expires
    if (headers.count("Expires")) newEntry.metadata.expiresValue = HttpDate::parse(headers.at("Expires"));
    else if (headers.count("expires")) newEntry.metadata.expiresValue = HttpDate::parse(headers.at("expires"));

    // Last-Modified
    if (headers.count("Last-Modified")) newEntry.metadata.lastModifiedValue = HttpDate::parse(headers.at("Last-Modified"));
    else if (headers.count("last-modified")) newEntry.metadata.lastModifiedValue = HttpDate::parse(headers.at("last-modified"));

    // Utils
    auto getHeader = [&](const std::string& key) {
        auto it = headers.find(key);
        if (it == headers.end()) {
             // Try lowercase
             std::string lowerKey = key; 
             std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
             it = headers.find(lowerKey);
        }
        return (it != headers.end()) ? it->second : std::string();
    };

    newEntry.metadata.etag = getHeader("ETag");
    newEntry.metadata.vary = getHeader("Vary");
    newEntry.metadata.cacheControl = getHeader("Cache-Control");
    
    std::string ageStr = getHeader("Age");
    if (!ageStr.empty()) {
        try { newEntry.metadata.ageValue = std::stol(ageStr); } catch(...) {}
    }

    // 4. Persistir
    storage_->put(req.getUrl(), newEntry);
}

void HttpCache::update(const std::string& key, const CacheEntry& staleEntry, const HttpResponse& validationRes) {
    // 304 Not Modified recebi. Atualizar metadados e headers.
    CacheEntry updatedEntry = staleEntry;
    
    // Atualizar headers que vieram no 304
    for (const auto& [name, value] : validationRes.getHeaders()) {
        updatedEntry.responseHeaders[name] = value;
    }
    
    // Atualizar Date se presente (reseta o age calculation)
    auto now = Clock::now();
    updatedEntry.metadata.responseTime = now;
    
    // Re-parse de metadados críticos
    const auto& headers = updatedEntry.responseHeaders;
    
    auto getHeader = [&](const std::string& key) {
        auto it = headers.find(key);
        if (it == headers.end()) {
             std::string lowerKey = key; 
             std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);
             it = headers.find(lowerKey);
        }
        return (it != headers.end()) ? it->second : std::string();
    };

    // Date
    if (headers.count("Date")) updatedEntry.metadata.dateValue = HttpDate::parse(headers.at("Date"));
    else if (headers.count("date")) updatedEntry.metadata.dateValue = HttpDate::parse(headers.at("date"));
    
    // Expires
    if (headers.count("Expires")) updatedEntry.metadata.expiresValue = HttpDate::parse(headers.at("Expires"));
    else if (headers.count("expires")) updatedEntry.metadata.expiresValue = HttpDate::parse(headers.at("expires"));

    // Last-Modified
    if (headers.count("Last-Modified")) updatedEntry.metadata.lastModifiedValue = HttpDate::parse(headers.at("Last-Modified"));
    else if (headers.count("last-modified")) updatedEntry.metadata.lastModifiedValue = HttpDate::parse(headers.at("last-modified"));

    updatedEntry.metadata.etag = getHeader("ETag");
    updatedEntry.metadata.vary = getHeader("Vary");
    updatedEntry.metadata.cacheControl = getHeader("Cache-Control");
    
    std::string ageStr = getHeader("Age");
    if (!ageStr.empty()) {
        try { updatedEntry.metadata.ageValue = std::stol(ageStr); } catch(...) {}
    }
    
    storage_->put(key, updatedEntry);
}

} // namespace browser_network
