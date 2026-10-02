cmake_minimum_required(VERSION 3.14)
project(browser_network_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(FetchContent)

set(BROWSER_NETWORK_IXWEBSOCKET_OVERRIDE_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ixwebsocket_overrides")
set(BROWSER_NETWORK_IXWEBSOCKET_OVERRIDE_FILES
    "ixwebsocket/IXWebSocketHttpHeaders.cpp")

function(browser_network_apply_ixwebsocket_overrides)
  if(NOT DEFINED ixwebsocket_SOURCE_DIR)
    message(FATAL_ERROR "ixwebsocket_SOURCE_DIR nao definido para aplicar overrides locais")
  endif()

  foreach(relative_path IN LISTS BROWSER_NETWORK_IXWEBSOCKET_OVERRIDE_FILES)
    set(override_source "${BROWSER_NETWORK_IXWEBSOCKET_OVERRIDE_ROOT}/${relative_path}")
    set(override_destination "${ixwebsocket_SOURCE_DIR}/${relative_path}")

    if(NOT EXISTS "${override_source}")
      message(FATAL_ERROR "Override local do ixwebsocket ausente: ${override_source}")
    endif()

    if(NOT EXISTS "${override_destination}")
      message(FATAL_ERROR "Arquivo alvo do ixwebsocket ausente: ${override_destination}")
    endif()

    execute_process(
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${override_source}" "${override_destination}"
      RESULT_VARIABLE copy_result
    )

    if(NOT copy_result EQUAL 0)
      message(FATAL_ERROR "Falha ao aplicar override local do ixwebsocket: ${relative_path}")
    endif()
  endforeach()
endfunction()

# Global settings for dependencies
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "Build static libraries" FORCE)
set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
set(HTTP_ONLY ON CACHE BOOL "" FORCE)
set(IXWEBSOCKET_INSTALL OFF CACHE BOOL "Disable install" FORCE)
set(CURL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set(CURL_ENABLE_EXPORT_TARGET OFF CACHE BOOL "" FORCE)
set(CURL_ENABLE_SSL ON CACHE BOOL "" FORCE)
set(USE_TLS ON CACHE BOOL "Enable TLS for IXWebSocket" FORCE)

if(APPLE)
  set(CURL_USE_SECTRANSP ON CACHE BOOL "" FORCE)
  set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
  set(CURL_USE_MBEDTLS OFF CACHE BOOL "" FORCE)
  set(CMAKE_USE_OPENSSL OFF CACHE BOOL "" FORCE)
  set(USE_OPEN_SSL OFF CACHE BOOL "Use OpenSSL for IXWebSocket TLS" FORCE)
  set(USE_MBED_TLS OFF CACHE BOOL "Use mbedTLS for IXWebSocket TLS" FORCE)
elseif(ANDROID)
  set(CURL_USE_SECTRANSP OFF CACHE BOOL "" FORCE)
  set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
  set(CURL_USE_MBEDTLS ON CACHE BOOL "" FORCE)
  set(CMAKE_USE_OPENSSL OFF CACHE BOOL "" FORCE)
  set(USE_OPEN_SSL OFF CACHE BOOL "Use OpenSSL for IXWebSocket TLS" FORCE)
  set(USE_MBED_TLS ON CACHE BOOL "Use mbedTLS for IXWebSocket TLS" FORCE)
  set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
  set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "" FORCE)
  set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "" FORCE)
else()
  set(CURL_USE_SECTRANSP OFF CACHE BOOL "" FORCE)
  set(CURL_USE_OPENSSL ON CACHE BOOL "" FORCE)
  set(CURL_USE_MBEDTLS OFF CACHE BOOL "" FORCE)
  set(CMAKE_USE_OPENSSL ON CACHE BOOL "" FORCE)
  set(USE_OPEN_SSL ON CACHE BOOL "Use OpenSSL for IXWebSocket TLS" FORCE)
  set(USE_MBED_TLS OFF CACHE BOOL "Use mbedTLS for IXWebSocket TLS" FORCE)
endif()

# Abseil-cpp
FetchContent_Declare(
  abseil-cpp
  URL https://github.com/abseil/abseil-cpp/archive/refs/tags/20240722.0.tar.gz
)
FetchContent_MakeAvailable(abseil-cpp)

if(ANDROID)
  FetchContent_Declare(
    mbedtls
    GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
    GIT_TAG        v3.6.6
  )
  FetchContent_MakeAvailable(mbedtls)

  set(MBEDTLS_INCLUDE_DIRS "${mbedtls_SOURCE_DIR}/include" CACHE PATH "" FORCE)
  set(MBEDTLS_LIBRARY "mbedtls" CACHE STRING "" FORCE)
  set(MBEDX509_LIBRARY "mbedx509" CACHE STRING "" FORCE)
  set(MBEDCRYPTO_LIBRARY "mbedcrypto" CACHE STRING "" FORCE)
  set(MBEDTLS_VERSION "3.6.6" CACHE STRING "" FORCE)
  set(MBEDTLS_VERSION_GREATER_THAN_3 ON CACHE BOOL "" FORCE)
  set(MBEDTLS_LIBRARIES
      "mbedtls;mbedx509;mbedcrypto"
      CACHE STRING "" FORCE
  )
endif()

# LibCurl
# Using FindPackage first for speed on host, but falling back or enforcing FetchContent if requested.
# For this task, we'll try FindPackage first to avoid compiling Curl from scratch on every test run if installed,
# but since the requirement is strict "add lib to project", we might want FetchContent. 
# However, compiling Curl takes time. Let's use generic FetchContent for robustness.
FetchContent_Declare(
  curl
  URL https://github.com/curl/curl/releases/download/curl-8_5_0/curl-8.5.0.tar.gz
)
FetchContent_MakeAvailable(curl)

# IXWebSocket (WebSocket client/server)
FetchContent_Declare(
  ixwebsocket
  GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
  GIT_TAG        v11.4.5
)
FetchContent_MakeAvailable(ixwebsocket)
browser_network_apply_ixwebsocket_overrides()

# Define source files for the core library
set(CORE_SOURCES
    core/Core.cpp
)

set(PORTS_SOURCES
    # Ports are abstract interfaces
)

set(ADAPTERS_SOURCES
    adapters/curl/CurlAdapter.cpp
    adapters/websocket/WebSocketAdapter.cpp
    core/cache/CachePolicy.cpp
    core/cache/HttpCache.cpp
    adapters/cache/CachedNetworkAdapter.cpp
    core/security/SecurityPolicy.cpp
    core/security/CookiePolicy.cpp
    core/security/CorsPolicy.cpp
    adapters/security/SecurityAdapter.cpp
)

# Create the main library target
if(ANDROID)
  set(CORE_LIB_TYPE SHARED)
else()
  set(CORE_LIB_TYPE STATIC)
endif()

add_library(browser_network_core ${CORE_LIB_TYPE}
    ${CORE_SOURCES}
    ${PORTS_SOURCES}
    ${ADAPTERS_SOURCES}
)

# Include directories
target_include_directories(browser_network_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/ports
    ${CMAKE_CURRENT_SOURCE_DIR}/usecases
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters
    ${CMAKE_CURRENT_SOURCE_DIR}/core/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/core/cache
    ${CMAKE_CURRENT_SOURCE_DIR}/core/security
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/cache
    ${CMAKE_CURRENT_SOURCE_DIR}/adapters/security
)

# Link dependencies
target_link_libraries(browser_network_core PUBLIC
    absl::flat_hash_map
    CURL::libcurl
    ixwebsocket::ixwebsocket
)

if(ANDROID)
  target_link_libraries(browser_network_core PUBLIC ${MBEDTLS_LIBRARIES})
endif()

# Configuration for testing (optional/future)
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
