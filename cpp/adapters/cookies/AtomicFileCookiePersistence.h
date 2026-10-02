#pragma once

#include "../../ports/ICookiePersistence.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace browser_network {

/**
 * @brief Persistência local de cookies via arquivo com replace atômico.
 *
 * O snapshot é escrito em um arquivo temporário no mesmo diretório
 * e promovido com rename para evitar arquivos parcialmente gravados.
 */
class AtomicFileCookiePersistence : public ICookiePersistence {
public:
    explicit AtomicFileCookiePersistence(std::filesystem::path filePath)
        : filePath_(std::move(filePath)) {}

    std::vector<StoredCookie> load() override {
        std::ifstream input(filePath_, std::ios::binary);
        if (!input.is_open()) {
            return {};
        }

        const CookieTime now = currentTime();
        std::vector<StoredCookie> cookies;
        std::string line;

        while (std::getline(input, line)) {
            if (line.empty() || line[0] == '#') {
                continue;
            }

            auto cookie = parseLine(line);
            if (!cookie.has_value()) {
                continue;
            }

            if (!cookie->persistent || cookie->isExpired(now)) {
                continue;
            }

            cookies.push_back(std::move(cookie.value()));
        }

        return cookies;
    }

    bool save(const std::vector<StoredCookie>& cookies) override {
        ensureParentDirectoryExists();

        const auto temporaryPath = makeTemporaryPath();
        const CookieTime now = currentTime();

        try {
            {
                std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
                if (!output.is_open()) {
                    return false;
                }

                output << kHeaderLine << '\n';
                output << kFieldsLine << '\n';

                for (const auto& cookie : cookies) {
                    if (!cookie.persistent || !cookie.expiresAt.has_value() || cookie.isExpired(now)) {
                        continue;
                    }

                    output << serializeLine(cookie) << '\n';
                }

                output.flush();
                if (!output.good()) {
                    return false;
                }
            }

            std::error_code error;
            std::filesystem::rename(temporaryPath, filePath_, error);
            if (error) {
                std::filesystem::remove(temporaryPath, error);
                return false;
            }
            return true;
        } catch (...) {
            std::error_code cleanupError;
            std::filesystem::remove(temporaryPath, cleanupError);
            return false;
        }
    }

    void clear() {
        std::error_code error;
        std::filesystem::remove(filePath_, error);
        if (error) {
            throw std::runtime_error("Falha ao remover arquivo de cookies: " + error.message());
        }
    }

    const std::filesystem::path& getFilePath() const {
        return filePath_;
    }

private:
    static constexpr const char* kHeaderLine = "# browser-network persistent cookies v1";
    static constexpr const char* kFieldsLine =
        "# fields: name\\tvalue\\tdomain\\tpath\\texpires_at_unix_seconds\\tsecure\\thttp_only\\thost_only\\tsame_site\\tcreation_time_unix_seconds";

    std::filesystem::path filePath_;

    static CookieTime currentTime() {
        return std::chrono::system_clock::now();
    }

    static std::int64_t toUnixSeconds(CookieTime time) {
        using namespace std::chrono;
        return duration_cast<seconds>(time.time_since_epoch()).count();
    }

    static CookieTime fromUnixSeconds(std::int64_t unixSeconds) {
        using namespace std::chrono;
        return CookieTime{seconds(unixSeconds)};
    }

    static std::vector<std::string> splitTabs(const std::string& line) {
        std::vector<std::string> fields;
        std::string current;

        for (char ch : line) {
            if (ch == '\t') {
                fields.push_back(current);
                current.clear();
                continue;
            }
            current.push_back(ch);
        }

        fields.push_back(current);
        return fields;
    }

    static bool parseBool(const std::string& value, bool& output) {
        if (value == "1") {
            output = true;
            return true;
        }
        if (value == "0") {
            output = false;
            return true;
        }
        return false;
    }

    static bool isSafeField(const std::string& value) {
        return value.find('\t') == std::string::npos &&
               value.find('\n') == std::string::npos &&
               value.find('\r') == std::string::npos;
    }

    static std::optional<StoredCookie> parseLine(const std::string& line) {
        const auto fields = splitTabs(line);
        if (fields.size() != 10) {
            return std::nullopt;
        }

        StoredCookie cookie;
        cookie.name = fields[0];
        cookie.value = fields[1];
        cookie.domain = fields[2];
        cookie.path = fields[3].empty() ? "/" : fields[3];
        cookie.sameSite = fields[8];
        cookie.persistent = true;

        if (cookie.name.empty() || cookie.domain.empty()) {
            return std::nullopt;
        }

        std::int64_t expiresAtUnixSeconds = 0;
        std::int64_t creationTimeUnixSeconds = 0;
        try {
            expiresAtUnixSeconds = std::stoll(fields[4]);
            creationTimeUnixSeconds = std::stoll(fields[9]);
        } catch (...) {
            return std::nullopt;
        }

        if (!parseBool(fields[5], cookie.secure) ||
            !parseBool(fields[6], cookie.httpOnly) ||
            !parseBool(fields[7], cookie.hostOnly)) {
            return std::nullopt;
        }

        cookie.expiresAt = fromUnixSeconds(expiresAtUnixSeconds);
        cookie.creationTime = fromUnixSeconds(creationTimeUnixSeconds);
        return cookie;
    }

    static std::string serializeLine(const StoredCookie& cookie) {
        if (cookie.name.empty() || cookie.domain.empty() || !cookie.expiresAt.has_value()) {
            throw std::runtime_error("Cookie persistente invalido: campos obrigatorios ausentes");
        }

        if (!isSafeField(cookie.name) ||
            !isSafeField(cookie.value) ||
            !isSafeField(cookie.domain) ||
            !isSafeField(cookie.path) ||
            !isSafeField(cookie.sameSite)) {
            throw std::runtime_error("Cookie persistente contem caracteres nao suportados");
        }

        std::ostringstream buffer;
        buffer << cookie.name << '\t'
               << cookie.value << '\t'
               << cookie.domain << '\t'
               << (cookie.path.empty() ? "/" : cookie.path) << '\t'
               << toUnixSeconds(cookie.expiresAt.value()) << '\t'
               << (cookie.secure ? '1' : '0') << '\t'
               << (cookie.httpOnly ? '1' : '0') << '\t'
               << (cookie.hostOnly ? '1' : '0') << '\t'
               << cookie.sameSite << '\t'
               << toUnixSeconds(cookie.creationTime);
        return buffer.str();
    }

    void ensureParentDirectoryExists() const {
        auto directory = filePath_.parent_path();
        if (directory.empty()) {
            return;
        }

        std::error_code error;
        std::filesystem::create_directories(directory, error);
        if (error) {
            throw std::runtime_error("Falha ao criar diretorio de cookies: " + error.message());
        }
    }

    std::filesystem::path makeTemporaryPath() const {
        const auto suffix = nextTemporarySuffix();
        return filePath_.parent_path() /
               (filePath_.filename().string() + ".tmp." + std::to_string(suffix));
    }

    static std::uint64_t nextTemporarySuffix() {
        static std::atomic<std::uint64_t> counter{0};
        return counter.fetch_add(1, std::memory_order_relaxed);
    }
};

} // namespace browser_network
