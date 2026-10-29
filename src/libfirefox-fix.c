#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <wayland-client.h>

#define XDG_TOPLEVEL_STATE_MAXIMIZED 1

typedef int (*add_listener_fn)(struct wl_proxy *proxy, void (**implementation)(void), void *data);

static void (*g_real_configure)(void *, void *, int32_t, int32_t, struct wl_array *) = NULL;
static void *g_hooked_listener[8];

static void toplevel_configure_wrapper(void *data, void *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states) {
    bool has_maximized = false;
    uint32_t *s = (uint32_t *)states->data;
    size_t count = states->size / sizeof(uint32_t);

    for (size_t i = 0; i < count; i++) {
        if (s[i] == XDG_TOPLEVEL_STATE_MAXIMIZED) {
            has_maximized = true;
            break;
        }
    }

    if (!has_maximized) {
        uint32_t new_states[count + 1];
        if (count > 0 && states->data) {
            memcpy(new_states, states->data, states->size);
        }
        new_states[count] = XDG_TOPLEVEL_STATE_MAXIMIZED;
        struct wl_array augmented = {
            .size = (count + 1) * sizeof(uint32_t),
            .alloc = (count + 1) * sizeof(uint32_t),
            .data = new_states
        };
        if (g_real_configure) {
            g_real_configure(data, xdg_toplevel, width, height, &augmented);
        }
    } else {
        if (g_real_configure) {
            g_real_configure(data, xdg_toplevel, width, height, states);
        }
    }
}

int wl_proxy_add_listener(struct wl_proxy *proxy, void (**implementation)(void), void *data) {
    static add_listener_fn real_fn = NULL;
    if (!real_fn) real_fn = (add_listener_fn)dlsym(RTLD_NEXT, "wl_proxy_add_listener");

    const char *cls = proxy ? wl_proxy_get_class(proxy) : NULL;
    if (cls && strcmp(cls, "xdg_toplevel") == 0 && implementation) {
        void **funcs = (void **)implementation;
        g_real_configure = (void (*)(void *, void *, int32_t, int32_t, struct wl_array *))funcs[0];
        memcpy(g_hooked_listener, funcs, sizeof(g_hooked_listener));
        g_hooked_listener[0] = (void *)toplevel_configure_wrapper;
        return real_fn(proxy, (void (**)(void))g_hooked_listener, data);
    }
    return real_fn(proxy, implementation, data);
}
