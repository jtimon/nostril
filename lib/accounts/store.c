#include "ext.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <unistd.h>

struct account_store { int directory; int lock; };

static int private_object(int fd, int directory) {
    struct stat st;
    if (fstat(fd, &st)) return errno;
    if ((directory ? !S_ISDIR(st.st_mode) : !S_ISREG(st.st_mode)) ||
        st.st_uid != getuid() || (st.st_mode & 077)) return EACCES;
    return 0;
}

int32_t nostril_store_directory(const char *path, Bool create) {
    int created = 0;
    if (create) {
        if (mkdir(path, 0700) == 0) created = 1;
        else if (errno != EEXIST) return errno;
    }
    int fd = open(path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return errno;
    int error = private_object(fd, 1);
    close(fd);
    if (!error && created) {
        char *parent = strdup(path);
        if (!parent) return ENOMEM;
        char *slash = strrchr(parent, '/');
        if (slash == parent) slash[1] = 0;
        else if (slash) *slash = 0;
        fd = open(slash ? parent : ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (fd < 0 || fsync(fd)) error = errno;
        if (fd >= 0) close(fd);
        free(parent);
    }
    return error;
}

void *nostril_store_lock(const char *path, int32_t *error) {
    *error = 0;
    int directory = open(path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (directory < 0) { *error = errno; return NULL; }
    *error = private_object(directory, 1);
    if (*error) { close(directory); return NULL; }
    int lock = openat(directory, ".lock", O_CREAT | O_RDWR | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (lock < 0) { *error = errno; close(directory); return NULL; }
    *error = private_object(lock, 0);
    if (!*error && flock(lock, LOCK_EX)) *error = errno;
    if (*error) { close(lock); close(directory); return NULL; }
    struct account_store *store = malloc(sizeof(*store));
    if (!store) { *error = ENOMEM; close(lock); close(directory); return NULL; }
    store->directory = directory;
    store->lock = lock;
    return store;
}

void nostril_store_unlock(void *handle) {
    struct account_store *store = handle;
    close(store->lock);
    close(store->directory);
    free(store);
}

static Str *store_string(size_t size) {
    Str *s = calloc(1, sizeof(*s));
    if (!s) abort();
    s->c_str = calloc(size + 1, 1);
    if (!s->c_str) abort();
    s->count = s->cap = (USize)size;
    return s;
}

Str *nostril_store_read(void *handle, const char *name, int32_t *error) {
    struct account_store *store = handle;
    *error = 0;
    int fd = openat(store->directory, name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) { *error = errno == ENOENT ? -1 : errno; return store_string(0); }
    *error = private_object(fd, 0);
    struct stat st;
    if (!*error && fstat(fd, &st)) *error = errno;
    if (!*error && (st.st_size < 0 || st.st_size > 65536)) *error = EFBIG;
    if (*error) { close(fd); return store_string(0); }
    Str *out = store_string((size_t)st.st_size);
    size_t at = 0;
    while (at < out->count) {
        ssize_t n = read(fd, out->c_str + at, out->count - at);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { *error = n ? errno : EIO; break; }
        at += (size_t)n;
    }
    close(fd);
    return out;
}

int32_t nostril_store_write(void *handle, const char *name, const Str *bytes, Bool exclusive) {
    struct account_store *store = handle;
    char temporary[64];
    uint64_t nonce;
    if (bytes->count > 65536) return EFBIG;
    if (getrandom(&nonce, sizeof(nonce), 0) != sizeof(nonce)) return EIO;
    snprintf(temporary, sizeof(temporary), ".pending-%016llx", (unsigned long long)nonce);
    const char *destination = exclusive ? name : temporary;
    int fd = openat(store->directory, destination,
        O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) return errno;
    int error = 0;
    size_t at = 0;
    while (at < bytes->count) {
        ssize_t n = write(fd, bytes->c_str + at, bytes->count - at);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { error = n ? errno : EIO; break; }
        at += (size_t)n;
    }
    if (!error && fsync(fd)) error = errno;
    if (close(fd) && !error) error = errno;
    if (!error && !exclusive && renameat(store->directory, temporary, store->directory, name)) error = errno;
    if (error) unlinkat(store->directory, destination, 0);
    if (!error && fsync(store->directory)) error = errno;
    return error;
}
