#include <curl/curl.h>
#include <gtk/gtk.h>
#include <json-glib/json-glib.h>
#include <stdbool.h>

#define HOST "example.com"

#define MARGIN 8
#define DEFAULT_WINDOW_WIDTH 400
#define DEFAULT_WINDOW_HEIGHT 600

#define auto __auto_type

G_DEFINE_AUTOPTR_CLEANUP_FUNC(CURL, curl_easy_cleanup);
typedef struct curl_slist CurlSList;
G_DEFINE_AUTOPTR_CLEANUP_FUNC(CurlSList, curl_slist_free_all);

typedef struct {
    // Fetching is done on a different thread,
    // so it needs a separate curl handle.
    CURL *fetch_curl;
    CURL *post_curl;
    GPtrArray *pending_spyyts;
    GMutex pending_spyyts_mutex;
    GtkWidget *feed;
} State;

static GtkWidget *spyyt_widget_new(gchar const *text) {
    auto spyyt = gtk_label_new(text);
    gtk_label_set_selectable(GTK_LABEL(spyyt), true);
    gtk_widget_set_margin_start(spyyt, MARGIN);
    gtk_widget_set_margin_end(spyyt, MARGIN);
    gtk_widget_set_margin_top(spyyt, MARGIN);
    gtk_widget_set_margin_bottom(spyyt, MARGIN);
    gtk_widget_set_halign(spyyt, GTK_ALIGN_START);
    return spyyt;
}

static void show_spyyt(gchar const *text, State *state) {
    auto spyyt = spyyt_widget_new(text);
    gtk_container_add(GTK_CONTAINER(state->feed), spyyt);
    gtk_widget_show(spyyt);
}

static gboolean show_pending_spyyts(State *state) {
    g_mutex_lock(&state->pending_spyyts_mutex);
    g_ptr_array_foreach(state->pending_spyyts, (GFunc)show_spyyt, state);
    g_ptr_array_set_size(state->pending_spyyts, 0);
    g_mutex_unlock(&state->pending_spyyts_mutex);

    auto feed_scroll =
        gtk_widget_get_parent(gtk_widget_get_parent(state->feed));
    auto adjustment =
        gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(feed_scroll));
    gtk_adjustment_set_value(adjustment, gtk_adjustment_get_upper(adjustment));

    return false;
}

static size_t
receive_spyyt_batch(void *data, size_t size, size_t nmemb, State *state) {
    auto byte_count = size * nmemb;

    g_autoptr(JsonParser) parser = json_parser_new();
    json_parser_load_from_data(parser, data, byte_count, NULL);
    g_autoptr(JsonReader) reader =
        json_reader_new(json_parser_get_root(parser));
    g_mutex_lock(&state->pending_spyyts_mutex);
    for (gint i = 0; json_reader_read_element(reader, i); ++i) {
        json_reader_read_member(reader, "text");
        auto text = json_reader_get_string_value(reader);
        json_reader_end_member(reader);
        g_ptr_array_add(state->pending_spyyts, g_strdup(text));
        json_reader_end_element(reader);
    }
    g_mutex_unlock(&state->pending_spyyts_mutex);

    gdk_threads_add_idle((GSourceFunc)show_pending_spyyts, state);

    return byte_count;
}

static void *fetch(State *state) {
    auto curl = state->fetch_curl;
    curl_easy_setopt(curl, CURLOPT_URL, "wss://" HOST "/api/spyyts");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receive_spyyt_batch);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, state);
    curl_easy_perform(curl);

    return NULL;
}

static gchar *serialize_spyyt(gchar const *text) {
    g_autoptr(JsonBuilder) builder = json_builder_new();
    json_builder_begin_object(builder);
    json_builder_set_member_name(builder, "text");
    json_builder_add_string_value(builder, text);
    json_builder_end_object(builder);
    g_autoptr(JsonGenerator) generator = json_generator_new();
    auto root = json_builder_get_root(builder);
    json_generator_set_root(generator, root);
    return json_generator_to_data(generator, NULL);
}

static void post_spyyt(GtkWidget *widget, State *state) {
    auto grid = gtk_widget_get_parent(widget);
    auto children = gtk_container_get_children(GTK_CONTAINER(grid));
    auto text_box = GTK_ENTRY(g_list_nth_data(children, 1));
    auto text = gtk_entry_get_text(text_box);
    if (!*text) {
        return;
    }

    g_autofree auto body = serialize_spyyt(text);
    gtk_entry_set_text(text_box, "");

    auto curl = state->post_curl;
    curl_easy_setopt(curl, CURLOPT_URL, "https://" HOST "/api/spyyts");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    g_autoptr(CurlSList) headers =
        curl_slist_append(NULL, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    auto result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        auto root_window = gtk_widget_get_parent(grid);
        auto flags = GTK_DIALOG_DESTROY_WITH_PARENT;
        auto dialog = gtk_message_dialog_new(
            GTK_WINDOW(root_window), flags, GTK_MESSAGE_ERROR,
            GTK_BUTTONS_CLOSE, "Failed to send spyyt: %s",
            curl_easy_strerror(result)
        );
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }
}

static void activate(GtkApplication *app, State *state) {
    auto window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Spytter");
    gtk_window_set_default_size(
        GTK_WINDOW(window), DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT
    );

    auto grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), MARGIN);
    gtk_container_add(GTK_CONTAINER(window), grid);

    auto feed_scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_grid_attach(GTK_GRID(grid), feed_scroll, 0, 0, 2, 1);
    auto feed = gtk_list_box_new();
    state->feed = feed;
    gtk_widget_set_vexpand(feed, true);
    gtk_container_add(GTK_CONTAINER(feed_scroll), feed);

    auto text_box = gtk_entry_new();
    gtk_widget_set_hexpand(text_box, true);
    gtk_entry_set_placeholder_text(
        GTK_ENTRY(text_box), "Write something boring"
    );
    gtk_entry_set_activates_default(GTK_ENTRY(text_box), true);
    gtk_grid_attach(GTK_GRID(grid), text_box, 0, 1, 1, 1);

    auto spyyt_button = gtk_button_new_with_label("SPYYT");
    g_signal_connect(spyyt_button, "clicked", G_CALLBACK(post_spyyt), state);
    gtk_grid_attach(GTK_GRID(grid), spyyt_button, 1, 1, 1, 1);
    gtk_widget_set_can_default(spyyt_button, true);
    gtk_widget_grab_default(spyyt_button);

    gtk_widget_show_all(window);
}

int main(int argc, char *argv[]) {
    g_autoptr(CURL) fetch_curl = curl_easy_init();
    g_assert_nonnull(fetch_curl);
    g_autoptr(CURL) post_curl = curl_easy_init();
    g_assert_nonnull(post_curl);

    auto state = (State){
        .fetch_curl = fetch_curl,
        .post_curl = post_curl,
        .pending_spyyts = g_ptr_array_new(),
        .pending_spyyts_mutex = {},
        .feed = NULL,
    };

    g_autoptr(GtkApplication) app =
        gtk_application_new("org.gtk.example", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), &state);

    auto fetch_thread = g_thread_new("fetch", (GThreadFunc)fetch, &state);

    auto status = g_application_run(G_APPLICATION(app), argc, argv);

    // Stop the fetching thread by making it time out.
    curl_easy_setopt(fetch_curl, CURLOPT_TIMEOUT_MS, 1l);
    g_thread_join(fetch_thread);

    return status;
}
