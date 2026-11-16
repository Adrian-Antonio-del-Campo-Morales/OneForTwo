// Responsibility: native publication in Ubuntu. Nautilus consumes
// x-special/gnome-copied-files and text/uri-list with file:// local and
// readable; GTK3 publishes those destinations in both X11 and Wayland.
#include "platform/clipboard.hpp"

#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace o2f {
namespace {

std::atomic<bool> g_stop{false};
std::string g_gnomePayload;
std::string g_uriPayload;
std::string g_textPayload;
GtkClipboard* g_clipboard = nullptr;

void onSignal(int) { g_stop.store(true); }

std::string uriEncodePath(const std::string& utf8Path) {
    static const char* kHex = "0123456789ABCDEF";
    std::string out;
    out.reserve(utf8Path.size() + 8);
    for (unsigned char c : utf8Path) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '~' || c == '/') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(kHex[c >> 4]);
            out.push_back(kHex[c & 0x0f]);
        }
    }
    return out;
}

void onGet(GtkClipboard*, GtkSelectionData* selection, guint info, gpointer) {
    const std::string* payload = nullptr;
    switch (info) {
        case 0: payload = &g_gnomePayload; break;
        case 1: payload = &g_uriPayload; break;
        case 2: payload = &g_textPayload; break;
        default: return;
    }
    gtk_selection_data_set(selection, gtk_selection_data_get_target(selection), 8,
                           reinterpret_cast<const guchar*>(payload->data()),
                           static_cast<gint>(payload->size()));
}

void onClear(GtkClipboard*, gpointer) {}

bool publishSelection() {
    GtkTargetEntry targets[3];
    std::memset(targets, 0, sizeof(targets));
    targets[0].target = const_cast<gchar*>("x-special/gnome-copied-files");
    targets[0].info = 0;
    targets[1].target = const_cast<gchar*>("text/uri-list");
    targets[1].info = 1;
    targets[2].target = const_cast<gchar*>("UTF8_STRING");
    targets[2].info = 2;
    return gtk_clipboard_set_with_data(g_clipboard, targets, 3, onGet, onClear, nullptr);
}

struct PublishAction {
    GtkWidget* status = nullptr;
    bool published = false;
};

void onPublishClicked(GtkButton* button, gpointer data) {
    auto& action = *static_cast<PublishAction*>(data);
    if (!publishSelection()) {
        gtk_label_set_text(GTK_LABEL(action.status), "The desk rejected the offer.");
        return;
    }
    action.published = true;
    gtk_label_set_text(GTK_LABEL(action.status),
                       "Files prepared. Go back to Nautilus and paste with Ctrl+V.\n"
                       "Keep this window open until you finish the glue.");
    gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
    std::puts("offer GTK published after local action of the user");
    std::fflush(stdout);
}

gboolean onPublishWindowClose(GtkWidget*, GdkEvent*, gpointer) {
    g_stop.store(true);
    gtk_main_quit();
    return TRUE;
}

gboolean tick(gpointer) {
    if (g_stop.load()) {
        gtk_main_quit();
        return FALSE;
    }
    return TRUE;
}

}  // namespace

bool clipboardAvailable(std::string& why) {
    why.clear();
    return true;
}

bool onDemandClipboardAvailable(std::string& why) {
    why =
        "Linux: this implementation only publishes file:// local routes. Delivery under "
        "claim would require that the content be displayed by a backend GIO/GVfs or FUSE which "
        "Nautilus can read; is not implemented in the experiment";
    return false;
}

int runClipboardPublisher(const ClipboardRequest& request, bool flushOnExit, std::string& err) {
    err.clear();
    (void)flushOnExit;
    if (!request.localPaths.empty() && !request.offer.entries.empty()) {
        err = "a publication cannot combine local routes and remote manifest";
        return 1;
    }
    if (request.localPaths.empty()) {
        err = "Linux only supports publication of files already present on disk; "
              "use cache mode";
        return 2;
    }

    std::string uris;
    std::string text;
    std::string gnomeCopied = "copy";
    for (const std::string& path : request.localPaths) {
        if (path.empty() || path.front() != '/') {
            err = "the published route must be absolute";
            return 1;
        }
        const std::string uri = "file://" + uriEncodePath(path);
        uris += uri + "\r\n";
        text += uri + "\n";
        // Nautilus divides by LF and rejects empty components: this format
        // does not use CRLF or carry a final jump, unlike text/uri-list.
        gnomeCopied += "\n" + uri;
    }
    g_uriPayload = uris;
    g_gnomePayload = std::move(gnomeCopied);
    g_textPayload = text;

    g_stop.store(false);
    int argc = 1;
    char program[] = "oneintwo-files";
    char* argv[] = {program, nullptr};
    // gtk_init_check waits char***; the direction of a pointer is passed to the array.
    char** argvPointer = argv;
    if (!gtk_init_check(&argc, &argvPointer)) {
        err = "Could not open graphical session (DISPLAY/WAYLAND_DISPLAY)";
        return 1;
    }
    g_clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    bool wayland = false;
#ifdef GDK_WINDOWING_WAYLAND
    wayland = GDK_IS_WAYLAND_DISPLAY(gdk_display_get_default());
#endif
    PublishAction action;
    GtkWidget* window = nullptr;
    if (wayland) {
        // Wayland requires a local event valid for set_selection.
        // process without window sent serial=0 and Nautilus did not receive the offer.
        window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_window_set_title(GTK_WINDOW(window), "OneInTwo — File Testing");
        gtk_container_set_border_width(GTK_CONTAINER(window), 16);
        GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
        gtk_container_add(GTK_CONTAINER(window), box);
        action.status = gtk_label_new(
            "Press Publish files and then paste in Nautilus.\n"
            "Close this window ends the test publication.");
        gtk_box_pack_start(GTK_BOX(box), action.status, TRUE, TRUE, 0);
        GtkWidget* button = gtk_button_new_with_label("Publish Files");
        gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
        g_signal_connect(button, "clicked", G_CALLBACK(onPublishClicked), &action);
        g_signal_connect(window, "delete-event", G_CALLBACK(onPublishWindowClose), nullptr);
        gtk_widget_show_all(window);
        std::puts("backend=gtk-wayland; waiting for local action in Publishing files");
        std::fflush(stdout);
    } else if (!publishSelection()) {
        err = "the desktop rejected the offer of files on the clipboard";
        return 1;
    }
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
    g_timeout_add(200, tick, nullptr);
    gtk_main();
    gtk_clipboard_clear(g_clipboard);
    g_clipboard = nullptr;
    if (window != nullptr) {
        gtk_widget_destroy(window);
        if (!action.published) {
            err = "publication cancelled before local user action";
            return 1;
        }
    }
    return 0;
}

}  // namespace o2f
