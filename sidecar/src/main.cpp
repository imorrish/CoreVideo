#include "mainwindow.h"
#include "obs-live-validator.h"
#include "sidecar-style.h"
#include "show-theme.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QSettings>
#include <QStyleFactory>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("CoreVideo Sidecar");
    app.setOrganizationName("CoreVideo");
    app.setApplicationVersion("0.1.2");

    // Fusion gives clean cross-platform rendering with full QSS support
    app.setStyle(QStyleFactory::create("Fusion"));
    const auto builtins = ShowTheme::builtIns();
    app.setStyleSheet(sidecar_stylesheet(&builtins.first())); // Midnight Blue default

    QCommandLineParser parser;
    parser.setApplicationDescription("CoreVideo Sidecar — broadcast control surface.");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption hostOpt("obs-host",
        "Override OBS WebSocket host.", "host");
    QCommandLineOption portOpt("obs-port",
        "Override OBS WebSocket port.", "port");
    QCommandLineOption passOpt("obs-password",
        "Override OBS WebSocket password.", "password");
    QCommandLineOption autoOpt("obs-autoconnect",
        "Connect to OBS automatically on launch.");
    QCommandLineOption validateOpt("obs-validate",
        "Run a live OBS scene graph validation and exit.");
    QCommandLineOption repairOpt("obs-repair",
        "Provision/repair CoreVideo OBS scenes before validation.");
    QCommandLineOption timeoutOpt("obs-timeout-ms",
        "Validation timeout in milliseconds.", "ms");
    QCommandLineOption reportOpt("obs-report",
        "Write the live OBS validation report to a file.", "path");

    parser.addOption(hostOpt);
    parser.addOption(portOpt);
    parser.addOption(passOpt);
    parser.addOption(autoOpt);
    parser.addOption(validateOpt);
    parser.addOption(repairOpt);
    parser.addOption(timeoutOpt);
    parser.addOption(reportOpt);
    parser.process(app);

    MainWindow::StartupConfig startup;
    if (parser.isSet(hostOpt)) {
        startup.hostOverride = parser.value(hostOpt);
    }
    if (parser.isSet(portOpt)) {
        bool ok = false;
        const int p = parser.value(portOpt).toInt(&ok);
        if (ok && p > 0 && p < 65536) startup.portOverride = p;
    }
    if (parser.isSet(passOpt)) {
        startup.passwordOverride = parser.value(passOpt);
    }
    startup.autoConnect = parser.isSet(autoOpt);

    if (parser.isSet(validateOpt) || parser.isSet(repairOpt)) {
        QSettings s;
        OBSLiveValidator::Config config;
        config.obs.host = parser.isSet(hostOpt)
            ? parser.value(hostOpt)
            : s.value("obs/host", "localhost").toString();
        config.obs.port = s.value("obs/port", 4455).toInt();
        if (parser.isSet(portOpt)) {
            bool ok = false;
            const int p = parser.value(portOpt).toInt(&ok);
            if (ok && p > 0 && p < 65536)
                config.obs.port = p;
        }
        config.obs.password = parser.isSet(passOpt)
            ? parser.value(passOpt)
            : s.value("obs/password", "").toString();
        config.obs.autoReconnect = false;
        config.renderer.sourcePattern = s.value("scene/sourcePattern", "Zoom Participant %1").toString();
        config.renderer.fallbackSceneName = s.value("scene/target", "CoreVideo Main").toString();
        config.renderer.canvasWidth = s.value("scene/canvasW", 1920).toDouble();
        config.renderer.canvasHeight = s.value("scene/canvasH", 1080).toDouble();
        config.repair = parser.isSet(repairOpt);
        if (parser.isSet(timeoutOpt)) {
            bool ok = false;
            const int timeout = parser.value(timeoutOpt).toInt(&ok);
            if (ok && timeout >= 3000)
                config.timeoutMs = timeout;
        }
        if (parser.isSet(reportOpt))
            config.reportPath = parser.value(reportOpt);

        auto *validator = new OBSLiveValidator(config, &app);
        QTimer::singleShot(0, validator, &OBSLiveValidator::start);
        return app.exec();
    }

    MainWindow w(startup);
    w.show();

    return app.exec();
}
