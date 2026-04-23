/* GTK4 Frontend for X3D Toggle
 *
 * Minimal wrapper: Loads declarative UI, binds button IDs to CLI commands.
 * `action_x3dtoggle_X` button -> executes `x3d-toggle X`
 */

#include <gtk/gtk.h>
#include <adwaita.h>
#include <stdlib.h>
#include <string.h>

static GtkWidget *lbl_status_dump = NULL;

static void log_gui_error(const char *msg) {
    pid_t pid = fork();
    if (pid == 0) {
        execlp("x3d-toggle", "x3d-toggle", "gui-log", msg, (char *)NULL);
        _exit(1);
    }
}

static gboolean update_dashboard_cb(gpointer user_data) {
    (void)user_data;
    if (!lbl_status_dump) return G_SOURCE_CONTINUE;

    FILE *fp = popen("x3d-toggle status", "r");
    if (!fp) {
        log_gui_error("Failed to execute popen for x3d-toggle status");
        gtk_label_set_label(GTK_LABEL(lbl_status_dump), "Error: Failed to launch x3d-toggle status.");
        return G_SOURCE_CONTINUE;
    }

    char buf[4096] = {0};
    size_t bytes_read = fread(buf, 1, sizeof(buf) - 1, fp);
    if (ferror(fp)) {
        log_gui_error("fread encountered an error while reading daemon status");
    }
    pclose(fp);

    if (bytes_read > 0) {
        gtk_label_set_label(GTK_LABEL(lbl_status_dump), buf);
    } else {
        gtk_label_set_label(GTK_LABEL(lbl_status_dump), "Daemon offline or unreachable.");
    }
    return G_SOURCE_CONTINUE;
}

static void on_action_clicked(GtkButton *btn, gpointer user_data) {
    (void)btn;
    const char *cmd = (const char *)user_data;

    pid_t pid = fork();
    if (pid == 0) {
        execlp("x3d-toggle", "x3d-toggle", cmd, (char *)NULL);
        _exit(1);
    } else if (pid < 0) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "fork() failed when attempting to execute command: %s", cmd);
        log_gui_error(err_msg);
        g_printerr("Error: %s\n", err_msg);
    }
}

static GtkWidget* add_nav_row(GtkListBox *list, const char *id, const char *title) {
    GtkWidget *row = gtk_list_box_row_new();
    gtk_widget_set_name(row, id);
    GtkWidget *lbl = gtk_label_new(title);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), lbl);
    gtk_list_box_append(list, row);
    return row;
}

static void on_nav_row_selected(GtkListBox *box, GtkListBoxRow *row, gpointer user_data) {
    if (!row) return;
    GtkStack *stack = GTK_STACK(user_data);
    const char *id = gtk_widget_get_name(GTK_WIDGET(row));
    gtk_stack_set_visible_child_name(stack, id);
}

static void on_app_activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;

    /* Silence libadwaita dark theme warnings and enforce dark mode */
    AdwStyleManager *style_manager = adw_style_manager_get_default();
    adw_style_manager_set_color_scheme(style_manager, ADW_COLOR_SCHEME_FORCE_DARK);

    /* Load CSS */
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_resource(provider, "/org/x3d-toggle/gui/theme.css");
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    /* Load UI */
    GtkBuilder *builder = gtk_builder_new_from_resource("/org/x3d-toggle/gui/x3d-toggle.ui");
    
    GtkWindow *window = GTK_WINDOW(gtk_builder_get_object(builder, "main_window"));
    gtk_window_set_application(window, app);

    lbl_status_dump = GTK_WIDGET(gtk_builder_get_object(builder, "lbl_status_dump"));

    /* Bind Navigation */
    GtkListBox *sidebar = GTK_LIST_BOX(gtk_builder_get_object(builder, "sidebar_list"));
    GtkStack *stack = GTK_STACK(gtk_builder_get_object(builder, "content_stack"));
    
    add_nav_row(sidebar, "dashboard", "Dashboard");
    add_nav_row(sidebar, "modes", "Hardware Modes");
    add_nav_row(sidebar, "daemon", "Daemon Settings");
    
    g_signal_connect(sidebar, "row-selected", G_CALLBACK(on_nav_row_selected), stack);

    /* Bind Actions automatically by ID (action_x3dtoggle_<cmd>) */
    const char *actions[] = {
        "cache", "frequency", "dual", "default", "auto",
        "wake", "sleep", "stop", "enable", "start", NULL
    };

    for (int i = 0; actions[i] != NULL; i++) {
        char btn_id[64];
        snprintf(btn_id, sizeof(btn_id), "action_x3dtoggle_%s", actions[i]);
        GObject *obj = gtk_builder_get_object(builder, btn_id);
        if (obj) {
            char *cmd_copy = g_strdup(actions[i]);
            g_signal_connect_data(obj, "clicked", G_CALLBACK(on_action_clicked),
                                  cmd_copy, (GClosureNotify)g_free, 0);
        }
    }

    g_object_unref(builder);
    gtk_window_present(window);

    /* Start Live Dashboard Polling */
    g_timeout_add_seconds(1, update_dashboard_cb, NULL);
    update_dashboard_cb(NULL); /* Initial fetch */
}

int main(int argc, char **argv) {
    AdwApplication *app = adw_application_new("org.x3d-toggle.gui", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_app_activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
