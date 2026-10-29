#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <wayland-client.h>

#define XDG_TOPLEVEL_STATE_MAXIMIZED 1
#define WL_SEAT_CAPABILITY_TOUCH 4
#define WL_SURFACE_ATTACH 1
#define WL_SURFACE_SET_BUFFER_SCALE 8

typedef int (*add_listener_fn)(struct wl_proxy *proxy, void (**implementation)(void), void *data);
typedef struct wl_proxy *(*marshal_array_flags_fn)(struct wl_proxy *proxy, uint32_t opcode,
                                                  const struct wl_interface *interface,
                                                  uint32_t version, uint32_t flags,
                                                  union wl_argument *args);
typedef struct wl_proxy *(*marshal_flags_fn)(struct wl_proxy *proxy, uint32_t opcode,
                                            const struct wl_interface *interface,
                                            uint32_t version, uint32_t flags, ...);

static add_listener_fn g_real_add_listener = NULL;
static marshal_array_flags_fn g_real_marshal_array_flags = NULL;

static int32_t g_output_scale = 2;

/* --- xdg_toplevel hook --- */
static void (*g_real_toplevel_configure)(void *, void *, int32_t, int32_t, struct wl_array *) = NULL;
static void *g_hooked_toplevel_listener[8];

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
        if (g_real_toplevel_configure) {
            g_real_toplevel_configure(data, xdg_toplevel, width, height, &augmented);
        }
    } else {
        if (g_real_toplevel_configure) {
            g_real_toplevel_configure(data, xdg_toplevel, width, height, states);
        }
    }
}

/* --- wl_seat hook (Touch masking) --- */
static void (*g_real_seat_capabilities)(void *, struct wl_seat *, uint32_t) = NULL;
static void *g_hooked_seat_listener[4];

static void seat_capabilities_wrapper(void *data, struct wl_seat *seat, uint32_t caps) {
    /* Mask out WL_SEAT_CAPABILITY_TOUCH (4) so GTK and Firefox only see Pointer + Keyboard.
     * This forces Omalauncher to treat Firefox with full pointer emulation:
     * - Single tap = Left Click (immediately focuses URL bar, tabs, links, buttons)
     * - Press & hold = Right Click (context menu)
     * - Drag = Smooth Inertial Scrolling
     */
    caps &= ~WL_SEAT_CAPABILITY_TOUCH;
    if (g_real_seat_capabilities) {
        g_real_seat_capabilities(data, seat, caps);
    }
}

/* --- wl_output hook (Scale detection) --- */
static void (*g_real_output_scale)(void *, struct wl_output *, int32_t) = NULL;
static void *g_hooked_output_listener[8];

static void output_scale_wrapper(void *data, struct wl_output *output, int32_t factor) {
    if (factor > 0) {
        g_output_scale = factor;
    }
    if (g_real_output_scale) {
        g_real_output_scale(data, output, factor);
    }
}

int wl_proxy_add_listener(struct wl_proxy *proxy, void (**implementation)(void), void *data) {
    if (!g_real_add_listener) {
        g_real_add_listener = (add_listener_fn)dlsym(RTLD_NEXT, "wl_proxy_add_listener");
    }

    const char *cls = proxy ? wl_proxy_get_class(proxy) : NULL;
    if (cls && implementation) {
        if (strcmp(cls, "xdg_toplevel") == 0) {
            void **funcs = (void **)implementation;
            g_real_toplevel_configure = (void (*)(void *, void *, int32_t, int32_t, struct wl_array *))funcs[0];
            g_hooked_toplevel_listener[0] = (void *)toplevel_configure_wrapper;
            for (int i = 1; i < 8; i++) g_hooked_toplevel_listener[i] = funcs[i];
            return g_real_add_listener(proxy, (void (**)(void))g_hooked_toplevel_listener, data);
        } else if (strcmp(cls, "wl_seat") == 0) {
            void **funcs = (void **)implementation;
            g_real_seat_capabilities = (void (*)(void *, struct wl_seat *, uint32_t))funcs[0];
            g_hooked_seat_listener[0] = (void *)seat_capabilities_wrapper;
            g_hooked_seat_listener[1] = funcs[1];
            return g_real_add_listener(proxy, (void (**)(void))g_hooked_seat_listener, data);
        } else if (strcmp(cls, "wl_output") == 0) {
            void **funcs = (void **)implementation;
            /* wl_output_listener: 0:geometry, 1:mode, 2:done, 3:scale */
            g_real_output_scale = (void (*)(void *, struct wl_output *, int32_t))funcs[3];
            for (int i = 0; i < 8; i++) g_hooked_output_listener[i] = funcs[i];
            g_hooked_output_listener[3] = (void *)output_scale_wrapper;
            return g_real_add_listener(proxy, (void (**)(void))g_hooked_output_listener, data);
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
        /* Drop any rogue get_touch calls on wl_seat */
        if (strcmp(cls, "wl_seat") == 0 && opcode == 2 /* get_touch */) {
            return NULL;
        }

        /* Subsurface scale synchronization:
         * Ensure EVERY wl_surface (including WebRender content subsurface wl_surface#31)
         * has set_buffer_scale called before attaching a buffer.
         * This completely fixes the 2x scale blowup, pixelation, and cropping!
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
