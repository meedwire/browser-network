#include "CachePolicy.h"
#include "../utils/HttpDate.h"
#include <algorithm>
#include <iostream>

namespace browser_network {

float CachePolicy::heuristicFraction_ = 0.1f;

bool CachePolicy::isCacheable(const HttpRequest& req, const HttpResponse& res) {
    // 1. Método deve ser cacheável (GET ou HEAD)
    if (req.getMethod() != HttpMethod::GET && req.getMethod() != HttpMethod::HEAD) {
        return false;
    }

    // 2. Status code deve ser cacheável
    // Removido 206 conforme requisito 11 (Partial Content não cacheado simplificado)
    int status = res.getStatus().code;
    bool statusCacheable = (status == 200 || status == 203 || status == 204 || 
                            status == 300 || status == 301 || 
                            status == 404 || status == 405 || status == 410 || 
                            status == 414 || status == 501);
    if (!statusCacheable) {
        return false;
    }

    const auto& headers = res.getHeaders();

    // 3. Cache-Control: no-store
    auto itCC = headers.find("Cache-Control");
    if (itCC == headers.end()) itCC = headers.find("cache-control");
    
    std::string cc = (itCC != headers.end()) ? itCC->second : "";
    if (cc.find("no-store") != std::string::npos) {
        return false;
    }
    
    // 3b. Pragma: no-cache (apenas se Cache-Control não existir, mas por segurança checamos)
    // RFC 7234: Pragma: no-cache em request, response geralmente só Cache-Control
    // Mas se vier Pragma: no-cache na response, é bom respeitar.
    
    // 4. Authorization e Cache-Control
    auto itAuth = req.getHeaders().find("Authorization");
    if (itAuth != req.getHeaders().end()) {
        // Se tem Authorization, sÃ³ cache se tiver "public", "must-revalidate" ou "s-maxage"
        bool allowsCache = (cc.find("public") != std::string::npos || 
                            cc.find("s-maxage") != std::string::npos ||
                            cc.find("must-revalidate") != std::string::npos);
        if (!allowsCache) {
            return false;
        }
    }

    return true;
}

std::chrono::seconds CachePolicy::computeAge(const CacheEntry& entry, TimePoint now) {
    using namespace std::chrono;
    
    // metadata stores prepared TimePoints
    auto dateValue = entry.metadata.dateValue.value_or(entry.metadata.responseTime);
    
    // apparent_age = max(0, response_time - date_value);
    auto diffApparent = duration_cast<seconds>(entry.metadata.responseTime - dateValue);
    long apparentAge = std::max(0L, (long)diffApparent.count());

    // response_delay = response_time - request_time;
    auto diffDelay = duration_cast<seconds>(entry.metadata.responseTime - entry.metadata.requestTime);
    long responseDelay = (long)diffDelay.count();

    // corrected_age_value = age_value + response_delay;
    long correctedAgeValue = entry.metadata.ageValue + responseDelay;

    // corrected_initial_age = max(apparent_age, corrected_age_value);
    long correctedInitialAge = std::max(apparentAge, correctedAgeValue);

    // resident_time = now - response_time;
    auto diffResident = duration_cast<seconds>(now - entry.metadata.responseTime);
    long residentTime = std::max(0L, (long)diffResident.count());

    // current_age = corrected_initial_age + resident_time;
    return seconds(correctedInitialAge + residentTime);
}

std::chrono::seconds CachePolicy::computeFreshnessLifetime(const CacheEntry& entry) {
    using namespace std::chrono;
    
    std::string cc = entry.metadata.cacheControl;
    
    // 1. max-age
    size_t maxAgePos = cc.find("max-age=");
    if (maxAgePos != std::string::npos) {
        try {
            long maxAge = std::stol(cc.substr(maxAgePos + 8));
            return seconds(maxAge);
        } catch (...) {}
    }
    
    // 2. Expires
    if (entry.metadata.expiresValue.has_value() && entry.metadata.dateValue.has_value()) {
        auto diff = duration_cast<seconds>(entry.metadata.expiresValue.value() - entry.metadata.dateValue.value());
        return std::max(seconds(0), diff);
    }
    
    // 3. Heurística
    if (entry.metadata.lastModifiedValue.has_value() && entry.metadata.dateValue.has_value()) {
        auto diff = duration_cast<seconds>(entry.metadata.dateValue.value() - entry.metadata.lastModifiedValue.value());
        long lifetime = (long)(diff.count() * heuristicFraction_);
        return std::max(seconds(0), seconds(lifetime));
    }
    
    return seconds(0);
}

CacheResponseStatus CachePolicy::checkStatus(const CacheEntry& entry, const HttpRequest& req, TimePoint now) {
    if (!matchesRequest(entry, req)) {
        return CacheResponseStatus::Miss;
    }
    
    if (entry.metadata.cacheControl.find("no-cache") != std::string::npos) {
        return CacheResponseStatus::Stale; // Força revalidação
    }
    
    auto age = computeAge(entry, now);
    auto freshness = computeFreshnessLifetime(entry);
    
    if (age < freshness) {
        return CacheResponseStatus::Fresh;
    }

    // Verificar extensões Stale (RFC 5861)
    std::string cc = entry.metadata.cacheControl;
    
    // stale-while-revalidate
    size_t swrPos = cc.find("stale-while-revalidate=");
    if (swrPos != std::string::npos) {
        try {
            long swr = std::stol(cc.substr(swrPos + 23));
            if (age < freshness + std::chrono::seconds(swr)) {
                return CacheResponseStatus::StaleWhileRevalidate;
            }
        } catch (...) {}
    }

    // stale-if-error
    size_t siePos = cc.find("stale-if-error=");
    if (siePos != std::string::npos) {
        try {
             long sie = std::stol(cc.substr(siePos + 15));
             if (age < freshness + std::chrono::seconds(sie)) {
                 return CacheResponseStatus::StaleIfError;
             }
        } catch (...) {}
    }
    
    return CacheResponseStatus::Stale;
}

bool CachePolicy::matchesRequest(const CacheEntry& entry, const HttpRequest& req) {
    if (entry.url != req.getUrl()) return false;
    if (entry.method != req.getMethod()) return false;
    
    if (!entry.metadata.vary.empty()) {
        if (entry.metadata.vary == "*") return false;
        
        // Parse Vary header (comma separated)
        std::string vary = entry.metadata.vary;
        std::stringstream ss(vary);
        std::string segment;
        
        while (std::getline(ss, segment, ',')) {
            // Trim whitespace
            size_t first = segment.find_first_not_of(' ');
            if (std::string::npos == first) continue;
            size_t last = segment.find_last_not_of(' ');
            std::string headerName = segment.substr(first, (last - first + 1));
            
            // Comparar headers da request atual com a original
            auto getHeader = [](const HeaderMap& map, const std::string& key) {
                auto it = map.find(key);
                // Case-insensitive lookup simples
                if (it == map.end()) {
                    std::string lower = key;
                    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                    it = map.find(lower);
                }
                return (it != map.end()) ? it->second : std::string();
            };
            
            std::string originalVal = getHeader(entry.originalRequestHeaders, headerName);
            std::string currentVal = getHeader(req.getHeaders(), headerName);
            
            if (originalVal != currentVal) {
                return false;
            }
        }
    }
    
    return true;
}

} // namespace browser_network
