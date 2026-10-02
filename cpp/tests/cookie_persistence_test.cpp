#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "../adapters/cookies/AtomicFileCookiePersistence.h"

using namespace browser_network;

namespace {

std::int64_t nowUnixSeconds() {
    using namespace std::chrono;
    return duration_cast<seconds>(std::chrono::system_clock::now().time_since_epoch())
        .count();
}

CookieTime fromUnixSeconds(std::int64_t unixSeconds) {
    return CookieTime{std::chrono::seconds(unixSeconds)};
}

struct ScopedTempDir {
    explicit ScopedTempDir(std::string name)
        : path(std::filesystem::temp_directory_path() / std::move(name)) {
        std::error_code error;
        std::filesystem::remove_all(path, error);
        std::filesystem::create_directories(path);
    }

    ~ScopedTempDir() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }

    std::filesystem::path path;
};

StoredCookie makeCookie(const std::string& name,
                        const std::string& value,
                        const std::string& domain,
                        std::int64_t expiresAtUnixSeconds) {
    StoredCookie cookie;
    cookie.name = name;
    cookie.value = value;
    cookie.domain = domain;
    cookie.path = "/";
    cookie.persistent = expiresAtUnixSeconds > 0;
    if (cookie.persistent) {
        cookie.expiresAt = fromUnixSeconds(expiresAtUnixSeconds);
    }
    cookie.creationTime = fromUnixSeconds(nowUnixSeconds());
    cookie.secure = true;
    cookie.httpOnly = true;
    cookie.hostOnly = false;
    cookie.sameSite = "Lax";
    return cookie;
}

void test_load_missing_file_returns_empty() {
    ScopedTempDir tempDir("browser-network-cookie-test-missing");
    AtomicFileCookiePersistence persistence(tempDir.path / "cookies" / "persisted.txt");

    const auto cookies = persistence.load();
    assert(cookies.empty());

    std::cout << "test_load_missing_file_returns_empty PASSED" << std::endl;
}

void test_save_and_load_round_trip_filters_non_persistent_entries() {
    ScopedTempDir tempDir("browser-network-cookie-test-roundtrip");
    AtomicFileCookiePersistence persistence(tempDir.path / "nested" / "cookies.txt");

    const auto now = nowUnixSeconds();

    auto validCookie = makeCookie("session_id", "abc123", ".example.com", now + 3600);
    validCookie.sameSite = "None";
    validCookie.hostOnly = true;

    auto sessionCookie = makeCookie("memory_only", "ignored", ".example.com", 0);
    auto expiredCookie = makeCookie("expired", "old", ".example.com", now - 60);

    assert(persistence.save({validCookie, sessionCookie, expiredCookie}) == true);

    const auto loaded = persistence.load();
    assert(loaded.size() == 1);
    assert(loaded[0].name == "session_id");
    assert(loaded[0].value == "abc123");
    assert(loaded[0].domain == ".example.com");
    assert(loaded[0].path == "/");
    assert(loaded[0].sameSite == "None");
    assert(loaded[0].secure == true);
    assert(loaded[0].httpOnly == true);
    assert(loaded[0].hostOnly == true);

    std::ifstream input(persistence.getFilePath(), std::ios::binary);
    std::string fileContents((std::istreambuf_iterator<char>(input)),
                             std::istreambuf_iterator<char>());
    assert(fileContents.find("# browser-network persistent cookies v1") != std::string::npos);
    assert(fileContents.find("session_id\tabc123\t.example.com\t/\t") != std::string::npos);
    assert(fileContents.find("memory_only") == std::string::npos);
    assert(fileContents.find("expired") == std::string::npos);

    std::cout << "test_save_and_load_round_trip_filters_non_persistent_entries PASSED" << std::endl;
}

void test_load_ignores_corrupted_and_expired_rows() {
    ScopedTempDir tempDir("browser-network-cookie-test-corrupt");
    const auto filePath = tempDir.path / "cookies.txt";

    const auto now = nowUnixSeconds();
    std::ofstream output(filePath, std::ios::binary | std::ios::trunc);
    output << "# browser-network persistent cookies v1\n";
    output << "valid\tvalue\t.example.com\t/\t" << (now + 600) << "\t1\t0\t1\tStrict\t" << now << "\n";
    output << "broken-line-without-tabs\n";
    output << "bad-expiry\tvalue\t.example.com\t/\tnope\t1\t0\t0\tLax\t" << now << "\n";
    output << "expired\tvalue\t.example.com\t/\t" << (now - 1) << "\t1\t0\t0\tLax\t" << now << "\n";
    output.close();

    AtomicFileCookiePersistence persistence(filePath);
    const auto loaded = persistence.load();

    assert(loaded.size() == 1);
    assert(loaded[0].name == "valid");
    assert(loaded[0].sameSite == "Strict");
    assert(loaded[0].hostOnly == true);

    std::cout << "test_load_ignores_corrupted_and_expired_rows PASSED" << std::endl;
}

void test_save_replaces_snapshot_without_temp_artifacts() {
    ScopedTempDir tempDir("browser-network-cookie-test-replace");
    AtomicFileCookiePersistence persistence(tempDir.path / "cookies.txt");

    const auto now = nowUnixSeconds();
    assert(persistence.save({makeCookie("old_cookie", "v1", ".example.com", now + 120)}) == true);
    assert(persistence.save({makeCookie("new_cookie", "v2", ".example.com", now + 240)}) == true);

    const auto loaded = persistence.load();
    assert(loaded.size() == 1);
    assert(loaded[0].name == "new_cookie");
    assert(loaded[0].value == "v2");

    size_t fileCount = 0;
    for (const auto& entry : std::filesystem::directory_iterator(tempDir.path)) {
        assert(entry.path().filename().string().find(".tmp.") == std::string::npos);
        fileCount++;
    }
    assert(fileCount == 1);

    std::cout << "test_save_replaces_snapshot_without_temp_artifacts PASSED" << std::endl;
}

void test_clear_removes_snapshot() {
    ScopedTempDir tempDir("browser-network-cookie-test-clear");
    AtomicFileCookiePersistence persistence(tempDir.path / "cookies.txt");

    const auto now = nowUnixSeconds();
    assert(persistence.save({makeCookie("clear_me", "1", ".example.com", now + 300)}) == true);
    assert(std::filesystem::exists(persistence.getFilePath()));

    persistence.clear();
    assert(!std::filesystem::exists(persistence.getFilePath()));
    assert(persistence.load().empty());

    std::cout << "test_clear_removes_snapshot PASSED" << std::endl;
}

} // namespace

int main() {
    test_load_missing_file_returns_empty();
    test_save_and_load_round_trip_filters_non_persistent_entries();
    test_load_ignores_corrupted_and_expired_rows();
    test_save_replaces_snapshot_without_temp_artifacts();
    test_clear_removes_snapshot();

    std::cout << "\n=== Todos os testes de Cookie Persistence PASSARAM ===" << std::endl;
    return 0;
}
