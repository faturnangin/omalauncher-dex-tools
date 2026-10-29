#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <wayland-client.h>

#define WL_SEAT_CAPABILITY_TOUCH 4
#define WL_SURFACE_ATTACH 1
#define WL_SURFACE_SET_BUFFER_SCALE 8
#define MAX_SEATS 8

typedef int (*add_listener_fn)(struct wl_proxy *proxy, void (**implementation)(void), void *data);
typedef struct wl_proxy *(*marshal_array_flags_fn)(struct wl_proxy *proxy, uint32_t opcode,
                                                  const struct wl_interface *interface,
                                                  uint32_t version, uint32_t flags,
                                                  union wl_argument *args);

static add_listener_fn g_real_add_listener = NULL;
static marshal_array_flags_fn g_real_marshal_array_flags = NULL;

static int32_t g_output_scale = 2;

/* Per-proxy seat lookup to preserve caller's private data pointer across all callbacks */
static struct {
    struct wl_proxy *proxy;
    void (*real_capabilities)(void *, struct wl_seat *, uint32_t);
    void (*real_name)(void *, struct wl_seat *, const char *);
} g_seats[MAX_SEATS];
static int g_seat_count = 0;

static void seat_capabilities_wrapper(void *data, struct wl_seat *seat, uint32_t caps) {
    /* Mask out WL_SEAT_CAPABILITY_TOUCH (0x4).
     * This makes GTK and Firefox see Pointer (mouse) + Keyboard, activating
     * Omalauncher's responsive touch-to-pointer mapper:
     * - Tap = Left click (URL bar focus, tab switch, link/button clicks)
     * - Hold = Right click (context menus)
     * - Drag = Smooth scrolling
     */
    caps &= ~WL_SEAT_CAPABILITY_TOUCH;
    for (int i = 0; i < g_seat_count; i++) {
        if (g_seats[i].proxy == (struct wl_proxy *)seat) {
            if (g_seats[i].real_capabilities) {
                g_seats[i].real_capabilities(data, seat, caps);
            }
            return;
        }
    }
}

static void seat_name_wrapper(void *data, struct wl_seat *seat, const char *name) {
    for (int i = 0; i < g_seat_count; i++) {
        if (g_seats[i].proxy == (struct wl_proxy *)seat) {
            if (g_seats[i].real_name) {
                g_seats[i].real_name(data, seat, name);
            }
            return;
        }
    }
}

static struct wl_seat_listener g_hooked_seat_impl = {
    .capabilities = seat_capabilities_wrapper,
    .name = seat_name_wrapper
};

int wl_proxy_add_listener(struct wl_proxy *proxy, void (**implementation)(void), void *data) {
    if (!g_real_add_listener) {
        g_real_add_listener = (add_listener_fn)dlsym(RTLD_NEXT, "wl_proxy_add_listener");
    }

    const char *cls = proxy ? wl_proxy_get_class(proxy) : NULL;
    if (cls && implementation) {
        if (strcmp(cls, "wl_seat") == 0) {
            void **funcs = (void **)implementation;
            if (g_seat_count < MAX_SEATS) {
                g_seats[g_seat_count].proxy = proxy;
                g_seats[g_seat_count].real_capabilities = (void (*)(void *, struct wl_seat *, uint32_t))funcs[0];
                g_seats[g_seat_count].real_name = (void (*)(void *, struct wl_seat *, const char *))funcs[1];
                g_seat_count++;
            }
            /* Pass original data pointer untouched so funcs receive their expected object */
            return g_real_add_listener(proxy, (void (**)(void))&g_hooked_seat_impl, data);
        }
    }
    return g_real_add_listener(proxy, implementation, data);
}

struct wl_proxy *wl_proxy_marshal_array_flags(struct wl_proxy *proxy, uint32_t opcode,
                                             const struct wl_interface *interface,
                                             uint32_t version, uint32_t flags,
                                             union wl_argument *args) {
    if (!g_real_marshal_array_flags) {
        g_real_marshal_array_flags = (marshal_array_flags_fn)dlsym(RTLD_NEXT, "wl_proxy_marshal_array_flags");
    }

    const char *cls = proxy ? wl_proxy_get_class(proxy) : NULL;
    if (cls) {
        /* Ensure EVERY wl_surface (including WebRender content subsurface)
         * has set_buffer_scale set before attaching a buffer.
         * Eliminates 2x stretch, pixelation, and cropping in both portrait and landscape.
         */
        if (strcmp(cls, "wl_surface") == 0 && opcode == WL_SURFACE_ATTACH) {
            union wl_argument scale_arg;
            scale_arg.i = (g_output_scale > 0) ? g_output_scale : 2;
            g_real_marshal_array_flags(proxy, WL_SURFACE_SET_BUFFER_SCALE, NULL,
                                      wl_proxy_get_version(proxy), 0, &scale_arg);
        }
    }

    return g_real_marshal_array_flags(proxy, opcode, interface, version, flags, args);
}
