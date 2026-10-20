#include <libwebsockets.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Exported by the compiled Til library. This is executable code, not a Til
 * closure value. The native ABI here is the project's Linux/USize64 profile. */
extern int32_t til_nostril_x5fws__ws_callback(void *, int32_t, void *, void *, uint64_t);

_Static_assert(LWS_CALLBACK_CLIENT_CONNECTION_ERROR == 1 &&
    LWS_CALLBACK_CLIENT_ESTABLISHED == 3 && LWS_CALLBACK_CLIENT_RECEIVE == 8 &&
    LWS_CALLBACK_CLIENT_WRITEABLE == 10 && LWS_CALLBACK_CLIENT_CLOSED == 75 &&
    LWS_WRITE_TEXT == 0, "Update the Til callback constants for this LWS version");
_Static_assert(sizeof(size_t) == sizeof(uint64_t), "Nostril's native build uses USize64");
_Static_assert(sizeof(enum lws_callback_reasons) == sizeof(int32_t), "Callback reason ABI must match I32");

struct native_ws {
    struct lws_context *context;
    struct lws *connection;
    lws_parse_uri_t *uri;
    char *path;
    char *ca_file;
    const char *host;
    int port;
    int tls;
};

static const struct lws_protocols protocols[] = {
    { "nostril", (lws_callback_function *)til_nostril_x5fws__ws_callback,
      0, 4096, 0, NULL, 0 },
    LWS_PROTOCOL_LIST_TERM
};

void *ws_native_new(void *state, const char *url, const char *ca_file) {
    struct native_ws *native = calloc(1, sizeof(*native));
    struct lws_context_creation_info info;
    const char *protocol, *path;
    if (!native) return NULL;
    native->ca_file = strdup(ca_file);
    if (!native->ca_file) goto fail;
    native->uri = lws_parse_uri_create(url);
    if (!native->uri) goto fail;
    protocol = native->uri->scheme;
    path = native->uri->path;
    native->host = native->uri->host;
    native->port = native->uri->port;
    if (!*native->host || !native->port || native->uri->unix_skt) goto fail;
    if (strcmp(protocol, "ws") && strcmp(protocol, "wss")) goto fail;
    native->tls = !strcmp(protocol, "wss");
    native->path = malloc(strlen(path) + 2);
    if (!native->path) goto fail;
    if (*path == '/') strcpy(native->path, path);
    else { native->path[0] = '/'; strcpy(native->path + 1, path); }
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.user = state;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT | LWS_SERVER_OPTION_VALIDATE_UTF8;
    info.client_ssl_ca_filepath = *native->ca_file ? native->ca_file : NULL;
    info.fd_limit_per_thread = 16;
    native->context = lws_create_context(&info);
    if (!native->context) goto fail;
    return native;
fail:
    free(native->path);
    free(native->ca_file);
    lws_parse_uri_destroy(&native->uri);
    free(native);
    return NULL;
}

int ws_native_connect(void *handle) {
    struct native_ws *native = handle;
    struct lws_client_connect_info info;
    memset(&info, 0, sizeof(info));
    info.context = native->context;
    info.address = native->host;
    info.host = native->host;
    info.origin = native->host;
    info.port = native->port;
    info.path = native->path;
    info.local_protocol_name = "nostril";
    /* No WebSocket subprotocol header, and no TLS verification bypass flags. */
    info.ssl_connection = native->tls ? LCCSCF_USE_SSL : 0;
    info.pwsi = &native->connection;
    return lws_client_connect_via_info(&info) ? 0 : -1;
}

int ws_native_poll(void *handle) {
    struct native_ws *native = handle;
    /* lws_service's timeout argument alone does not guarantee nonblocking. */
    lws_cancel_service(native->context);
    return lws_service(native->context, 0);
}

uint64_t ws_native_pre(void) { return LWS_PRE; }

void ws_native_destroy(void *handle) {
    struct native_ws *native = handle;
    lws_context_destroy(native->context);
    free(native->path);
    free(native->ca_file);
    lws_parse_uri_destroy(&native->uri);
    free(native);
}
