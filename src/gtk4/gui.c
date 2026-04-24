/* GTK4 Frontend for X3D Toggle
 *
 * Minimal wrapper: Loads declarative UI, binds button IDs to CLI commands.
 * `action_x3dtoggle_X` button -> executes `x3d-toggle X`
 */

#include <adwaita.h>
#include <gtk/gtk.h>
#include "status.h"

extern int socket_send(const char *cmd, char *response, size_t resp_len);
extern size_t scat(char *dest, const char *src, size_t dest_size);
extern int printf_sn(char *buf, size_t size, const char *fmt, ...);

#define BUFF_LINE 256
#define BUFF_INFO 128
#define BUFF_STATE 16

static GtkWidget *lbl_status_dump = NULL;
static GtkStack *content_stack = NULL;

/* Detail Page Labels */
static GtkWidget *lbl_stat_daemon_state = NULL;
static GtkWidget *lbl_stat_ipc_status = NULL;
static GtkWidget *lbl_stat_ebpf_status = NULL;
static GtkWidget *lbl_stat_sched_mode = NULL;

static GtkWidget *lbl_stat_vcache_mode = NULL;
static GtkWidget *lbl_stat_boost_mode = NULL;
static GtkWidget *lbl_stat_ccd_status = NULL;

static GtkWidget *lbl_stat_driver_mode = NULL;
static GtkWidget *lbl_stat_epp_profile = NULL;
static GtkWidget *lbl_stat_cppc_gov = NULL;
static GtkWidget *lbl_stat_smt_status = NULL;
static GtkWidget *lbl_stat_platform = NULL;

static void log_gui_error(const char *msg) {
  char cmd[BUFF_LINE];
  printf_sn(cmd, sizeof(cmd), "GUI_LOG %s", msg);
  socket_send(cmd, NULL, 0);
}

static gboolean update_dashboard_cb(gpointer user_data) {
  (void)user_data;
  if (!lbl_status_dump)
    return G_SOURCE_CONTINUE;

  Status st;
  if (daemon_status(&st) != 0) {
    gtk_label_set_label(GTK_LABEL(lbl_status_dump), "Error fetching status.");
    return G_SOURCE_CONTINUE;
  }

  /* Update Dashboard Summary */
  char display[BUFF_LINE * 2];
  printf_sn(display, sizeof(display),
            "<b>Daemon State:</b>  %s\n"
            "<b>Detection:</b>     %s\n"
            "<b>Refresh Rate:</b>   %.1fs",
            st.daemon_state, st.ebpf_status, st.refresh_interval);
  gtk_label_set_markup(GTK_LABEL(lbl_status_dump), display);

  /* Update Detail Pages */
  if (lbl_stat_daemon_state) gtk_label_set_text(GTK_LABEL(lbl_stat_daemon_state), st.daemon_state);
  if (lbl_stat_ipc_status) gtk_label_set_text(GTK_LABEL(lbl_stat_ipc_status), st.ipc_status);
  if (lbl_stat_ebpf_status) gtk_label_set_text(GTK_LABEL(lbl_stat_ebpf_status), st.ebpf_status);
  if (lbl_stat_sched_mode) gtk_label_set_text(GTK_LABEL(lbl_stat_sched_mode), st.d_mode);

  if (lbl_stat_vcache_mode) gtk_label_set_text(GTK_LABEL(lbl_stat_vcache_mode), st.c_mode);
  if (lbl_stat_boost_mode) gtk_label_set_text(GTK_LABEL(lbl_stat_boost_mode), st.st_buff);
  if (lbl_stat_ccd_status) gtk_label_set_text(GTK_LABEL(lbl_stat_ccd_status), st.ccd_state);

  if (lbl_stat_driver_mode) gtk_label_set_text(GTK_LABEL(lbl_stat_driver_mode), st.d_buff);
  if (lbl_stat_epp_profile) gtk_label_set_text(GTK_LABEL(lbl_stat_epp_profile), st.epp);
  if (lbl_stat_cppc_gov) gtk_label_set_text(GTK_LABEL(lbl_stat_cppc_gov), st.gov);
  if (lbl_stat_smt_status) gtk_label_set_text(GTK_LABEL(lbl_stat_smt_status), st.smt);
  if (lbl_stat_platform) gtk_label_set_text(GTK_LABEL(lbl_stat_platform), st.plat);

  return G_SOURCE_CONTINUE;
}

static void on_action_clicked(GtkButton *btn, gpointer user_data) {
  (void)btn;
  const char *cmd = (const char *)user_data;
  char ipc_cmd[BUFF_LINE];

  if (strcmp(cmd, "cache") == 0 || strcmp(cmd, "frequency") == 0) {
    printf_sn(ipc_cmd, sizeof(ipc_cmd), "SET_MODE %s", cmd);
  } else if (strcmp(cmd, "auto") == 0 || strcmp(cmd, "default") == 0) {
    printf_sn(ipc_cmd, sizeof(ipc_cmd), "SET_DAEMON %s", cmd);
  } else if (strcmp(cmd, "stop") == 0) {
    scat(ipc_cmd, "DAEMON_DISABLE", sizeof(ipc_cmd));
  } else {
    scat(ipc_cmd, "DAEMON_INFO", sizeof(ipc_cmd));
  }

  if (socket_send(ipc_cmd, NULL, 0) != 0) {
    log_gui_error("Failed to send IPC command from GUI");
  }
}

static GtkWidget *add_nav_row(GtkListBox *list, const char *id,
                              const char *title) {
  GtkWidget *row = gtk_list_box_row_new();
  gtk_widget_set_name(row, id);
  GtkWidget *lbl = gtk_label_new(title);
  gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
  gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), lbl);
  gtk_list_box_append(list, row);
  return row;
}

static void on_nav_row_selected(GtkListBox *box, GtkListBoxRow *row,
                                gpointer user_data) {
  (void)box;
  if (!row)
    return;
  GtkStack *stack = GTK_STACK(user_data);
  const char *id = gtk_widget_get_name(GTK_WIDGET(row));
  gtk_stack_set_visible_child_name(stack, id);
}

static void on_status_nav_clicked(GtkGestureClick *gesture, int n_press, double x, double y, gpointer user_data) {
  (void)gesture; (void)n_press; (void)x; (void)y;
  const char *target = (const char *)user_data;
  if (content_stack) {
    gtk_stack_set_visible_child_name(content_stack, target);
  }
}

static void on_back_clicked(GtkButton *btn, gpointer user_data) {
  (void)btn; (void)user_data;
  if (content_stack) {
    gtk_stack_set_visible_child_name(content_stack, "dashboard");
  }
}

static void on_app_activate(GtkApplication *app, gpointer user_data) {
  (void)user_data;

  /* Silence libadwaita dark theme warnings and enforce dark mode */
  AdwStyleManager *style_manager = adw_style_manager_get_default();
  adw_style_manager_set_color_scheme(style_manager,
                                     ADW_COLOR_SCHEME_FORCE_DARK);

  /* Load CSS */
  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_resource(provider,
                                      "/org/x3d-toggle/gui/theme.css");
  gtk_style_context_add_provider_for_display(
      gdk_display_get_default(), GTK_STYLE_PROVIDER(provider),
      GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  /* Load UI */
  GtkBuilder *builder =
      gtk_builder_new_from_resource("/org/x3d-toggle/gui/x3d-toggle.ui");

  GtkWindow *window =
      GTK_WINDOW(gtk_builder_get_object(builder, "main_window"));
  gtk_window_set_application(window, app);

  lbl_status_dump =
      GTK_WIDGET(gtk_builder_get_object(builder, "lbl_status_dump"));

  /* Bind Navigation */
  GtkListBox *sidebar_top =
      GTK_LIST_BOX(gtk_builder_get_object(builder, "sidebar_list"));
  GtkListBox *sidebar_bottom =
      GTK_LIST_BOX(gtk_builder_get_object(builder, "sidebar_bottom_list"));
  content_stack = GTK_STACK(gtk_builder_get_object(builder, "content_stack"));

  add_nav_row(sidebar_top, "dashboard", "Dashboard");
  add_nav_row(sidebar_bottom, "settings", "Settings");
  add_nav_row(sidebar_bottom, "docs", "Documentation");

  g_signal_connect(sidebar_top, "row-selected", G_CALLBACK(on_nav_row_selected),
                   content_stack);
  g_signal_connect(sidebar_bottom, "row-selected", G_CALLBACK(on_nav_row_selected),
                   content_stack);

  /* Bind Dashboard Status Nav Indicators */
  const char *status_navs[] = {"nav_daemon_status", "daemon_status_page",
                               "nav_vcache_status", "vcache_status_page",
                               "nav_cppc_status",   "cppc_status_page", NULL};
  for (int i = 0; status_navs[i] != NULL; i += 2) {
    GObject *row = gtk_builder_get_object(builder, status_navs[i]);
    if (row) {
      GtkGesture *click = gtk_gesture_click_new();
      g_signal_connect(click, "pressed", G_CALLBACK(on_status_nav_clicked), (gpointer)status_navs[i+1]);
      gtk_widget_add_controller(GTK_WIDGET(row), GTK_EVENT_CONTROLLER(click));
    }
  }

  /* Bind Detail Labels */
  lbl_stat_daemon_state = GTK_WIDGET(gtk_builder_get_object(builder, "lbl_stat_daemon_state"));
  lbl_stat_ipc_status   = GTK_WIDGET(gtk_builder_get_object(builder, "lbl_stat_ipc_status"));
  lbl_stat_ebpf_status  = GTK_WIDGET(gtk_builder_get_object(builder, "lbl_stat_ebpf_status"));
  lbl_stat_sched_mode   = GTK_WIDGET(gtk_builder_get_object(builder, "lbl_stat_sched_mode"));
  lbl_stat_vcache_mode  = GTK_WIDGET(gtk_builder_get_object(builder, "lbl_stat_vcache_mode"));
  lbl_stat_boost_mode   = GTK_WIDGET(gtk_builder_get_object(builder, "lbl_stat_boost_mode"));
  lbl_stat_ccd_status   = GTK_WIDGET(gtk_builder_get_object(builder, "lbl_stat_ccd_status"));
  lbl_stat_driver_mode  = GTK_WIDGET(gtk_builder_get_object(builder, "lbl_stat_driver_mode"));
  lbl_stat_epp_profile  = GTK_WIDGET(gtk_builder_get_object(builder, "lbl_stat_epp_profile"));
  lbl_stat_cppc_gov     = GTK_WIDGET(gtk_builder_get_object(builder, "lbl_stat_cppc_gov"));
  lbl_stat_smt_status   = GTK_WIDGET(gtk_builder_get_object(builder, "lbl_stat_smt_status"));
  lbl_stat_platform     = GTK_WIDGET(gtk_builder_get_object(builder, "lbl_stat_platform"));

  /* Deselect from one list when something is selected in the other */
  g_signal_connect_swapped(sidebar_top, "row-selected", 
                           G_CALLBACK(gtk_list_box_unselect_all), sidebar_bottom);
  g_signal_connect_swapped(sidebar_bottom, "row-selected", 
                           G_CALLBACK(gtk_list_box_unselect_all), sidebar_top);

  /* Select Dashboard by default */
  GtkListBoxRow *first_row = gtk_list_box_get_row_at_index(sidebar_top, 0);
  if (first_row) {
    gtk_list_box_select_row(sidebar_top, first_row);
    on_nav_row_selected(sidebar_top, first_row, content_stack);
  }

  /* Bind Actions automatically by ID (action_x3dtoggle_<cmd>) */
  const char *actions[] = {"cache",  "frequency", "dual",  "default",
                           "auto",   "wake",      "sleep", "stop",
                           "enable", "start",     NULL};

  for (int i = 0; actions[i] != NULL; i++) {
    char btn_id[64];
    printf_sn(btn_id, sizeof(btn_id), "action_x3dtoggle_%s", actions[i]);
    GObject *obj = gtk_builder_get_object(builder, btn_id);
    if (obj) {
      char *cmd_copy = g_strdup(actions[i]);
      g_signal_connect_data(obj, "clicked", G_CALLBACK(on_action_clicked),
                            cmd_copy, (GClosureNotify)g_free, 0);
    }
  }

  /* Bind Back Buttons */
  const char *back_btns[] = {"btn_back_daemon", "btn_back_vcache", "btn_back_cppc", NULL};
  for (int i = 0; back_btns[i] != NULL; i++) {
    GObject *btn = gtk_builder_get_object(builder, back_btns[i]);
    if (btn) {
      g_signal_connect(btn, "clicked", G_CALLBACK(on_back_clicked), NULL);
    }
  }
  gtk_window_present(window);

  /* Start Live Dashboard Polling */
  g_timeout_add_seconds(1, update_dashboard_cb, NULL);
  update_dashboard_cb(NULL); /* Initial fetch */
}

int main(int argc, char **argv) {
  AdwApplication *app =
      adw_application_new("org.x3d-toggle.gui", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(on_app_activate), NULL);
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
}
