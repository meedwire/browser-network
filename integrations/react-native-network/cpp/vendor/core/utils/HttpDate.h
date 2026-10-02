#pragma once

#include <chrono>
#include <string>
#include <optional>
#include <iomanip>
#include <sstream>

namespace browser_network {

using Clock = std::chrono::system_clock;
using TimePoint = std::chrono::time_point<Clock>;

class HttpDate {
public:
    /**
     * @brief Interpreta uma string de data HTTP (RFC 1123).
     * Exemplo: "Tue, 15 Nov 1994 08:12:31 GMT"
     */
    static std::optional<TimePoint> parse(const std::string& dateStr) {
        if (dateStr.empty()) return std::nullopt;
        
        std::istringstream ss(dateStr);
        std::tm tm = {};
        // Formato preferencial RFC 7231: Sun, 06 Nov 1994 08:49:37 GMT
        ss >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");
        
        if (ss.fail()) {
            // Tentar formato RFC 850: Sunday, 06-Nov-94 08:49:37 GMT
            ss.clear();
            ss.str(dateStr);
            ss >> std::get_time(&tm, "%A, %d-%b-%y %H:%M:%S GMT");
        }
        
        if (ss.fail()) {
             // Tentar formato asctime: Sun Nov  6 08:49:37 1994
            ss.clear();
            ss.str(dateStr);
            ss >> std::get_time(&tm, "%a %b %d %H:%M:%S %Y");
        }

        if (ss.fail()) return std::nullopt;

        // Converter para time_point (assumindo UTC)
        // std::get_time não lida com timezones, e timegm não é C++ standard cross-platform (mas é POSIX).
        // Para simplificar e manter portabilidade C++ pura, usamos mktime e ajustamos GMT offset.
        // O ideal em projetos reais é usar uma lib de data, mas aqui vamos com standard.
        // Para este propósito, assumiremos que o sistema está configurado corretamente
        // ou faremos o cálculo manual de UTC -> epoch.
        
        time_t t = timegm(&tm); // POSIX/BSD standard, disponível em Mac/Linux.
        return Clock::from_time_t(t);
    }

    /**
     * @brief Formata um time_point para string HTTP (RFC 1123).
     */
    static std::string format(TimePoint tp) {
        std::time_t t = Clock::to_time_t(tp);
        std::tm tm = *std::gmtime(&t);
        
        std::stringstream ss;
        ss << std::put_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");
        return ss.str();
    }
};

} // namespace browser_network
