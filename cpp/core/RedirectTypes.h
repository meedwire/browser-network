#pragma once

#include "HttpEntities.h"
#include <string>
#include <vector>

namespace browser_network {

/**
 * @brief Modo de tratamento de redirecionamentos conforme a Fetch API.
 *
 * Espelha a opção `redirect` da Fetch API dos navegadores:
 * - Follow: Segue redirecionamentos automaticamente (padrão).
 * - Error: Retorna erro se um redirecionamento for recebido.
 * - Manual: Retorna a resposta 3xx sem seguir, permitindo tratamento manual no JS.
 *
 * O modo Manual é essencial para expo-router e react-navigation, que precisam
 * interceptar redirecionamentos para decisões de navegação (ex: verificar auth).
 */
enum class RedirectMode {
    Follow,  ///< Segue redirecionamentos automaticamente
    Error,   ///< Retorna erro ao receber 3xx
    Manual   ///< Retorna a resposta 3xx para tratamento manual
};

/**
 * @brief Registro de um hop individual na cadeia de redirecionamentos.
 *
 * Cada redirecionamento gera uma entrada com a URL de origem,
 * o status HTTP (301, 302, 303, 307, 308), e o método HTTP
 * utilizado naquele hop (pode mudar em 303).
 *
 * Inclui headers da requisição e resposta para permitir que observadores
 * (como DevTools) reportem cada hop individual na cadeia de redirects.
 */
struct RedirectEntry {
    std::string url;       ///< URL que gerou o redirecionamento
    int statusCode;        ///< Código HTTP do redirecionamento (301-308)
    std::string method;    ///< Método HTTP usado neste hop (GET, POST, etc.)
    HeaderMap requestHeaders;  ///< Headers da requisição
    HeaderMap responseHeaders; ///< Headers da resposta (inclui Location)
};

/**
 * @brief Informações completas sobre redirecionamentos de uma requisição.
 *
 * Agrega a cadeia completa de hops, a flag `redirected`, e a URL final.
 * Compatível com as propriedades `Response.redirected` e `Response.url`
 * da Fetch API dos navegadores.
 *
 * Usado por expo-router e react-navigation para:
 * - Obter a URL final de destino (deep linking)
 * - Inspecionar cada hop da cadeia (decisões de navegação)
 * - Saber se houve redirecionamento (Response.redirected)
 */
struct RedirectInfo {
    bool redirected = false;                ///< true se houve pelo menos um redirecionamento
    std::string finalUrl;                   ///< URL final após todos os redirecionamentos
    std::vector<RedirectEntry> chain;       ///< Cadeia completa de hops
};

/**
 * @brief Converte HttpMethod para string legível.
 * @param method O método HTTP.
 * @return String representando o método (ex: "GET", "POST").
 */
inline std::string httpMethodToString(HttpMethod method) {
    switch (method) {
        case HttpMethod::GET:     return "GET";
        case HttpMethod::POST:    return "POST";
        case HttpMethod::PUT:     return "PUT";
        case HttpMethod::DELETE:  return "DELETE";
        case HttpMethod::PATCH:   return "PATCH";
        case HttpMethod::HEAD:    return "HEAD";
        case HttpMethod::OPTIONS: return "OPTIONS";
        default:                  return "UNKNOWN";
    }
}

} // namespace browser_network
