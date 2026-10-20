cmake_minimum_required(VERSION 3.24)
project(nostril_websocket_dependency C)
find_package(OpenSSL REQUIRED COMPONENTS SSL Crypto)

# Keep policy outside the unmodified upstream snapshot. HTTP/1.1 upgrade,
# WebSocket client, OpenSSL TLS, and the built-in poll loop are the baseline.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
foreach(option_name IN ITEMS
    LWS_WITH_SSL LWS_ROLE_H1 LWS_ROLE_WS LWS_WITH_STATIC LWS_WITH_SHARED
    LWS_STATIC_PIC LWS_SSL_CLIENT_USE_OS_CA_CERTS LWS_WITHOUT_SERVER
    LWS_WITH_GENCRYPTO LWS_WITH_LEJP
    LWS_WITHOUT_TESTAPPS LWS_WITHOUT_EXTENSIONS)
    set(${option_name} ON CACHE BOOL "Nostril build profile" FORCE)
endforeach()
foreach(option_name IN ITEMS
    LWS_WITHOUT_CLIENT LWS_WITH_HTTP2 LWS_WITH_HTTP3 LWS_ROLE_QUIC
    LWS_ROLE_MQTT LWS_ROLE_RAW_FILE LWS_WITH_UDP LWS_WITH_SECURE_STREAMS
    LWS_WITH_MINIMAL_EXAMPLES LWS_WITH_EVLIB_PLUGINS LWS_WITH_PLUGINS
    LWS_WITH_LIBUV LWS_WITH_LIBEV LWS_WITH_LIBEVENT LWS_WITH_LIBCAP
    LWS_WITH_UPNG LWS_WITH_JPEG LWS_WITH_DLO LWS_WITH_GZINFLATE
    LWS_WITH_LHP LWS_WITH_LEJP_CONF LWS_WITH_JSONRPC
    LWS_WITH_STUB LWS_WITH_SYS_SMD LWS_WITH_SYS_STATE LWS_WITH_CONMON
    LWS_WITH_CACHE_NSCOOKIEJAR LWS_WITH_WOL LWS_WITH_NETLINK LWS_WITH_DIR
    LWS_WITH_GNUTLS LWS_WITH_MBEDTLS LWS_WITH_BEARSSL LWS_WITH_ZLIB
    LWS_WITH_HTTP_BASIC_AUTH LWS_WITH_HTTP_DIGEST_AUTH)
    set(${option_name} OFF CACHE BOOL "Nostril build profile" FORCE)
endforeach()

# The snapshot has no .git directory. Do not report Nostril's revision as LWS's.
set(CMAKE_DISABLE_FIND_PACKAGE_Git ON)
set(GIT_EXECUTABLE "" CACHE FILEPATH "No git metadata in vendored snapshot" FORCE)
set(LWS_BUILD_HASH "a842936f881921b6462b3328cce8e700b7a7ecd2")
add_subdirectory(../../vendor/libwebsockets upstream)

add_executable(nostril-websocket-smoke smoke.c)
target_link_libraries(nostril-websocket-smoke PRIVATE websockets)
target_compile_options(nostril-websocket-smoke PRIVATE -Wall -Wextra -Werror)
add_executable(nostril-websocket-shared-smoke smoke.c)
target_link_libraries(nostril-websocket-shared-smoke PRIVATE websockets_shared)
target_compile_options(nostril-websocket-shared-smoke PRIVATE -Wall -Wextra -Werror)
