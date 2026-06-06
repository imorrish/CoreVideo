#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>
#include "zoom-source.h"
#include "zoom-participant-audio-source.h"
#include "zoom-engine-client.h"
#include "zoom-reconnect.h"
#include "zoom-settings.h"
#include "zoom-settings-dialog.h"
#include "zoom-output-dialog.h"
#include "zoom-diagnostics-dialog.h"
#include "zoom-dock.h"
#include "zoom-iso-recorder.h"
#include "zoom-iso-panel.h"
#include "zoom-control-server.h"
#include "zoom-osc-server.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QDockWidget>
#include <QMainWindow>
#include <QPointer>
#if defined(WIN32)
#include <windows.h>
extern "C" IMAGE_DOS_HEADER __ImageBase;
#endif
#if !defined(WIN32)
#include <csignal>
#endif

static QPointer<ZoomDock> g_dock;
static QPointer<ZoomIsoPanel> g_iso_panel;
static QPointer<ZoomOutputDialog> g_output_panel;
static QPointer<ZoomDiagnosticsDialog> g_diagnostics_panel;
static bool g_frontend_callback_registered = false;
static bool g_shutdown_started = false;

static ZoomDock *ensure_zoom_dock();
static ZoomIsoPanel *ensure_iso_panel();
static ZoomOutputDialog *ensure_output_panel();
static ZoomDiagnosticsDialog *ensure_diagnostics_panel();

static void shutdown_corevideo()
{
    if (g_shutdown_started)
        return;
    g_shutdown_started = true;
    blog(LOG_INFO, "[obs-zoom-plugin] Shutting down CoreVideo runtime");
    if (g_dock)
        g_dock->prepare_shutdown();
    if (g_iso_panel)
        g_iso_panel->prepare_shutdown();
    if (g_output_panel)
        g_output_panel->prepare_shutdown();
    if (g_diagnostics_panel)
        g_diagnostics_panel->prepare_shutdown();
    ZoomControlServer::instance().stop();
    ZoomOscServer::instance().stop();
    ZoomIsoRecorder::instance().stop();
    ZoomEngineClient::instance().stop();
}

static void frontend_event_callback(enum obs_frontend_event event, void *)
{
    if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
        ensure_zoom_dock();
        ensure_iso_panel();
        ensure_output_panel();
        ensure_diagnostics_panel();
    }
    if (event == OBS_FRONTEND_EVENT_EXIT)
        shutdown_corevideo();
}

static void configure_qt_plugin_paths()
{
#if defined(WIN32)
    wchar_t module_path[MAX_PATH] = {};
    if (!GetModuleFileNameW(reinterpret_cast<HMODULE>(&__ImageBase),
                            module_path, MAX_PATH)) {
        return;
    }

    const QFileInfo plugin_info(QString::fromWCharArray(module_path));
    const QDir plugin_dir = plugin_info.dir();
    const QStringList candidates = {
        plugin_dir.absoluteFilePath("plugins"),
        plugin_dir.absoluteFilePath("qt/plugins"),
        plugin_dir.absoluteFilePath("../plugins"),
    };

    for (const QString &path : candidates) {
        if (QFileInfo::exists(path))
            QCoreApplication::addLibraryPath(path);
    }

    blog(LOG_INFO, "[obs-zoom-plugin] Qt library paths: %s",
         QCoreApplication::libraryPaths().join(";").toUtf8().constData());
#else
    (void)QCoreApplication::libraryPaths();
#endif
}

static ZoomDock *ensure_zoom_dock()
{
    auto *main_win = static_cast<QMainWindow *>(obs_frontend_get_main_window());
    if (!main_win) {
        blog(LOG_WARNING, "[obs-zoom-plugin] obs_frontend_get_main_window() returned null - dock not created");
        return nullptr;
    }

    if (!g_dock) {
        g_dock = new ZoomDock(main_win);
        obs_frontend_add_dock_by_id("ZoomControlDock", "Zoom Control", g_dock);
        blog(LOG_INFO, "[obs-zoom-plugin] Registered dock: ZoomControlDock");
    }
    return g_dock;
}

static void show_zoom_dock()
{
    ZoomDock *dock_widget = ensure_zoom_dock();
    if (!dock_widget) return;

    QWidget *parent = dock_widget;
    while (parent && !qobject_cast<QDockWidget *>(parent))
        parent = parent->parentWidget();

    if (auto *dock = qobject_cast<QDockWidget *>(parent)) {
        dock->show();
        dock->raise();
        dock->activateWindow();
        dock_widget->setFocus();
        return;
    }

    dock_widget->show();
    dock_widget->raise();
    dock_widget->activateWindow();
}

static ZoomIsoPanel *ensure_iso_panel()
{
    auto *main_win = static_cast<QMainWindow *>(obs_frontend_get_main_window());
    if (!main_win) {
        blog(LOG_WARNING, "[obs-zoom-plugin] obs_frontend_get_main_window() returned null - ISO panel not created");
        return nullptr;
    }

    if (!g_iso_panel) {
        g_iso_panel = new ZoomIsoPanel(main_win);
        obs_frontend_add_dock_by_id("ZoomIsoRecorderDock", "Zoom ISO Recorder", g_iso_panel);
        blog(LOG_INFO, "[obs-zoom-plugin] Registered dock: ZoomIsoRecorderDock");
    }
    return g_iso_panel;
}

static void show_iso_panel()
{
    ZoomIsoPanel *panel = ensure_iso_panel();
    if (!panel) return;

    QWidget *parent = panel;
    while (parent && !qobject_cast<QDockWidget *>(parent))
        parent = parent->parentWidget();

    if (auto *dock = qobject_cast<QDockWidget *>(parent)) {
        dock->show();
        dock->raise();
        dock->activateWindow();
        panel->setFocus();
        return;
    }

    panel->show();
    panel->raise();
    panel->activateWindow();
}

template <typename Widget>
static void show_dock_widget(Widget *widget)
{
    if (!widget) return;

    QWidget *parent = widget;
    while (parent && !qobject_cast<QDockWidget *>(parent))
        parent = parent->parentWidget();

    if (auto *dock = qobject_cast<QDockWidget *>(parent)) {
        widget->show();
        dock->show();
        dock->raise();
        dock->activateWindow();
        widget->setFocus();
        return;
    }

    widget->show();
    widget->raise();
    widget->activateWindow();
}

static ZoomOutputDialog *ensure_output_panel()
{
    auto *main_win = static_cast<QMainWindow *>(obs_frontend_get_main_window());
    if (!main_win) {
        blog(LOG_WARNING, "[obs-zoom-plugin] obs_frontend_get_main_window() returned null - Output Manager dock not created");
        return nullptr;
    }

    if (!g_output_panel) {
        g_output_panel = new ZoomOutputDialog(main_win);
        obs_frontend_add_dock_by_id("ZoomOutputManagerDock", "Zoom Output Manager", g_output_panel);
        blog(LOG_INFO, "[obs-zoom-plugin] Registered dock: ZoomOutputManagerDock");
    }
    return g_output_panel;
}

static void show_output_panel()
{
    ZoomOutputDialog *panel = ensure_output_panel();
    if (panel)
        panel->refresh_now();
    show_dock_widget(panel);
}

void corevideo_show_output_manager_dock()
{
    show_output_panel();
}

static ZoomDiagnosticsDialog *ensure_diagnostics_panel()
{
    auto *main_win = static_cast<QMainWindow *>(obs_frontend_get_main_window());
    if (!main_win) {
        blog(LOG_WARNING, "[obs-zoom-plugin] obs_frontend_get_main_window() returned null - Diagnostics dock not created");
        return nullptr;
    }

    if (!g_diagnostics_panel) {
        g_diagnostics_panel = new ZoomDiagnosticsDialog(main_win);
        obs_frontend_add_dock_by_id("ZoomDiagnosticsDock", "Zoom Diagnostics", g_diagnostics_panel);
        blog(LOG_INFO, "[obs-zoom-plugin] Registered dock: ZoomDiagnosticsDock");
    }
    return g_diagnostics_panel;
}

static void show_diagnostics_panel()
{
    ZoomDiagnosticsDialog *panel = ensure_diagnostics_panel();
    if (panel)
        panel->refresh_now();
    show_dock_widget(panel);
}

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-zoom-plugin", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
    return "OBS Zoom Plugin - stream and record Zoom meetings directly from OBS";
}

bool obs_module_load(void)
{
    blog(LOG_INFO, "[obs-zoom-plugin] Loading plugin v%s", OBS_ZOOM_PLUGIN_VERSION);
#ifdef COREVIDEO_HW_ACCEL
    blog(LOG_INFO, "[obs-zoom-plugin] Hardware video acceleration: enabled at build (FFmpeg)");
#else
    blog(LOG_INFO, "[obs-zoom-plugin] Hardware video acceleration: disabled (built without ENABLE_FFMPEG_HW_ACCEL - CPU path only)");
#endif
    configure_qt_plugin_paths();

#if !defined(WIN32)
    // Writing to a closed pipe (engine crashed) raises SIGPIPE on POSIX,
    // which would kill the host OBS process. Ignore it so the write returns
    // EPIPE instead and we can route the failure through normal error paths.
    std::signal(SIGPIPE, SIG_IGN);
#endif

    zoom_source_register();
    zoom_participant_audio_source_register();
    blog(LOG_INFO, "[obs-zoom-plugin] Registered CoreVideo source kinds");

    ZoomPluginSettings s = ZoomPluginSettings::load();
    ZoomReconnectManager::instance().set_policy(s.reconnect_policy);
    ZoomControlServer::instance().set_token(s.control_token);
    if (!ZoomControlServer::instance().start(s.control_server_port))
        blog(LOG_WARNING, "[obs-zoom-plugin] TCP control server unavailable - continuing without it");
    if (!ZoomOscServer::instance().start(s.osc_server_port))
        blog(LOG_WARNING, "[obs-zoom-plugin] OSC server unavailable - continuing without it");

    obs_frontend_add_tools_menu_item("Zoom Plugin Settings", [](void *) {
        auto *main_win = static_cast<QMainWindow *>(obs_frontend_get_main_window());
        ZoomSettingsDialog dlg(main_win);
        dlg.exec();
    }, nullptr);

    obs_frontend_add_tools_menu_item("Zoom Output Manager", [](void *) {
        show_output_panel();
    }, nullptr);

    obs_frontend_add_tools_menu_item("Zoom Diagnostics", [](void *) {
        show_diagnostics_panel();
    }, nullptr);

    obs_frontend_add_tools_menu_item("Zoom Control", [](void *) {
        show_zoom_dock();
    }, nullptr);

    obs_frontend_add_tools_menu_item("Zoom ISO Recorder", [](void *) {
        show_iso_panel();
    }, nullptr);

    obs_frontend_add_event_callback(frontend_event_callback, nullptr);
    g_frontend_callback_registered = true;

    blog(LOG_INFO, "[obs-zoom-plugin] Plugin loaded successfully");
    return true;
}

void obs_module_unload(void)
{
    blog(LOG_INFO, "[obs-zoom-plugin] Unloading plugin");
    if (g_frontend_callback_registered) {
        obs_frontend_remove_event_callback(frontend_event_callback, nullptr);
        g_frontend_callback_registered = false;
    }
    shutdown_corevideo();
    g_dock.clear();
    g_iso_panel.clear();
    g_output_panel.clear();
    g_diagnostics_panel.clear();
}
