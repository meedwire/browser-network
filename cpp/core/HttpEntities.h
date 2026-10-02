#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "absl/container/flat_hash_map.h"

namespace browser_network {

enum class HttpMethod {
    GET,
    POST,
    PUT,
    DELETE,
    PATCH,
    HEAD,
    OPTIONS
};

using HeaderMap = absl::flat_hash_map<std::string, std::string>;
using ByteBuffer = std::vector<uint8_t>;

struct HttpStatus {
    int code;
    std::string message;
};

} // namespace browser_network
