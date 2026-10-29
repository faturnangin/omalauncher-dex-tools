#include <gtk/gtk.h>
#include <stdlib.h>
#include <unistd.h>

struct app_entry {
    const char *label;
    const char *sub;
    const char *cmd;
};

static const struct app_entry apps[] = {
    {"Terminal", "Foot Wayland terminal", "foot"},
    {"Antigravity CLI", "Coding agent in terminal", "foot --app-id=dev.omalauncher.agy -T Antigravity -e bash -lc 'agy; exec bash'"},
    {"Antigravity IDE", "Full graphical IDE", "antigravity-ide"},
    {"Firefox", "Desktop browser with DevTools", "/usr/local/bin/firefox"},
    {"HeidiSQL", "SQL database manager", "/usr/local/bin/heidisql"},
    {"btop++", "System resource monitor", "foot --app-id=btop -T btop -e /usr/local/bin/btop"},
    {"Cliamp", "Terminal music player", "foot --app-id=cliamp -T Cliamp -e /usr/local/bin/cliamp"},
    {"Files", "Omarchy File Manager", "/usr/local/bin/omarchy-files"},
};

static void on_launch_clicked(GtkWidget *btn, gpointer data) {
    const char *cmd = (const char *)data;
    if (fork() == 0) {
        setsid();
        execl("/bin/bash", "bash", "-lc", cmd, NULL);
        _exit(0);
    }
}

int main(int argc, char **argv) {
    setenv("GDK_BACKEND", "wayland", 1);
    gtk_init(&argc, &argv);

    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        "window { background-color: #16161e; color: #c0caf5; }"
        "button { background-image: none; background-color: #1f2335; color: #c0caf5; border: 1px solid #3b4261; border-radius: 10px; padding: 12px; }"
        "button:hover { background-color: #292e42; border-color: #7aa2f7; }"
        ".title { font-family: monospace; font-size: 16px; font-weight: bold; color: #7aa2f7; }"
        ".app-name { font-family: monospace; font-size: 14px; font-weight: bold; color: #c0caf5; }"
        ".app-sub { font-family: monospace; font-size: 11px; color: #565f89; }",
        -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(), GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), "Omarchy Linux Apps");
    gtk_window_set_default_size(GTK_WINDOW(win), 520, 380);
    g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(vbox, 18);
    gtk_widget_set_margin_end(vbox, 18);
    gtk_widget_set_margin_top(vbox, 16);
    gtk_widget_set_margin_bottom(vbox, 16);
    gtk_container_add(GTK_CONTAINER(win), vbox);

    GtkWidget *hdr = gtk_label_new("Linux Apps (opens each app in its own DeX window)");
    gtk_style_context_add_class(gtk_widget_get_style_context(hdr), "title");
    gtk_widget_set_halign(hdr, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), hdr, FALSE, FALSE, 0);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_row_homogeneous(GTK_GRID(grid), TRUE);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), grid, TRUE, TRUE, 0);

    int n = sizeof(apps) / sizeof(apps[0]);
    for (int i = 0; i < n; i++) {
        GtkWidget *btn = gtk_button_new();
        GtkWidget *b_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        GtkWidget *lbl1 = gtk_label_new(apps[i].label);
        GtkWidget *lbl2 = gtk_label_new(apps[i].sub);
        gtk_style_context_add_class(gtk_widget_get_style_context(lbl1), "app-name");
        gtk_style_context_add_class(gtk_widget_get_style_context(lbl2), "app-sub");
        gtk_widget_set_halign(lbl1, GTK_ALIGN_START);
        gtk_widget_set_halign(lbl2, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(b_box), lbl1, FALSE, FALSE, 0);
        gtk_box_pack_start(GTK_BOX(b_box), lbl2, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(btn), b_box);
        g_signal_connect(btn, "clicked", G_CALLBACK(on_launch_clicked), (gpointer)apps[i].cmd);
        gtk_grid_attach(GTK_GRID(grid), btn, i % 2, i / 2, 1, 1);
    }

    gtk_widget_show_all(win);
    gtk_main();
    return 0;
}
