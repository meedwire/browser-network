cmake_minimum_required(VERSION 3.18.0)
project(BrowserNetwork)

set(CMAKE_CXX_STANDARD 20)

find_package(ReactAndroid REQUIRED CONFIG)
find_package(fbjni REQUIRED CONFIG)

set(CORE_CPP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../cpp")

add_library(browser_network_core_local STATIC
    "${CORE_CPP_DIR}/core/Core.cpp"
    "${CORE_CPP_DIR}/core/cache/CachePolicy.cpp"
    "${CORE_CPP_DIR}/core/cache/HttpCache.cpp"
    "${CORE_CPP_DIR}/core/security/SecurityPolicy.cpp"
    "${CORE_CPP_DIR}/core/security/CookiePolicy.cpp"
    "${CORE_CPP_DIR}/core/security/CorsPolicy.cpp"
    "${CORE_CPP_DIR}/adapters/cache/CachedNetworkAdapter.cpp"
    "${CORE_CPP_DIR}/adapters/curl/CurlAdapter.cpp"
    "${CORE_CPP_DIR}/adapters/security/SecurityAdapter.cpp"
    "${CORE_CPP_DIR}/adapters/websocket/WebSocketAdapter.cpp"
)

target_compile_definitions(browser_network_core_local PUBLIC
    CURL_STATICLIB
    IXWEBSOCKET_USE_TLS
    IXWEBSOCKET_USE_MBED_TLS
    IXWEBSOCKET_USE_ZLIB
)

target_include_directories(browser_network_core_local PUBLIC
    "${CORE_CPP_DIR}"
    "${CORE_CPP_DIR}/core"
    "${CORE_CPP_DIR}/ports"
    "${CORE_CPP_DIR}/usecases"
    "${CORE_CPP_DIR}/adapters"
    "${CORE_CPP_DIR}/core/utils"
    "${CORE_CPP_DIR}/core/cache"
    "${CORE_CPP_DIR}/core/security"
    "${CORE_CPP_DIR}/adapters/cache"
    "${CORE_CPP_DIR}/adapters/security"
)

add_library(react_native_browser_network SHARED
    ../cpp/BrowserNetworkJSI.cpp
    ../cpp/WebSocketJSI.cpp
    ../cpp/observers/NetworkObserverAdapter.cpp
    ../cpp/observers/JsiNetworkObserver.cpp
    ../cpp/observers/InspectorNetworkObserver.cpp
    BrowserNetworkOnLoad.cpp
)

target_compile_options(react_native_browser_network PRIVATE
    -DFOLLY_NO_CONFIG
    -DFOLLY_MOBILE=1
    -DFOLLY_USE_LIBCPP=1
)

target_include_directories(react_native_browser_network PUBLIC
    ../cpp
    ../cpp/vendor
)

target_link_libraries(react_native_browser_network
    browser_network_core_local
    ReactAndroid::jsi
    ReactAndroid::reactnative
    fbjni::fbjni
    log
)

# Link all vendored Android dependencies (absl, curl, ixwebsocket, mbedtls)
file(GLOB_RECURSE STATIC_LIBS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/libs/${ANDROID_ABI}/*.a"
)
target_link_libraries(react_native_browser_network ${STATIC_LIBS})
