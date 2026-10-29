#define _GNU_SOURCE
#include <dlfcn.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client-core.h>

typedef struct wl_proxy *(*marshal_array_flags_fn)(
    struct wl_proxy *proxy, uint32_t opcode,
    const struct wl_interface *interface, uint32_t version,
    uint32_t flags, union wl_argument *args);

#define MAX_SURFACES 128

struct tracked_surface {
    struct wl_proxy *wl_surf;
    struct wl_proxy *xdg_surf;
    struct wl_proxy *xdg_top;
    bool has_buffer_ever;
    bool has_parent;
    int32_t min_w;
    int32_t min_h;
    char title[128];
};

static struct tracked_surface surfaces[MAX_SURFACES];
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static struct tracked_surface *get_by_wl_surf(struct wl_proxy *wl_surf, bool create) {
    if (!wl_surf) return NULL;
    struct tracked_surface *free_slot = NULL;
    for (int i = 0; i < MAX_SURFACES; i++) {
        if (surfaces[i].wl_surf == wl_surf) return &surfaces[i];
        if (!free_slot && surfaces[i].wl_surf == NULL) free_slot = &surfaces[i];
    }
    if (create && free_slot) {
        memset(free_slot, 0, sizeof(*free_slot));
        free_slot->wl_surf = wl_surf;
        return free_slot;
    }
    return NULL;
}

static struct tracked_surface *get_by_xdg_surf(struct wl_proxy *xdg_surf) {
    if (!xdg_surf) return NULL;
    for (int i = 0; i < MAX_SURFACES; i++) {
        if (surfaces[i].wl_surf && surfaces[i].xdg_surf == xdg_surf) return &surfaces[i];
    }
    return NULL;
}

static struct tracked_surface *get_by_xdg_top(struct wl_proxy *xdg_top) {
    if (!xdg_top) return NULL;
    for (int i = 0; i < MAX_SURFACES; i++) {
        if (surfaces[i].wl_surf && surfaces[i].xdg_top == xdg_top) return &surfaces[i];
    }
    return NULL;
}

struct wl_proxy *wl_proxy_marshal_array_flags(
    struct wl_proxy *proxy, uint32_t opcode,
    const struct wl_interface *interface, uint32_t version,
    uint32_t flags, union wl_argument *args) {
    static marshal_array_flags_fn real_fn = NULL;
    if (!real_fn) {
        real_fn = (marshal_array_flags_fn)dlsym(RTLD_NEXT, "wl_proxy_marshal_array_flags");
    }
    if (!proxy) {
        return real_fn(proxy, opcode, interface, version, flags, args);
    }

    const char *cls = wl_proxy_get_class(proxy);
    if (!cls) {
        return real_fn(proxy, opcode, interface, version, flags, args);
    }

    if (strcmp(cls, "xdg_wm_base") == 0 && opcode == 2 && args && interface &&
        interface->name && strcmp(interface->name, "xdg_surface") == 0) {
        struct wl_proxy *wl_surf = (struct wl_proxy *)args[1].o;
        struct wl_proxy *xdg_surf = real_fn(proxy, opcode, interface, version, flags, args);
        if (xdg_surf && wl_surf) {
            pthread_mutex_lock(&lock);
            struct tracked_surface *ts = get_by_wl_surf(wl_surf, true);
            if (ts) ts->xdg_surf = xdg_surf;
            pthread_mutex_unlock(&lock);
        }
        return xdg_surf;
    }

    if (strcmp(cls, "xdg_surface") == 0) {
        if (opcode == 0) {
            pthread_mutex_lock(&lock);
            struct tracked_surface *ts = get_by_xdg_surf(proxy);
            if (ts) ts->xdg_surf = NULL;
            pthread_mutex_unlock(&lock);
            return real_fn(proxy, opcode, interface, version, flags, args);
        }
        if (opcode == 1 && interface && interface->name && strcmp(interface->name, "xdg_toplevel") == 0) {
            struct wl_proxy *top = real_fn(proxy, opcode, interface, version, flags, args);
            if (top) {
                pthread_mutex_lock(&lock);
                struct tracked_surface *ts = get_by_xdg_surf(proxy);
                if (ts) {
                    ts->xdg_top = top;
                    ts->has_parent = false;
                    ts->min_w = 0;
                    ts->min_h = 0;
                    ts->title[0] = '\0';
                }
                pthread_mutex_unlock(&lock);
            }
            return top;
        }
    }

    if (strcmp(cls, "xdg_toplevel") == 0) {
        if (opcode == 0) {
            pthread_mutex_lock(&lock);
            struct tracked_surface *ts = get_by_xdg_top(proxy);
            if (ts) ts->xdg_top = NULL;
            pthread_mutex_unlock(&lock);
            return real_fn(proxy, opcode, interface, version, flags, args);
        }
        if (opcode == 1 && args) {
            pthread_mutex_lock(&lock);
            struct tracked_surface *ts = get_by_xdg_top(proxy);
            if (ts) ts->has_parent = (args[0].o != NULL);
            pthread_mutex_unlock(&lock);
            return real_fn(proxy, opcode, interface, version, flags, args);
        }
        if (opcode == 2 && args && args[0].s) {
            pthread_mutex_lock(&lock);
            struct tracked_surface *ts = get_by_xdg_top(proxy);
            if (ts) {
                strncpy(ts->title, args[0].s, sizeof(ts->title) - 1);
                ts->title[sizeof(ts->title) - 1] = '\0';
            }
            pthread_mutex_unlock(&lock);
            return real_fn(proxy, opcode, interface, version, flags, args);
        }
        if (opcode == 8 && args) {
            pthread_mutex_lock(&lock);
            struct tracked_surface *ts = get_by_xdg_top(proxy);
            if (ts) {
                ts->min_w = args[0].i;
                ts->min_h = args[1].i;
            }
            pthread_mutex_unlock(&lock);
            return real_fn(proxy, opcode, interface, version, flags, args);
        }
    }

    if (strcmp(cls, "wl_surface") == 0) {
        if (opcode == 0) {
            pthread_mutex_lock(&lock);
            struct tracked_surface *ts = get_by_wl_surf(proxy, false);
            if (ts) memset(ts, 0, sizeof(*ts));
            pthread_mutex_unlock(&lock);
            return real_fn(proxy, opcode, interface, version, flags, args);
        }
        if (opcode == 1 && args) {
            struct wl_proxy *new_buf = (struct wl_proxy *)args[0].o;
            pthread_mutex_lock(&lock);
            struct tracked_surface *ts = get_by_wl_surf(proxy, true);
            if (ts && new_buf) ts->has_buffer_ever = true;
            pthread_mutex_unlock(&lock);
            return real_fn(proxy, opcode, interface, version, flags, args);
        }
        if (opcode == 6) {
            pthread_mutex_lock(&lock);
            struct tracked_surface *ts = get_by_wl_surf(proxy, false);
            if (ts && ts->xdg_top) {
                if (!ts->has_buffer_ever && !ts->has_parent && ts->min_w == 0 && ts->min_h == 0 &&
                    strcmp(ts->title, "heidisql") == 0) {
                    pthread_mutex_unlock(&lock);
                    return NULL;
                }
            }
            pthread_mutex_unlock(&lock);
            return real_fn(proxy, opcode, interface, version, flags, args);
        }
    }

    return real_fn(proxy, opcode, interface, version, flags, args);
}

int QPaintDevice_logicalDpiX(void *handle) {
    (void)handle;
    return 96;
}

int QPaintDevice_logicalDpiY(void *handle) {
    (void)handle;
    return 96;
}

double QScreen_logicalDotsPerInch(void *handle) {
    (void)handle;
    return 96.0;
}

double QScreen_logicalDotsPerInchX(void *handle) {
    (void)handle;
    return 96.0;
}

double QScreen_logicalDotsPerInchY(void *handle) {
    (void)handle;
    return 96.0;
}

double QScreen_physicalDotsPerInch(void *handle) {
    (void)handle;
    return 96.0;
}

double QScreen_physicalDotsPerInchX(void *handle) {
    (void)handle;
    return 96.0;
}

double QScreen_physicalDotsPerInchY(void *handle) {
    (void)handle;
    return 96.0;
}
