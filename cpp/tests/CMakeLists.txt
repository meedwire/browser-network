cmake_minimum_required(VERSION 3.14)

project(browser_network_tests)

# For now, we'll just use a simple executable without GTest to avoid fetching dependencies for this initial check.
# In a real scenario, we'd FetchContent GTest here.

add_executable(core_test core_test.cpp)

# Link against the core library (which currently is header only/interface, but good practice)
target_link_libraries(core_test PRIVATE browser_network_core)

add_executable(curl_adapter_test curl_adapter_test.cpp)
target_link_libraries(curl_adapter_test PRIVATE browser_network_core)

add_executable(stream_test stream_test.cpp)
target_link_libraries(stream_test PRIVATE browser_network_core)

add_executable(websocket_test websocket_test.cpp)
target_link_libraries(websocket_test PRIVATE browser_network_core)

add_executable(core_cache_test core_cache_test.cpp)
target_link_libraries(core_cache_test PRIVATE browser_network_core)

add_executable(cached_adapter_test cached_adapter_test.cpp)
target_link_libraries(cached_adapter_test PRIVATE browser_network_core)

add_executable(security_policy_test security_policy_test.cpp)
target_link_libraries(security_policy_test PRIVATE browser_network_core)

add_executable(cors_policy_test cors_policy_test.cpp)
target_link_libraries(cors_policy_test PRIVATE browser_network_core)

add_executable(security_adapter_test security_adapter_test.cpp)
target_link_libraries(security_adapter_test PRIVATE browser_network_core)

add_executable(cookie_jar_test cookie_jar_test.cpp)
target_link_libraries(cookie_jar_test PRIVATE browser_network_core)
add_test(NAME cookie_jar_test COMMAND cookie_jar_test)

# Testes abrangentes para 100% de cobertura (2026-02-15)
add_executable(http_request_comprehensive_test http_request_comprehensive_test.cpp)
target_link_libraries(http_request_comprehensive_test PRIVATE browser_network_core)

add_executable(cookie_persistence_test cookie_persistence_test.cpp)
target_link_libraries(cookie_persistence_test PRIVATE browser_network_core)
add_test(NAME cookie_persistence_test COMMAND cookie_persistence_test)
