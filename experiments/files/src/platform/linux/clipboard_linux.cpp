// Responsabilidad: publicación nativa en Ubuntu. Nautilus consume
// x-special/gnome-copied-files y text/uri-list con rutas file:// locales y
// legibles; GTK3 publica esos destinos tanto en X11 como en Wayland.
#include "platform/clipboard.hpp"

#include <gtk/gtk.h>

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <string>
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
        "Linux: esta implementación solo publica rutas file:// locales. La entrega bajo "
        "demanda exigiría exponer el contenido mediante un backend GIO/GVfs o FUSE que "
        "Nautilus pueda leer; no está implementado en el experimento";
    return false;
}

int runClipboardPublisher(const ClipboardRequest& request, bool flushOnExit, std::string& err) {
    err.clear();
    (void)flushOnExit;
    if (!request.localPaths.empty() && !request.offer.entries.empty()) {
        err = "una publicación no puede combinar rutas locales y manifiesto remoto";
        return 1;
    }
    if (request.localPaths.empty()) {
        err = "Linux solo admite publicación de archivos ya presentes en disco; "
              "use el modo caché";
        return 2;
    }

    std::string uris;
    std::string text;
    for (const std::string& path : request.localPaths) {
        if (path.empty() || path.front() != '/') {
            err = "la ruta publicada debe ser absoluta";
            return 1;
        }
        const std::string uri = "file://" + uriEncodePath(path);
        uris += uri + "\r\n";
        text += uri + "\n";
    }
    g_uriPayload = uris;
    g_gnomePayload = "copy\n" + uris;
    g_textPayload = text;

    g_stop.store(false);
    int argc = 1;
    char program[] = "oneintwo-files";
    char* argv[] = {program, nullptr};
    // gtk_init_check espera char***; se pasa la dirección de un puntero al array.
    char** argvPointer = argv;
    if (!gtk_init_check(&argc, &argvPointer)) {
        err = "no se pudo abrir la sesión gráfica (DISPLAY/WAYLAND_DISPLAY)";
        return 1;
    }
    g_clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    GtkTargetEntry targets[3];
    std::memset(targets, 0, sizeof(targets));
    targets[0].target = const_cast<gchar*>("x-special/gnome-copied-files");
    targets[0].info = 0;
    targets[1].target = const_cast<gchar*>("text/uri-list");
    targets[1].info = 1;
    targets[2].target = const_cast<gchar*>("UTF8_STRING");
    targets[2].info = 2;

    if (!gtk_clipboard_set_with_data(g_clipboard, targets, 3, onGet, onClear, nullptr)) {
        err = "el escritorio rechazó la oferta de archivos en el portapapeles";
        return 1;
    }
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);
    g_timeout_add(200, tick, nullptr);
    gtk_main();
    gtk_clipboard_clear(g_clipboard);
    g_clipboard = nullptr;
    return 0;
}

}  // namespace o2f
