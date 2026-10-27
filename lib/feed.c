#include "../vendor/nng/include/nng/nng.h"
#include <string.h>

/* Pinned NNG extension: per-peer try-send, owning the message only on success. */
extern int nostril_pair1_try_send(uint32_t socket_id, uint32_t peer, nng_msg *message);

/* Main-thread adapter: NNG owns a received message until we copy and free it.
 * Pipe handles identify individual connections, never application identities. */
int nostril_feed_receive(uint32_t socket_id, void *buffer, size_t capacity,
    size_t *size, uint32_t *peer) {
    nng_socket socket = { socket_id };
    nng_msg *message = NULL;
    int rc = nng_recvmsg(socket, &message, NNG_FLAG_NONBLOCK);
    if (rc != 0) return rc;
    *peer = nng_msg_get_pipe(message).id;
    *size = nng_msg_len(message);
    if (*size > capacity) {
        nng_msg_free(message);
        return NNG_EMSGSIZE;
    }
    memcpy(buffer, nng_msg_body(message), *size);
    nng_msg_free(message);
    return 0;
}

int nostril_feed_send(uint32_t socket_id, uint32_t peer, const void *bytes,
    size_t size) {
    nng_pipe pipe = { peer };
    nng_msg *message = NULL;
    int rc = nng_msg_alloc(&message, size);
    if (rc != 0) return rc;
    memcpy(nng_msg_body(message), bytes, size);
    nng_msg_set_pipe(message, pipe);
    rc = nostril_pair1_try_send(socket_id, peer, message);
    if (rc != 0) nng_msg_free(message);
    return rc;
}

int nostril_feed_peer_alive(uint32_t peer) {
    nng_pipe pipe = { peer };
    return nng_socket_id(nng_pipe_socket(pipe)) > 0;
}
