#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <wayland-client.h>

typedef int (*add_listener_fn)(struct wl_proxy *proxy, void (**implementation)(void), void *data);
typedef struct wl_proxy *(*marshal_flags_fn)(
    struct wl_proxy *proxy, uint32_t opcode,
    const struct wl_interface *interface, uint32_t version,
    uint32_t flags, union wl_argument *args);

struct wrapped_seat_listener {
    void (*real_capabilities)(void *data, struct wl_seat *seat, uint32_t capabilities);
    void (*real_name)(void *data, struct wl_seat *seat, const char *name);
    void *real_data;
};

static struct wrapped_seat_listener g_seat;

static void seat_caps_wrapper(void *data, struct wl_seat *seat, uint32_t capabilities) {
    // Mask out WL_SEAT_CAPABILITY_TOUCH (4) so GTK / Firefox treats touches as mouse pointer
    uint32_t no_touch = capabilities & ~4U;
    if (g_seat.real_capabilities) {
        g_seat.real_capabilities(data, seat, no_touch);
    }
}

static void seat_name_wrapper(void *data, struct wl_seat *seat, const char *name) {
    if (g_seat.real_name) {
        g_seat.real_name(data, seat, name);
    }
}

static struct wl_seat_listener g_hooked_listener = {
    .capabilities = seat_caps_wrapper,
    .name = seat_name_wrapper,
};

int wl_proxy_add_listener(struct wl_proxy *proxy, void (**implementation)(void), void *data) {
    static add_listener_fn real_fn = NULL;
    if (!real_fn) real_fn = (add_listener_fn)dlsym(RTLD_NEXT, "wl_proxy_add_listener");

    const char *cls = proxy ? wl_proxy_get_class(proxy) : NULL;
    if (cls && strcmp(cls, "wl_seat") == 0 && implementation) {
        struct wl_seat_listener *orig = (struct wl_seat_listener *)implementation;
        g_seat.real_capabilities = orig->capabilities;
        g_seat.real_name = orig->name;
        g_seat.real_data = data;
        return real_fn(proxy, (void (**)(void))&g_hooked_listener, data);
    }
    return real_fn(proxy, implementation, data);
}

struct wl_proxy *wl_proxy_marshal_array_flags(
    struct wl_proxy *proxy, uint32_t opcode,
    const struct wl_interface *interface, uint32_t version,
    uint32_t flags, union wl_argument *args) {
    static marshal_flags_fn real_fn = NULL;
    if (!real_fn) real_fn = (marshal_flags_fn)dlsym(RTLD_NEXT, "wl_proxy_marshal_array_flags");

    const char *cls = proxy ? wl_proxy_get_class(proxy) : NULL;
    // wl_seat opcode 2 is get_touch
    if (cls && strcmp(cls, "wl_seat") == 0 && opcode == 2) {
        return NULL;
    }
    return real_fn(proxy, opcode, interface, version, flags, args);
}
