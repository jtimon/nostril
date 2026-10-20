#include <libwebsockets.h>
#include <stdio.h>
#include <string.h>

#if !defined(LWS_ROLE_WS) || !defined(LWS_ROLE_H1) || !defined(LWS_OPENSSL_SUPPORT)
#error "Nostril needs WebSocket, HTTP/1 upgrade and TLS support"
#endif
#ifndef LWS_WITH_CLIENT
#error "Nostril needs client support"
#endif
#if defined(LWS_WITH_HTTP2) || defined(LWS_WITH_HTTP3) || defined(LWS_ROLE_MQTT)
#error "Unexpected protocol enabled in the minimal profile"
#endif

/* Exercise the linked library and TLS context lifecycle without network access.
 * This is dependency verification, not a Nostr transport implementation. */
int main(void) {
    static const struct lws_protocols protocols[] = {
        { "nostril", lws_callback_http_dummy, 0, 4096, 0, NULL, 0 },
        LWS_PROTOCOL_LIST_TERM
    };
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.fd_limit_per_thread = 8;
    lws_set_log_level(LLL_ERR | LLL_WARN, NULL);
    for (int i = 0; i < 2; i++) {
        struct lws_context *context = lws_create_context(&info);
        if (!context) return 1;
        lws_context_destroy(context);
    }
    printf("libwebsockets %s: client TLS context OK\n", lws_get_library_version());
    return 0;
}
