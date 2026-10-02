#pragma once

#include <array>
#include <string_view>

namespace browser_network {
namespace cookie_psl {

inline constexpr std::array<std::string_view, 12> kMinimalPublicSuffixes = {
    "app",
    "co.jp",
    "co.uk",
    "com",
    "dev",
    "edu",
    "gov",
    "io",
    "net",
    "org",
    "org.uk",
    "uk",
};

inline bool isPublicSuffix(std::string_view domain) {
    for (std::string_view suffix : kMinimalPublicSuffixes) {
        if (suffix == domain) {
            return true;
        }
    }

    return false;
}

} // namespace cookie_psl
} // namespace browser_network
