#include "mainwindow.h"
#include "obs-audit-report.h"
#include "preview-canvas.h"
#include "template-panel.h"
#include "look-panel.h"
#include "look-library.h"
#include "theme-panel.h"
#include "participant-panel.h"
#include "scenes-panel.h"
#include "macros-panel.h"
#include "overlay-panel.h"
#include "template-manager.h"
#include "settings-page.h"
#include "obs-connect-dialog.h"
#include "obs-look-renderer.h"
#include "command-palette.h"
#include "sidecar-style.h"
#include "zoom-control-client.h"
#include <QAbstractItemView>
#include <QShortcut>
#include <QKeySequence>
#include <QTimer>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QDialog>
#include <QFrame>
#include <QGridLayout>
#include <QPlainTextEdit>
#include <QDockWidget>
#include <QStackedWidget>
#include <QFile>
#include <QJsonDocument>
#include <QSettings>
#include <QDateTime>
#include <QApplication>
#include <QStyle>
#include <QSizePolicy>
#include <QFileDialog>
#include <QInputDialog>
#include <QStandardPaths>
#include <QDir>
#include <QUuid>
#include <QLineEdit>
#include <QCheckBox>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QTableWidget>
#include <algorithm>

namespace {
constexpr int kSidecarPlaceholderIdBase = -1000;

bool isSidecarPlaceholderId(int participantId)
{
    return participantId <= kSidecarPlaceholderIdBase
        && participantId > kSidecarPlaceholderIdBase - 100;
}

ParticipantInfo sidecarPlaceholderParticipant(int slotIndex)
{
    static const QVector<QColor> colors = {
        QColor(0x1e, 0x6a, 0xe0), QColor(0x20, 0xa0, 0x60),
        QColor(0x9b, 0x40, 0xd0), QColor(0xe0, 0x60, 0x20),
        QColor(0x29, 0x79, 0xff), QColor(0xd0, 0x40, 0x90),
        QColor(0xb0, 0x90, 0x20), QColor(0x40, 0xa0, 0xc0),
    };
    ParticipantInfo p;
    p.id = kSidecarPlaceholderIdBase - slotIndex;
    p.name = QStringLiteral("Placeholder %1").arg(slotIndex + 1);
    p.initials = QStringLiteral("P%1").arg(slotIndex + 1);
    p.color = colors.value(slotIndex % colors.size(), QColor(0x60, 0x70, 0x90));
    p.hasVideo = true;
    p.slotAssign = slotIndex;
    return p;
}

bool isValidSidecarSlot(int slot)
{
    return slot >= 0 && slot < 8;
}
}

MainWindow::MainWindow(const StartupConfig &startup, QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("CoreVideo Sidecar");
    setMinimumSize(1200, 720);
    resize(1440, 860);

    auto *central = new QWidget(this);
    setCentralWidget(central);
    auto *rootH = new QHBoxLayout(central);
    rootH->setContentsMargins(0, 0, 0, 0);
    rootH->setSpacing(0);

    // ── Left sidebar ──────────────────────────────────────────────────────────
    m_sidebar = new Sidebar(this);
    m_sidebar->setFixedWidth(200);
    rootH->addWidget(m_sidebar);

    // ── Center column ─────────────────────────────────────────────────────────
    auto *centerCol = new QWidget(central);
    centerCol->setObjectName("centerCol");
    centerCol->setStyleSheet("#centerCol { background: #0d0d12; }");
    auto *centerV = new QVBoxLayout(centerCol);
    centerV->setContentsMargins(0, 0, 0, 0);
    centerV->setSpacing(0);

    buildTopBar(centerCol);
    centerV->addWidget(m_topBar);

    buildCenterArea(centerCol);
    centerV->addWidget(m_canvasArea, 1);

    // Bottom toolbar
    auto *toolbar = new QWidget(centerCol);
    toolbar->setObjectName("bottomBar");
    toolbar->setFixedHeight(76);
    auto *tbRow = new QHBoxLayout(toolbar);
    tbRow->setContentsMargins(12, 10, 12, 10);
    tbRow->setSpacing(8);

    auto makeBtn = [&](const QString &label, bool primary = false) -> QPushButton * {
        auto *btn = new QPushButton(label, toolbar);
        btn->setObjectName("toolBtn");
        btn->setFixedHeight(44);
        if (primary) btn->setProperty("primary", "true");
        return btn;
    };

    m_swapBtn       = makeBtn("⇄  SWAP");
    m_takeBtn       = makeBtn("⏵  TAKE", true);
    m_autoBtn       = makeBtn("⏬  AUTO");
    m_ftbBtn        = makeBtn("■  FTB");
    auto *streamBtn = makeBtn("⏩  Stream");
    m_vcamBtn       = makeBtn("⏺  V-Cam OFF");

    m_renderPreviewBtn = makeBtn("Render PVW");
    m_mapBtn        = makeBtn("Map");

    m_renderPreviewBtn->setFixedWidth(110);
    m_takeBtn->setFixedWidth(110);
    m_autoBtn->setFixedWidth(100);
    m_ftbBtn->setFixedWidth(80);
    m_swapBtn->setFixedWidth(90);
    m_mapBtn->setFixedWidth(82);

    // AUTO duration picker — common broadcast values 0.5s..5s.
    m_autoDurationCombo = new QComboBox(toolbar);
    m_autoDurationCombo->setFixedHeight(36);
    m_autoDurationCombo->setFixedWidth(76);
    m_autoDurationCombo->addItem("0.5s",  500);
    m_autoDurationCombo->addItem("1.0s", 1000);
    m_autoDurationCombo->addItem("1.5s", 1500);
    m_autoDurationCombo->addItem("2.0s", 2000);
    m_autoDurationCombo->addItem("5.0s", 5000);
    m_autoDurationCombo->setCurrentIndex(2); // 1.5s default

    tbRow->addWidget(m_swapBtn);
    tbRow->addWidget(m_renderPreviewBtn);
    tbRow->addWidget(m_takeBtn);
    tbRow->addWidget(m_autoBtn);
    tbRow->addWidget(m_autoDurationCombo);
    tbRow->addWidget(m_ftbBtn);
    tbRow->addWidget(m_mapBtn);
    tbRow->addStretch(1);
    tbRow->addWidget(m_vcamBtn);
    tbRow->addWidget(streamBtn);

    centerV->addWidget(toolbar);
    rootH->addWidget(centerCol, 1);

    // ── Right panel ───────────────────────────────────────────────────────────
    auto *rightOuter = new QWidget(central);
    rightOuter->setObjectName("rightPanel");
    rightOuter->setFixedWidth(272);
    buildRightPanel(rightOuter);
    rootH->addWidget(rightOuter);

    // ── Log dock ──────────────────────────────────────────────────────────────
    buildLogDock();

    // ── OBS config from settings ──────────────────────────────────────────────
    QSettings cfg;
    m_obsConfig.host          = cfg.value("obs/host", "localhost").toString();
    m_obsConfig.port          = cfg.value("obs/port", 4455).toInt();
    m_obsConfig.password      = cfg.value("obs/password", "").toString();
    m_obsConfig.autoReconnect = cfg.value("obs/autoReconnect", true).toBool();

    // CLI / launch-time overrides — typically supplied by the parent OBS
    // plugin so "Launch Sidecar" delivers a one-click connected session.
    if (startup.hostOverride)     m_obsConfig.host     = *startup.hostOverride;
    if (startup.portOverride)     m_obsConfig.port     = *startup.portOverride;
    if (startup.passwordOverride) m_obsConfig.password = *startup.passwordOverride;

    // ── M/E bus ──────────────────────────────────────────────────────────────
    m_bus = new MEBus(this);
    connect(m_bus, &MEBus::previewChanged, this, [this](const Look &l) {
        m_sceneCanvas->setTemplate(l.tmpl);
        m_sceneCanvas->setParticipants(participantsForLook(l));
        m_sceneCanvas->setOverlays(l.overlays);
        m_sceneCanvas->setBackgroundImage(l.backgroundImagePath);
        m_sceneCanvas->setTileStyle(l.tileStyle);
        if (m_overlayPanel) m_overlayPanel->setActiveOverlays(l.overlays);
    });
    connect(m_bus, &MEBus::programChanged, this, [this](const Look &l) {
        m_liveCanvas->setTemplate(l.tmpl);
        m_liveCanvas->setParticipants(participantsForLook(l));
        m_liveCanvas->setOverlays(l.overlays);
        m_liveCanvas->setBackgroundImage(l.backgroundImagePath);
        m_liveCanvas->setTileStyle(l.tileStyle);
    });
    // TAKE is the only path that pushes to OBS.
    connect(m_bus, &MEBus::tookProgram, this, [this](const Look &l) {
        m_currentTemplate = l.tmpl;
        m_controlServer->notifyTemplateChanged(l.tmpl.id, l.tmpl.name);
        onApplyLayout();
    });

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_sidebar, &Sidebar::pageSelected, this, &MainWindow::onPageSelected);
    connect(m_takeBtn, &QPushButton::clicked,  this, &MainWindow::onTake);
    connect(m_autoBtn, &QPushButton::clicked,  this, &MainWindow::onAuto);
    connect(m_ftbBtn,  &QPushButton::clicked,  this, &MainWindow::onFTB);
    connect(m_swapBtn, &QPushButton::clicked,  this, &MainWindow::onSwapBuses);
    connect(m_engineBtn, &QPushButton::clicked, this, &MainWindow::onEngineToggle);
    connect(m_syncInspectBtn, &QPushButton::clicked, this, &MainWindow::openObsSyncInspector);
    connect(m_renderPreviewBtn, &QPushButton::clicked, this, &MainWindow::onRenderPreview);
    connect(m_mapBtn, &QPushButton::clicked, this, &MainWindow::openParticipantMappingWindow);
    connect(m_obsBtn,  &QPushButton::clicked,  this, &MainWindow::onObsConnect);
    connect(m_vcamBtn, &QPushButton::clicked,  this, &MainWindow::onVirtualCamToggle);

    // Spacebar = TAKE, Enter = AUTO, F12 = FTB
    auto *takeShortcut = new QShortcut(QKeySequence(Qt::Key_Space), this);
    takeShortcut->setContext(Qt::ApplicationShortcut);
    connect(takeShortcut, &QShortcut::activated, this, &MainWindow::onTake);

    auto *autoShortcut = new QShortcut(QKeySequence(Qt::Key_Return), this);
    autoShortcut->setContext(Qt::ApplicationShortcut);
    connect(autoShortcut, &QShortcut::activated, this, &MainWindow::onAuto);

    auto *autoShortcut2 = new QShortcut(QKeySequence(Qt::Key_Enter), this);
    autoShortcut2->setContext(Qt::ApplicationShortcut);
    connect(autoShortcut2, &QShortcut::activated, this, &MainWindow::onAuto);

    auto *ftbShortcut = new QShortcut(QKeySequence(Qt::Key_F12), this);
    ftbShortcut->setContext(Qt::ApplicationShortcut);
    connect(ftbShortcut, &QShortcut::activated, this, &MainWindow::onFTB);

    // Bus drives PGM canvas opacity during AUTO / FTB.
    connect(m_bus, &MEBus::pgmOpacityChanged, this, [this](float o) {
        if (m_liveCanvas) m_liveCanvas->setOpacity(o);
    });
    // Disable transition buttons while a transition is in flight to keep
    // the gesture atomic — no overlapping AUTOs.
    connect(m_bus, &MEBus::transitionStarted, this,
            [this](MEBus::TransitionKind k, int) {
        m_takeBtn->setEnabled(false);
        m_autoBtn->setEnabled(false);
        m_swapBtn->setEnabled(false);
        // FTB stays enabled only when itself is the active transition, so
        // the operator can interrupt — but for slice 4 just lock it out.
        m_ftbBtn->setEnabled(false);
        if (k == MEBus::FTBOn || k == MEBus::FTBOff) {
            m_ftbBtn->setText((k == MEBus::FTBOn) ? "■  …→BLK" : "■  BLK→…");
        }
    });
    connect(m_bus, &MEBus::transitionEnded, this, [this]() {
        m_takeBtn->setEnabled(true);
        m_autoBtn->setEnabled(true);
        m_swapBtn->setEnabled(true);
        m_ftbBtn->setEnabled(true);
        m_ftbBtn->setText(m_bus->isFTBActive() ? "■  FTB ON" : "■  FTB");
    });

    m_obsClient = new OBSClient(this);
    connect(m_obsClient, &OBSClient::stateChanged,       this, &MainWindow::onObsState);
    connect(m_obsClient, &OBSClient::log,                this, &MainWindow::onObsLog);
    connect(m_obsClient, &OBSClient::scenesReceived,     this, &MainWindow::onScenesReceived);
    connect(m_obsClient, &OBSClient::sceneItemsReceived, this,
            [this](const QString &, const QVector<OBSClient::SceneItem> &) {
        updateSceneSyncStatus();
    });
    connect(m_obsClient, &OBSClient::sceneChanged,       this, [this](const QString &name) {
        m_scenesPanel->setCurrentScene(name);
        stageLookFromObsScene(name);
    });
    connect(m_obsClient, &OBSClient::virtualCamStateChanged, this, &MainWindow::onVirtualCamState);
    connect(m_obsClient, &OBSClient::templateApplied,    this,
            [this](const QString &name, int n) {
                onObsLog(QStringLiteral("✓ Applied '%1' (%2 items).").arg(name).arg(n));
            });
    connect(m_obsClient, &OBSClient::requestFailed, this, [this](const QString &summary) {
        if (!m_sceneSyncStatusLabel)
            return;
        m_obsSyncState = ObsSyncState::Error;
        m_lastSyncError = summary;
        m_sceneSyncStatusLabel->setText(QStringLiteral("Sync error"));
        m_sceneSyncStatusLabel->setToolTip(summary);
        m_sceneSyncStatusLabel->setStyleSheet("color: #e04040; font-size: 11px; background: transparent;");
    });
    connect(m_obsClient, &OBSClient::inventoryReady, this, [this]() {
        provisionLookScenes();
        if (m_bus && m_bus->program().isValid()) {
            QTimer::singleShot(1200, this, [this]() {
                if (m_obsClient && m_obsClient->isConnected()
                    && m_bus && m_bus->program().isValid()) {
                    renderLookToOBS(m_bus->program(), true);
                }
            });
        }
        updateSceneSyncStatus();
    });

    m_controlServer = new SidecarControlServer(this);
    m_controlServer->start();

    connect(m_obsClient, &OBSClient::stateChanged, this, [this](OBSClient::State) {
        m_controlServer->notifyOBSState(m_obsClient->stateLabel());
    });
    connect(m_obsClient, &OBSClient::scenesReceived, this, [this](const QStringList &scenes) {
        m_controlServer->notifyScenesUpdated(scenes);
    });
    connect(m_obsClient, &OBSClient::sceneChanged, this, [this](const QString &scene) {
        m_controlServer->notifySceneChanged(scene);
    });

    connect(m_controlServer, &SidecarControlServer::phaseChangeRequested,
            this, [this](const QString &phase) {
        if (phase == "live")           onPhaseSelected(ShowPhase::Live);
        else if (phase == "post_show") onPhaseSelected(ShowPhase::PostShow);
        else                           onPhaseSelected(ShowPhase::PreShow);
    });
    // Companion / remote "apply template" = stage on PVW + immediate TAKE.
    connect(m_controlServer, &SidecarControlServer::templateApplyRequested,
            this, [this](const QString &id) {
        auto &tm = TemplateManager::instance();
        if (const auto *t = tm.findById(id)) {
            onTemplateSelected(*t);
            onTake();
        }
    });
    connect(m_controlServer, &SidecarControlServer::sceneChangeRequested,
            this, [this](const QString &scene) {
        onSceneActivated(scene);
    });

    m_zoomClient = new ZoomControlClient(this);
    connect(m_zoomClient, &ZoomControlClient::log, this, &MainWindow::onObsLog);
    connect(m_zoomClient, &ZoomControlClient::participantsUpdated,
            this, [this](const QVector<ParticipantInfo> &participants) {
        reconcileParticipantSlots(participants);
        updateParticipantSyncedLowerThirds();
        m_participantPanel->setParticipants(m_participants);
        if (m_sceneCanvas) m_sceneCanvas->setParticipants(participantsForLook(m_working));
        if (m_liveCanvas && m_bus) m_liveCanvas->setParticipants(participantsForLook(m_bus->program()));
        syncZoomOutputAssignments();
    });
    connect(m_zoomClient, &ZoomControlClient::outputSourcesUpdated,
            this, [this](const QStringList &sourceNames) {
        m_outputSources = sourceNames;
    });
    m_zoomClient->start();

    // Canvas slot assignment from drag-and-drop
    connect(m_liveCanvas,  &PreviewCanvas::slotAssigned, this, &MainWindow::onSlotAssigned);
    connect(m_sceneCanvas, &PreviewCanvas::slotAssigned, this, &MainWindow::onSlotAssigned);

    // Click-to-assign — selecting a slot focuses the Templates page and arms
    // the participant panel so the next card click fills that slot.
    connect(m_liveCanvas,  &PreviewCanvas::slotClicked, this, &MainWindow::onSlotClicked);
    connect(m_sceneCanvas, &PreviewCanvas::slotClicked, this, &MainWindow::onSlotClicked);

    // Right-click on a slot cycles its audio routing (Mix → Iso → Aud → Mix).
    connect(m_liveCanvas,  &PreviewCanvas::slotRoutingCycleRequested,
            this, &MainWindow::onSlotRoutingCycle);
    connect(m_sceneCanvas, &PreviewCanvas::slotRoutingCycleRequested,
            this, &MainWindow::onSlotRoutingCycle);

    // Participant card click — consumed only while assign mode is armed
    connect(m_participantPanel, &ParticipantPanel::assignRequested, this,
            [this](int pid, int slot) { onSlotAssigned(slot, pid); });
    connect(m_participantPanel, &ParticipantPanel::participantClicked,
            this, &MainWindow::onParticipantAssignClicked);

    // Command palette (Ctrl+K / Cmd+K)
    m_commandPalette = new CommandPalette(this);
    auto *paletteShortcut = new QShortcut(
        QKeySequence(QKeySequence::Find), this);
    paletteShortcut->setContext(Qt::ApplicationShortcut);
    connect(paletteShortcut, &QShortcut::activated,
            this, &MainWindow::openCommandPalette);
    auto *paletteShortcutK = new QShortcut(
        QKeySequence(Qt::CTRL | Qt::Key_K), this);
    paletteShortcutK->setContext(Qt::ApplicationShortcut);
    connect(paletteShortcutK, &QShortcut::activated,
            this, &MainWindow::openCommandPalette);

    // Templates (used by command palette + LookLibrary resolution)
    auto &tm = TemplateManager::instance();
    tm.loadBuiltIn();
    m_templatePanel->loadTemplates(tm.templates());

    // Broadcast-ready Looks library — load after templates so each Look's
    // templateId can be resolved into an in-memory LayoutTemplate.
    auto &ll = LookLibrary::instance();
    ll.loadBuiltIn();
    loadCustomLooks();
    refreshLookPanel();

    // Themes
    m_themePanel->loadThemes(ShowTheme::builtIns());

    // Stage a sensible default on PVW. Prefer the first preset Look; if
    // the library is empty, fall back to the bare 4-up template.
    if (!ll.looks().isEmpty()) {
        onLookSelected(ll.looks().first());
    } else if (const auto *grid4 = tm.findById("4-up-grid")) {
        onTemplateSelected(*grid4);
    }

    // Mock data — also stages slot assignments onto PVW.
    loadMockParticipants();

    // Commit the staged default to PGM so first paint matches PVW.
    m_bus->take();

    // Initial UI state
    onObsState(OBSClient::State::Disconnected);
    onObsLog("Sidecar ready. Click 'OBS' to connect, or press Ctrl+K for the command palette.");

    // Auto-connect on launch if the parent process asked for it (e.g. when
    // launched from the OBS plugin's "Launch Sidecar" button).
    if (startup.autoConnect) {
        QSettings s;
        s.setValue("obs/host",     m_obsConfig.host);
        s.setValue("obs/port",     m_obsConfig.port);
        s.setValue("obs/password", m_obsConfig.password);
        // Defer one tick so the window paints first
        QTimer::singleShot(0, this, [this]() {
            onObsLog(QStringLiteral("Auto-connecting to %1:%2 …")
                         .arg(m_obsConfig.host).arg(m_obsConfig.port));
            m_obsClient->connectToOBS(m_obsConfig);
        });
    }
}

// ── Top bar ───────────────────────────────────────────────────────────────────
void MainWindow::buildTopBar(QWidget *parent)
{
    m_topBar = new QWidget(parent);
    m_topBar->setObjectName("topBar");
    m_topBar->setFixedHeight(52);

    auto *row = new QHBoxLayout(m_topBar);
    row->setContentsMargins(16, 0, 12, 0);
    row->setSpacing(10);

    m_showNameLabel = new QLabel("Morning Show — Episode 142", m_topBar);
    m_showNameLabel->setStyleSheet("color: #e0e0f0; font-size: 15px; font-weight: 700;");

    // Show phase segmented control
    auto *phaseBar = new QWidget(m_topBar);
    phaseBar->setObjectName("phaseSegment");
    auto *phl = new QHBoxLayout(phaseBar);
    phl->setContentsMargins(2, 2, 2, 2);
    phl->setSpacing(0);

    m_preShowBtn  = new QPushButton("PRE",     phaseBar);
    m_liveBtn     = new QPushButton("● LIVE",  phaseBar);
    m_postShowBtn = new QPushButton("POST",    phaseBar);

    m_preShowBtn->setObjectName("phasePreBtn");
    m_liveBtn->setObjectName("phaseLiveBtn");
    m_postShowBtn->setObjectName("phasePostBtn");

    for (auto *b : {m_preShowBtn, m_liveBtn, m_postShowBtn}) {
        b->setCheckable(true);
        b->setFixedHeight(28);
        phl->addWidget(b);
    }
    m_preShowBtn->setChecked(true);

    connect(m_preShowBtn,  &QPushButton::clicked, this,
            [this]{ onPhaseSelected(ShowPhase::PreShow);  });
    connect(m_liveBtn,     &QPushButton::clicked, this,
            [this]{ onPhaseSelected(ShowPhase::Live);     });
    connect(m_postShowBtn, &QPushButton::clicked, this,
            [this]{ onPhaseSelected(ShowPhase::PostShow); });

    m_obsStatusLabel = new QLabel("Disconnected", m_topBar);
    m_obsStatusLabel->setStyleSheet("color: #8080a0; font-size: 11px; background: transparent;");

    m_sceneSyncStatusLabel = new QLabel("Sync idle", m_topBar);
    m_sceneSyncStatusLabel->setStyleSheet("color: #8080a0; font-size: 11px; background: transparent;");
    m_sceneSyncStatusLabel->setMinimumWidth(270);
    m_sceneSyncStatusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_engineBtn = new QPushButton("Sync OBS", m_topBar);
    m_engineBtn->setObjectName("engineOffBtn");
    m_engineBtn->setFixedHeight(34);

    m_syncInspectBtn = new QPushButton("Inspect", m_topBar);
    m_syncInspectBtn->setObjectName("toolBtn");
    m_syncInspectBtn->setFixedHeight(34);

    m_obsBtn = new QPushButton("OBS  ○", m_topBar);
    m_obsBtn->setObjectName("obsBtn");
    m_obsBtn->setFixedHeight(34);

    row->addWidget(m_showNameLabel);
    row->addStretch(1);
    row->addWidget(phaseBar);
    row->addWidget(m_obsStatusLabel);
    row->addWidget(m_sceneSyncStatusLabel);
    row->addSpacing(4);
    row->addWidget(m_obsBtn);
    row->addWidget(m_engineBtn);
    row->addWidget(m_syncInspectBtn);
}

// ── Center canvas area ────────────────────────────────────────────────────────
void MainWindow::buildCenterArea(QWidget *parent)
{
    m_canvasArea = new QWidget(parent);
    m_canvasArea->setMaximumHeight(560);
    m_canvasArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *row = new QHBoxLayout(m_canvasArea);
    row->setContentsMargins(12, 10, 12, 4);
    row->setSpacing(12);

    auto buildPane = [&](const QString &label, const QString &color,
                         PreviewCanvas *&out, QLabel *&outLbl) {
        auto *wrap = new QWidget(m_canvasArea);
        auto *v = new QVBoxLayout(wrap);
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(4);
        outLbl = new QLabel(label, wrap);
        outLbl->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: 800; "
                                      "letter-spacing: 0.1em; background: transparent;")
                                  .arg(color));
        out = new PreviewCanvas(wrap);
        out->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
        v->addWidget(outLbl);
        v->addWidget(out, 1);
        return wrap;
    };

    // PVW on the left (where you build), PGM on the right (what's on air) —
    // matches the visual flow "stage → take → on air."
    row->addWidget(buildPane("PVW  ◉  PREVIEW", "#20c460", m_sceneCanvas, m_pvwLabel), 1);
    row->addWidget(buildPane("PGM  ●  ON AIR",  "#ff4040", m_liveCanvas,  m_pgmLabel), 1);
}

static QString customLooksPath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return QDir(dir).filePath(QStringLiteral("custom-looks.json"));
}

void MainWindow::loadCustomLooks()
{
    m_customLooks.clear();
    QFile f(customLooksPath());
    if (!f.open(QIODevice::ReadOnly))
        return;

    QJsonParseError err{};
    const auto doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray())
        return;

    auto &tm = TemplateManager::instance();
    for (const auto &v : doc.array()) {
        Look look = Look::fromJson(v.toObject());
        if (!look.tmpl.isValid()) {
            if (const auto *t = tm.findById(look.templateId))
                look.tmpl = *t;
        }
        if (look.isValid())
            m_customLooks.append(look);
    }
}

void MainWindow::saveCustomLooks() const
{
    QJsonArray arr;
    for (const auto &look : m_customLooks)
        arr.append(look.toJson());

    QFile f(customLooksPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

QVector<Look> MainWindow::allLooks() const
{
    QVector<Look> looks = LookLibrary::instance().looks();
    looks += m_customLooks;
    return looks;
}

void MainWindow::refreshLookPanel()
{
    if (m_lookPanel)
        m_lookPanel->loadLooks(allLooks());
}

OBSLookRenderer::Config MainWindow::obsRendererConfig() const
{
    OBSLookRenderer::Config config;
    if (m_settingsPage) {
        config.sourcePattern = m_settingsPage->sourcePattern();
        config.fallbackSceneName = m_settingsPage->targetScene();
        config.canvasWidth = m_settingsPage->canvasWidth();
        config.canvasHeight = m_settingsPage->canvasHeight();
    }
    config.normalizeBroadcastCanvas();
    return config;
}

OBSLookRenderer MainWindow::obsRenderer() const
{
    return OBSLookRenderer(m_obsClient, obsRendererConfig());
}

QVector<PreviewCanvas::Participant>
MainWindow::participantsForLook(const Look &look) const
{
    const int nSlots = look.tmpl.slotList.size();
    QVector<PreviewCanvas::Participant> cp(nSlots);
    const QVector<int> participantSlots = participantSlotIndexesForLook(look);
    for (const auto &s : look.slotAssignments) {
        if (!participantSlots.contains(s.slotIndex)) continue;
        if (s.slotIndex < 0 || s.slotIndex >= nSlots) continue;
        for (const auto &p : m_participants) {
            if (p.id == s.participantId) {
                if (look.tileStyle.excludeNoVideo && !p.hasVideo)
                    break;
                cp[s.slotIndex] = {p.name, p.initials, p.color, p.isTalking, p.hasVideo};
                break;
            }
        }
    }
    return cp;
}

QStringList MainWindow::slotLabelsForLook(const Look &look) const
{
    QStringList labels;
    labels.reserve(8);
    for (int i = 0; i < 8; ++i)
        labels << QStringLiteral("Slot %1").arg(i + 1);

    const QVector<int> participantSlots = participantSlotIndexesForLook(look);
    for (const auto &s : look.slotAssignments) {
        if (!participantSlots.contains(s.slotIndex) || s.slotIndex < 0)
            continue;
        for (const auto &p : m_participants) {
            if (p.id != s.participantId)
                continue;
            if (look.tileStyle.excludeNoVideo && !p.hasVideo)
                break;
            while (labels.size() <= s.slotIndex)
                labels << QStringLiteral("Slot %1").arg(labels.size() + 1);
            labels[s.slotIndex] = p.name;
            break;
        }
    }
    return labels;
}

QVector<int> MainWindow::participantSlotIndexesForLook(const Look &look) const
{
    QVector<int> participantSlots;
    for (const TemplateSlot &slot : look.tmpl.slotList) {
        if ((look.templateId == QStringLiteral("speaker-screenshare")
             || look.tmpl.id == QStringLiteral("speaker-screenshare"))
            && slot.index == 1) {
            continue;
        }
        participantSlots.append(slot.index);
    }
    return participantSlots;
}

Look MainWindow::lookWithCurrentAssignments(const Look &look) const
{
    Look staged = look;
    const QVector<int> participantSlots = participantSlotIndexesForLook(staged);
    staged.slotAssignments.clear();

    for (const auto &participant : m_participants) {
        if (participant.slotAssign < 0 || !participantSlots.contains(participant.slotAssign))
            continue;
        staged.slotAssignments.append({participant.slotAssign, participant.id});
    }

    return staged;
}

QStringList MainWindow::sourceNamesForSlots(int slotCount) const
{
    return obsRenderer().sourceNamesForSlots(slotCount);
}

QStringList MainWindow::sourceNamesForLook(const Look &look) const
{
    return obsRenderer().sourceNamesForLook(look);
}

QStringList MainWindow::lookSceneNames() const
{
    return obsRenderer().sceneNamesForLooks(allLooks());
}

QString MainWindow::obsSceneNameForLook(const Look &look) const
{
    return obsRenderer().sceneNameForLook(look);
}

QVector<LookRenderPlan> MainWindow::lookRenderPlans() const
{
    QVector<LookRenderPlan> plans;
    const OBSLookRenderer renderer = obsRenderer();
    for (const Look &look : allLooks()) {
        const LookRenderPlan plan = renderer.renderPlanForLook(look, false, slotLabelsForLook(look));
        if (plan.valid)
            plans.append(plan);
    }
    return plans;
}

const Look *MainWindow::lookForObsSceneName(const QString &sceneName) const
{
    const QString normalized = sceneName.trimmed();
    for (const auto &look : LookLibrary::instance().looks()) {
        if (!look.tmpl.isValid())
            continue;
        if (obsSceneNameForLook(look) == normalized)
            return &look;
    }
    for (const auto &look : m_customLooks) {
        if (!look.tmpl.isValid())
            continue;
        if (obsSceneNameForLook(look) == normalized)
            return &look;
    }
    return nullptr;
}

void MainWindow::stageLookFromObsScene(const QString &sceneName)
{
    const Look *look = lookForObsSceneName(sceneName);
    if (!look)
        return;

    m_working = lookWithCurrentAssignments(*look);
    m_working.templateId = look->tmpl.id.isEmpty() ? look->templateId : look->tmpl.id;
    if (m_bus)
        m_bus->stageLook(m_working);
    onObsLog(QStringLiteral("OBS scene linked to Look: %1.").arg(m_working.name));
}

// ── Right panel ───────────────────────────────────────────────────────────────
void MainWindow::buildRightPanel(QWidget *parent)
{
    auto *vl = new QVBoxLayout(parent);
    vl->setContentsMargins(0, 0, 0, 0);
    vl->setSpacing(0);

    m_rightStack = new QStackedWidget(parent);

    // Page 0: Looks gallery + Participants (combined scroll)
    auto *tmplPage = new QWidget;
    auto *tmplV = new QVBoxLayout(tmplPage);
    tmplV->setContentsMargins(0, 0, 0, 0);
    tmplV->setSpacing(0);
    m_lookPanel = new LookPanel(tmplPage);
    connect(m_lookPanel, &LookPanel::lookSelected,
            this, &MainWindow::onLookSelected);
    connect(m_lookPanel, &LookPanel::createLookRequested,
            this, &MainWindow::onCreateLookRequested);
    connect(m_lookPanel, &LookPanel::saveLookRequested,
            this, &MainWindow::onSaveLookRequested);
    connect(m_lookPanel, &LookPanel::setBackgroundRequested,
            this, &MainWindow::onSetBackgroundRequested);
    connect(m_lookPanel, &LookPanel::designLookRequested,
            this, &MainWindow::onDesignLookRequested);
    tmplV->addWidget(m_lookPanel, 3);
    auto *div = new QFrame(tmplPage);
    div->setFrameShape(QFrame::HLine);
    div->setStyleSheet("color: #1e1e2e;");
    tmplV->addWidget(div);
    m_participantPanel = new ParticipantPanel(tmplPage);
    m_participantPanel->setMinimumHeight(300);
    tmplV->addWidget(m_participantPanel, 2);
    m_pageTemplates = m_rightStack->addWidget(tmplPage);

    // TemplatePanel still constructed (off-screen) so the command-palette
    // path that references TemplateManager can stage raw layouts. Kept for
    // back-compat; not visible in the right panel.
    m_templatePanel = new TemplatePanel;
    m_templatePanel->hide();
    connect(m_templatePanel, &TemplatePanel::templateSelected,
            this, &MainWindow::onTemplateSelected);

    // Page 1: Themes
    m_themePanel = new ThemePanel;
    connect(m_themePanel, &ThemePanel::themeSelected,
            this, &MainWindow::onThemeSelected);
    m_pageThemes = m_rightStack->addWidget(m_themePanel);

    // Page 2: Scenes
    m_scenesPanel = new ScenesPanel;
    connect(m_scenesPanel, &ScenesPanel::sceneActivated,
            this, &MainWindow::onSceneActivated);
    connect(m_scenesPanel, &ScenesPanel::refreshRequested, this, [this]() {
        m_obsClient->refreshInventory();
    });
    m_pageScenes = m_rightStack->addWidget(m_scenesPanel);

    // Page 3: Macros
    m_macrosPanel = new MacrosPanel;
    connect(m_macrosPanel, &MacrosPanel::macroTriggered,
            this, &MainWindow::onMacroTriggered);
    m_pageMacros = m_rightStack->addWidget(m_macrosPanel);

    // Page: Overlays
    m_overlayPanel = new OverlayPanel;
    connect(m_overlayPanel, &OverlayPanel::overlayRequested,
            this, [this](const Overlay &ov) {
        m_working.overlays.append(ov);
        m_bus->stageLook(m_working);
        m_overlayPanel->setActiveOverlays(m_working.overlays);
        onObsLog(QStringLiteral("Overlay staged on PVW: %1").arg(Overlay::humanLabel(ov)));
    });
    connect(m_overlayPanel, &OverlayPanel::removeOverlayRequested,
            this, [this](int idx) {
        if (idx < 0 || idx >= m_working.overlays.size()) return;
        m_working.overlays.remove(idx);
        m_bus->stageLook(m_working);
        m_overlayPanel->setActiveOverlays(m_working.overlays);
    });
    connect(m_overlayPanel, &OverlayPanel::clearOverlaysRequested,
            this, [this]() {
        m_working.overlays.clear();
        m_bus->stageLook(m_working);
        m_overlayPanel->setActiveOverlays(m_working.overlays);
        onObsLog("Cleared PVW overlays.");
    });
    m_pageOverlays = m_rightStack->addWidget(m_overlayPanel);

    // Page 4: Settings
    m_settingsPage = new SettingsPage;
    connect(m_settingsPage, &SettingsPage::settingsChanged,
            this, &MainWindow::onSettingsChanged);
    m_pageSettings = m_rightStack->addWidget(m_settingsPage);

    vl->addWidget(m_rightStack, 1);
}

// ── Log dock ──────────────────────────────────────────────────────────────────
void MainWindow::buildLogDock()
{
    m_logDock = new QDockWidget("OBS Log", this);
    m_logDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    m_logView = new QPlainTextEdit(m_logDock);
    m_logView->setReadOnly(true);
    m_logView->setMaximumBlockCount(500);
    m_logView->setStyleSheet(
        "QPlainTextEdit { background-color: #0a0a10; color: #b0b0d0; "
        "font-family: 'Menlo', 'Consolas', monospace; font-size: 11px; "
        "border: none; padding: 6px; }");
    m_logDock->setMaximumHeight(160);
    m_logDock->setWidget(m_logView);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);
    m_logDock->hide();
}

// ── Mock data ─────────────────────────────────────────────────────────────────
void MainWindow::loadMockParticipants()
{
    m_participants.clear();
    for (int i = 0; i < 8; ++i)
        m_participants.append(sidecarPlaceholderParticipant(i));
    m_participantPanel->setParticipants(m_participants);

    // Seed the working Look's slot assignments from each participant's
    // default slotAssign so first-paint shows people in slots.
    m_working.slotAssignments.clear();
    const QVector<int> participantSlots = participantSlotIndexesForLook(m_working);
    for (const auto &p : m_participants) {
        if (p.slotAssign >= 0 && participantSlots.contains(p.slotAssign))
            m_working.slotAssignments.append({p.slotAssign, p.id});
    }
    if (m_bus) m_bus->stageLook(m_working);
}

// ── Slots ─────────────────────────────────────────────────────────────────────
void MainWindow::provisionPlaceholderSources()
{
    if (!m_obsClient || !m_obsClient->isConnected() || !m_settingsPage)
        return;

    obsRenderer().provisionPlaceholders(8);
    QTimer::singleShot(3400, this, &MainWindow::updateSceneSyncStatus);
}

void MainWindow::provisionLookScenes()
{
    if (!m_obsClient || !m_obsClient->isConnected() || !m_settingsPage)
        return;

    obsRenderer().provisionLooks(allLooks());
    onObsLog("Provisioning shared sources and Look scenes in OBS.");
    QTimer::singleShot(3600, this, &MainWindow::updateSceneSyncStatus);
}

void MainWindow::repairCoreVideoDuplicates()
{
    if (!m_obsClient || !m_obsClient->isConnected()) {
        onObsLog("Repair skipped: OBS is not connected.");
        return;
    }
    m_obsClient->removeStaleCoreVideoDuplicates(sourceNamesForSlots(8), lookSceneNames());
    m_obsClient->hideStaleCoreVideoDesignLayers(lookRenderPlans());
}

void MainWindow::refreshObsAuditInventory()
{
    if (!m_obsClient || !m_obsClient->isConnected())
        return;

    m_obsClient->refreshInventory();
    m_obsClient->requestSceneItems(QStringLiteral("CoreVideo Sources"));
    m_obsClient->requestSceneItems(QStringLiteral("CoreVideo Screen Share"));
    for (int i = 0; i < 8; ++i)
        m_obsClient->requestSceneItems(QStringLiteral("CoreVideo Slot %1").arg(i + 1));
    for (const QString &scene : lookSceneNames())
        m_obsClient->requestSceneItems(scene);
}

void MainWindow::reconcileObsSceneGraph()
{
    if (!m_obsClient || !m_obsClient->isConnected()) {
        onObsLog("Sync OBS skipped: OBS is not connected.");
        return;
    }

    m_obsSyncState = ObsSyncState::Applying;
    m_lastSyncError.clear();
    m_lastRenderedLookName = "All Looks";
    m_lastRenderedSceneName = "CoreVideo scene graph";
    if (m_sceneSyncStatusLabel) {
        m_sceneSyncStatusLabel->setText("Syncing OBS");
        m_sceneSyncStatusLabel->setToolTip("Reconciling CoreVideo sources, slot scenes, Look scenes, layer ordering, and stale design layers.");
        m_sceneSyncStatusLabel->setStyleSheet("color: #e0a020; font-size: 11px; background: transparent;");
    }

    const auto before = m_obsClient->coreVideoSceneAudit(sourceNamesForSlots(8), lookRenderPlans());
    if (before.inventoryReady) {
        onObsLog(QStringLiteral("Sync OBS audit before: %1/%2 scenes, %3/%4 inputs, %5/%6 items.")
                     .arg(before.presentScenes).arg(before.expectedScenes)
                     .arg(before.presentInputs).arg(before.expectedInputs)
                     .arg(before.presentSceneItems).arg(before.expectedSceneItems));
    } else {
        onObsLog("Sync OBS: inventory not ready; refreshing OBS before reconcile.");
    }

    refreshObsAuditInventory();
    provisionPlaceholderSources();
    provisionLookScenes();

    QTimer::singleShot(4600, this, [this]() {
        repairCoreVideoDuplicates();
        refreshObsAuditInventory();
    });
    QTimer::singleShot(6200, this, [this]() {
        validateObsSceneGraphStatus(QStringLiteral("Sync OBS"), true);
    });
}

void MainWindow::repairLookGeometry(const Look &look)
{
    if (!m_obsClient || !m_obsClient->isConnected()) {
        onObsLog("Repair geometry skipped: OBS is not connected.");
        return;
    }

    const OBSLookRenderer renderer = obsRenderer();
    const LookRenderPlan plan = renderer.renderPlanForLook(look, false, slotLabelsForLook(look));
    if (!plan.valid) {
        onObsLog(QStringLiteral("Repair geometry skipped: Look '%1' has no valid render plan.").arg(look.name));
        return;
    }

    const LookGeometryRepairPlan repairPlan = geometryRepairPlanForLook(plan);
    if (!repairPlan.valid) {
        onObsLog(QStringLiteral("Repair geometry skipped: Look '%1' has no valid geometry repair plan.").arg(look.name));
        return;
    }

    m_obsClient->requestSceneItems(plan.sceneName);
    onObsLog(QStringLiteral("Repairing OBS geometry for '%1'.").arg(plan.sceneName));

    QTimer::singleShot(650, this, [this, plan, repairPlan]() {
        if (!m_obsClient || !m_obsClient->isConnected())
            return;
        m_obsClient->applyLayout(plan.sceneName,
                                 plan.tmpl,
                                 repairPlan.nestedSceneNames,
                                 plan.canvasWidth,
                                 plan.canvasHeight);
        m_obsClient->applyLookLayerOrder(plan.sceneName,
                                         plan.tmpl,
                                         plan.sourceNames,
                                         plan.tileStyle,
                                         plan.hasBackgroundImage(),
                                         plan.overlays.size());
    });
    QTimer::singleShot(1800, this, [this, scene = plan.sceneName]() {
        if (m_obsClient && m_obsClient->isConnected())
            m_obsClient->requestSceneItems(scene);
    });
}

static void fillInspectorTable(QTableWidget *table,
                               const QString &category,
                               const QStringList &items)
{
    for (const QString &item : items) {
        const int row = table->rowCount();
        table->insertRow(row);
        auto *categoryItem = new QTableWidgetItem(category);
        auto *detailItem = new QTableWidgetItem(item);
        categoryItem->setFlags(categoryItem->flags() & ~Qt::ItemIsEditable);
        detailItem->setFlags(detailItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(row, 0, categoryItem);
        table->setItem(row, 1, detailItem);
    }
}

static QString obsAuditPrimaryOutcome(const OBSClient::CoreVideoSceneAudit &audit,
                                      const QString &lastLook)
{
    if (audit.isClean()) {
        if (!lastLook.isEmpty())
            return QStringLiteral("Applied: %1").arg(lastLook.left(28));
        return QStringLiteral("Applied");
    }
    if (!audit.missingInputs.isEmpty())
        return QStringLiteral("Missing sources");
    if (!audit.missingScenes.isEmpty())
        return QStringLiteral("Missing scenes");
    if (!audit.missingSceneItems.isEmpty())
        return QStringLiteral("Scene mismatch");
    if (!audit.geometryDrift.isEmpty())
        return QStringLiteral("Geometry drift");
    if (!audit.staleDesignLayers.isEmpty())
        return QStringLiteral("Stale layers");
    if (!audit.inventoryReady)
        return QStringLiteral("OBS loading");
    return QStringLiteral("OBS drift");
}

static QString obsAuditRepairHint(const OBSClient::CoreVideoSceneAudit &audit)
{
    if (audit.isClean())
        return QStringLiteral("OBS scene graph matches the Sidecar Look catalog.");
    if (!audit.missingInputs.isEmpty())
        return QStringLiteral("Click Repair OBS to create missing CoreVideo sources.");
    if (!audit.missingScenes.isEmpty())
        return QStringLiteral("Click Repair OBS to create missing Look and nested scenes.");
    if (!audit.missingSceneItems.isEmpty())
        return QStringLiteral("Click Repair OBS to add missing scene items.");
    if (!audit.geometryDrift.isEmpty())
        return QStringLiteral("Click Repair OBS to re-apply Look geometry.");
    if (!audit.staleDesignLayers.isEmpty())
        return QStringLiteral("Click Repair OBS to hide stale design layers.");
    return QStringLiteral("Click Repair OBS to reconcile Sidecar with OBS.");
}

void MainWindow::openObsSyncInspector()
{
    auto *dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle("CoreVideo OBS Sync Inspector");
    dlg->resize(980, 640);
    dlg->setStyleSheet(sidecar_stylesheet(nullptr));

    auto *root = new QVBoxLayout(dlg);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    auto *summary = new QLabel(dlg);
    summary->setWordWrap(true);
    summary->setTextInteractionFlags(Qt::TextSelectableByMouse);
    root->addWidget(summary);

    auto *report = new QPlainTextEdit(dlg);
    report->setReadOnly(true);
    report->setMinimumHeight(150);
    report->setMaximumBlockCount(500);
    report->setPlaceholderText("Run validation to compare OBS scenes, sources, layers, and geometry with the Sidecar Look catalog.");
    root->addWidget(report);

    auto *table = new QTableWidget(dlg);
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels({"Category", "OBS item"});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    root->addWidget(table, 1);

    auto refresh = [this, summary, report, table]() {
        if (!m_obsClient || !m_obsClient->isConnected()) {
            summary->setText("OBS is not connected. Connect OBS before inspecting scene sync.");
            report->setPlainText("No live OBS audit is available until Sidecar is connected to obs-websocket.");
            table->setRowCount(0);
            return;
        }

        refreshObsAuditInventory();
        const auto audit =
            m_obsClient->coreVideoSceneAudit(sourceNamesForSlots(8), lookRenderPlans());
        summary->setText(obsAuditSummaryText(audit));
        report->setPlainText(obsAuditReportText(audit, 20));

        table->setRowCount(0);
        fillInspectorTable(table, "Missing scene", audit.missingScenes);
        fillInspectorTable(table, "Missing input", audit.missingInputs);
        fillInspectorTable(table, "Missing scene item", audit.missingSceneItems);
        fillInspectorTable(table, "Stale design layer", audit.staleDesignLayers);
        fillInspectorTable(table, "Geometry drift", audit.geometryDrift);
        if (table->rowCount() == 0) {
            fillInspectorTable(table, "OK", {
                QStringLiteral("CoreVideo OBS scene graph matches the Sidecar Look catalog.")
            });
        }
    };

    auto *buttons = new QHBoxLayout;
    auto *refreshBtn = new QPushButton("Refresh", dlg);
    auto *validateBtn = new QPushButton("Validate Live", dlg);
    auto *syncBtn = new QPushButton("Sync OBS", dlg);
    auto *repairValidateBtn = new QPushButton("Repair + Validate", dlg);
    auto *repairGeometryBtn = new QPushButton("Repair Geometry", dlg);
    auto *repairBtn = new QPushButton("Hide Stale Layers", dlg);
    auto *closeBtn = new QPushButton("Close", dlg);
    buttons->addWidget(refreshBtn);
    buttons->addWidget(validateBtn);
    buttons->addWidget(syncBtn);
    buttons->addWidget(repairValidateBtn);
    buttons->addWidget(repairGeometryBtn);
    buttons->addWidget(repairBtn);
    buttons->addStretch(1);
    buttons->addWidget(closeBtn);
    root->addLayout(buttons);

    connect(refreshBtn, &QPushButton::clicked, dlg, refresh);
    connect(validateBtn, &QPushButton::clicked, dlg, [this, summary, report, refresh]() {
        if (!m_obsClient || !m_obsClient->isConnected()) {
            refresh();
            return;
        }
        summary->setText("Validating live OBS scene graph...");
        report->setPlainText("Refreshing OBS inventory. This verifies the actual scenes, sources, scene items, stale design layers, and geometry currently in OBS.");
        refreshObsAuditInventory();
        QTimer::singleShot(1600, this, refresh);
    });
    connect(syncBtn, &QPushButton::clicked, dlg, [this, refresh]() {
        reconcileObsSceneGraph();
        QTimer::singleShot(7000, this, refresh);
    });
    connect(repairValidateBtn, &QPushButton::clicked, dlg, [this, summary, report, refresh]() {
        if (!m_obsClient || !m_obsClient->isConnected()) {
            refresh();
            return;
        }
        summary->setText("Repairing and validating OBS scene graph...");
        report->setPlainText("Creating missing CoreVideo scenes and sources, applying Look geometry, hiding stale design layers, then running a live audit.");
        reconcileObsSceneGraph();
        QTimer::singleShot(7600, this, refresh);
    });
    connect(repairGeometryBtn, &QPushButton::clicked, dlg, [this, refresh]() {
        for (const Look &look : allLooks())
            repairLookGeometry(look);
        QTimer::singleShot(3000, this, refresh);
    });
    connect(repairBtn, &QPushButton::clicked, dlg, [this, refresh]() {
        if (m_obsClient && m_obsClient->isConnected())
            m_obsClient->hideStaleCoreVideoDesignLayers(lookRenderPlans());
        QTimer::singleShot(1200, this, refresh);
    });
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);

    refresh();
    dlg->show();
}

void MainWindow::renderLookToOBS(const Look &look, bool makeProgram)
{
    if (!m_obsClient || !m_obsClient->isConnected() || !m_settingsPage)
        return;
    if (!look.tmpl.isValid()) {
        onObsLog(QStringLiteral("Look '%1' has no valid template to render.").arg(look.name));
        return;
    }

    const QString scene = obsSceneNameForLook(look);
    m_obsSyncState = ObsSyncState::Applying;
    m_lastSyncError.clear();
    m_lastRenderedLookName = look.name;
    m_lastRenderedSceneName = scene;
    m_lastExpectedDesignLayers = obsRenderer().designLayerSourceNames(look);
    if (m_sceneSyncStatusLabel) {
        m_sceneSyncStatusLabel->setText(QStringLiteral("Applying %1").arg(look.name.left(24)));
        m_sceneSyncStatusLabel->setToolTip(QStringLiteral("Rendering Look '%1' into OBS scene '%2'.")
                                           .arg(look.name, scene));
        m_sceneSyncStatusLabel->setStyleSheet("color: #e0a020; font-size: 11px; background: transparent;");
    }
    obsRenderer().renderLook(look, makeProgram, slotLabelsForLook(look));
    QTimer::singleShot(3300, this, [this, scene]() {
        if (m_obsClient && m_obsClient->isConnected())
            m_obsClient->requestSceneItems(scene);
    });
    QTimer::singleShot(4200, this, [this]() {
        if (!m_obsClient || !m_obsClient->isConnected())
            return;
        refreshObsAuditInventory();
        m_obsClient->hideStaleCoreVideoDesignLayers(lookRenderPlans());
    });
    QTimer::singleShot(6800, this, [this, scene]() {
        if (!m_obsClient || !m_obsClient->isConnected())
            return;

        validateObsSceneGraphStatus(QStringLiteral("Render %1").arg(scene), true);
    });
    onObsLog(QStringLiteral("Rendered Look '%1' to OBS scene '%2'.")
                 .arg(look.name, scene));
}

void MainWindow::reconcileParticipantSlots(const QVector<ParticipantInfo> &participants)
{
    QVector<ParticipantInfo> merged;
    merged.reserve(std::max<int>(static_cast<int>(participants.size()), 8));
    QSet<int> usedSlots;

    for (auto participant : participants) {
        participant.slotAssign = -1;
        for (const auto &existing : m_participants) {
            if (existing.id != participant.id)
                continue;
            if (isValidSidecarSlot(existing.slotAssign)
                && !usedSlots.contains(existing.slotAssign)) {
                participant.slotAssign = existing.slotAssign;
                usedSlots.insert(existing.slotAssign);
            }
            break;
        }
        merged.append(participant);
    }

    int nextSlot = 0;
    for (auto &participant : merged) {
        if (participant.slotAssign >= 0 || !participant.hasVideo)
            continue;
        while (nextSlot < 8 && usedSlots.contains(nextSlot))
            ++nextSlot;
        if (nextSlot >= 8)
            break;
        participant.slotAssign = nextSlot;
        usedSlots.insert(nextSlot);
    }

    for (int slot = 0; slot < 8; ++slot) {
        if (usedSlots.contains(slot))
            continue;

        ParticipantInfo placeholder = sidecarPlaceholderParticipant(slot);
        usedSlots.insert(placeholder.slotAssign);
        merged.append(placeholder);
    }

    std::sort(merged.begin(), merged.end(), [](const ParticipantInfo &a,
                                               const ParticipantInfo &b) {
        const bool aAssigned = isValidSidecarSlot(a.slotAssign);
        const bool bAssigned = isValidSidecarSlot(b.slotAssign);
        if (aAssigned != bAssigned)
            return aAssigned;
        if (aAssigned && a.slotAssign != b.slotAssign)
            return a.slotAssign < b.slotAssign;
        if (isSidecarPlaceholderId(a.id) != isSidecarPlaceholderId(b.id))
            return !isSidecarPlaceholderId(a.id);
        return a.name.localeAwareCompare(b.name) < 0;
    });

    m_participants = merged;
    m_working = lookWithCurrentAssignments(m_working);
    updateParticipantSyncedLowerThirds();
    if (m_bus)
        m_bus->stageLook(m_working);
}

void MainWindow::updateParticipantSyncedLowerThirds()
{
    if (!m_working.tmpl.isValid())
        return;

    m_working.overlays.erase(
        std::remove_if(m_working.overlays.begin(), m_working.overlays.end(),
                       [](const Overlay &ov) {
                           return LowerThirdController::isAutoLowerThird(ov);
                       }),
        m_working.overlays.end());

    const QVector<Overlay> generated =
        m_lowerThirds.participantSyncedOverlays(m_working, m_participants);
    for (const Overlay &ov : generated)
        m_working.overlays.append(ov);

    if (m_bus)
        m_bus->stageLook(m_working);
    if (m_overlayPanel)
        m_overlayPanel->setActiveOverlays(m_working.overlays);
}

void MainWindow::syncZoomOutputAssignments()
{
    if (!m_zoomClient || !m_settingsPage)
        return;

    QHash<int, int> current;
    for (const auto &participant : m_participants) {
        if (isSidecarPlaceholderId(participant.id))
            continue;
        if (!participant.hasVideo || participant.slotAssign < 0 || participant.slotAssign >= 8)
            continue;
        current.insert(participant.slotAssign, participant.id);
    }

    const QStringList sources = sourceNamesForSlots(8);
    for (auto it = current.constBegin(); it != current.constEnd(); ++it) {
        if (m_lastSyncedSlotParticipants.value(it.key(), -1) == it.value())
            continue;
        const QString sourceName = sources.value(it.key());
        if (!sourceName.isEmpty()) {
            const AudioRouting routing =
                m_slotRouting.value(it.key(), AudioRouting::Mixed);
            m_zoomClient->assignOutput(sourceName, it.value(), routing, "mono");
        }
    }

    for (auto it = m_lastSyncedSlotParticipants.constBegin();
         it != m_lastSyncedSlotParticipants.constEnd(); ++it) {
        if (current.contains(it.key()))
            continue;
        const QString sourceName = sources.value(it.key());
        if (!sourceName.isEmpty())
            m_zoomClient->assignOutput(sourceName, 0, AudioRouting::Mixed, "mono");
    }

    m_lastSyncedSlotParticipants = current;
}

void MainWindow::openParticipantMappingWindow()
{
    auto *dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle("CoreVideo Participant Mapping");
    dlg->resize(760, 520);

    auto *root = new QVBoxLayout(dlg);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    auto *status = new QLabel(dlg);
    status->setWordWrap(true);
    root->addWidget(status);

    auto refreshStatus = [this, status]() {
        const QString obs = (m_obsClient && m_obsClient->isConnected())
            ? QStringLiteral("connected")
            : QStringLiteral("not connected");
        QString text = QStringLiteral("OBS %1. Zoom video participants: %2. Known CoreVideo sources: %3.")
                           .arg(obs)
                           .arg(m_participants.size())
                           .arg(m_outputSources.size());
        if (m_obsClient && m_obsClient->isConnected()) {
            const auto audit = m_obsClient->coreVideoSceneAudit(sourceNamesForSlots(8), lookRenderPlans());
            text += QStringLiteral("\nScene graph: %1/%2 scenes, %3/%4 inputs, %5/%6 scene items.")
                        .arg(audit.presentScenes)
                        .arg(audit.expectedScenes)
                        .arg(audit.presentInputs)
                        .arg(audit.expectedInputs)
                        .arg(audit.presentSceneItems)
                        .arg(audit.expectedSceneItems);
            if (!audit.missingSceneItems.isEmpty())
                text += QStringLiteral("\nMissing scene items: %1").arg(audit.missingSceneItems.mid(0, 6).join(", "));
            if (!audit.staleDesignLayers.isEmpty())
                text += QStringLiteral("\nStale design layers: %1").arg(audit.staleDesignLayers.mid(0, 6).join(", "));
            if (!audit.geometryDrift.isEmpty())
                text += QStringLiteral("\nGeometry drift: %1").arg(audit.geometryDrift.mid(0, 4).join(", "));
        }
        status->setText(text);
    };
    refreshStatus();

    auto *actions = new QHBoxLayout;
    auto *refreshBtn = new QPushButton("Refresh Zoom/OBS", dlg);
    auto *sourceBtn = new QPushButton("Create Source Scene", dlg);
    auto *looksBtn = new QPushButton("Create Sources + Looks", dlg);
    auto *auditRepairBtn = new QPushButton("Sync OBS", dlg);
    auto *repairBtn = new QPushButton("Repair Duplicates", dlg);
    auto *openSourcesBtn = new QPushButton("Open Source Scene", dlg);
    actions->addWidget(refreshBtn);
    actions->addWidget(sourceBtn);
    actions->addWidget(looksBtn);
    actions->addWidget(auditRepairBtn);
    actions->addWidget(repairBtn);
    actions->addWidget(openSourcesBtn);
    actions->addStretch(1);
    root->addLayout(actions);

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(10);
    grid->setVerticalSpacing(8);
    grid->addWidget(new QLabel("Slot", dlg), 0, 0);
    grid->addWidget(new QLabel("OBS source", dlg), 0, 1);
    grid->addWidget(new QLabel("Zoom participant", dlg), 0, 2);
    grid->addWidget(new QLabel("Action", dlg), 0, 3);

    QStringList sources = sourceNamesForSlots(8);
    while (sources.size() < 8)
        sources << m_settingsPage->sourcePattern().arg(sources.size() + 1);

    QVector<QComboBox *> combos;
    combos.reserve(8);
    for (int i = 0; i < 8; ++i) {
        auto *slotLabel = new QLabel(QStringLiteral("Slot %1").arg(i + 1), dlg);
        auto *sourceLabel = new QLabel(sources.value(i), dlg);
        sourceLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

        auto *combo = new QComboBox(dlg);
        combo->addItem("Unassigned", -1);
        for (const auto &participant : m_participants) {
            const QString status = participant.isSharingScreen
                ? QStringLiteral("sharing")
                : (participant.hasVideo ? QStringLiteral("video") : QStringLiteral("no video"));
            combo->addItem(QStringLiteral("%1 (%2)").arg(participant.name, status),
                           participant.id);
            if (participant.slotAssign == i)
                combo->setCurrentIndex(combo->count() - 1);
        }
        combos << combo;

        auto *assignBtn = new QPushButton("Assign", dlg);
        connect(assignBtn, &QPushButton::clicked, dlg, [this, combo, i]() {
            const int participantId = combo->currentData().toInt();
            if (participantId == -1)
                clearSlotAssignment(i);
            else
                onSlotAssigned(i, participantId);
        });

        const int row = i + 1;
        grid->addWidget(slotLabel, row, 0);
        grid->addWidget(sourceLabel, row, 1);
        grid->addWidget(combo, row, 2);
        grid->addWidget(assignBtn, row, 3);
    }
    root->addLayout(grid);

    auto *shareFrame = new QFrame(dlg);
    shareFrame->setObjectName("shareMappingFrame");
    shareFrame->setStyleSheet(
        "#shareMappingFrame { background: #14141f; border: 1px solid #24243a; border-radius: 6px; }");
    auto *shareRow = new QHBoxLayout(shareFrame);
    shareRow->setContentsMargins(12, 10, 12, 10);
    shareRow->setSpacing(10);
    auto *shareTitle = new QLabel("Screen Share", shareFrame);
    shareTitle->setStyleSheet("color: #e0e0f0; font-weight: 700; background: transparent;");
    auto *shareSource = new QLabel("Zoom Screen Share", shareFrame);
    shareSource->setTextInteractionFlags(Qt::TextSelectableByMouse);
    shareSource->setStyleSheet("color: #9aa0c0; background: transparent;");
    auto *shareHint = new QLabel("Follows Zoom's active share feed", shareFrame);
    shareHint->setStyleSheet("color: #687090; background: transparent;");
    auto *assignShareBtn = new QPushButton("Assign Share Source", shareFrame);
    shareRow->addWidget(shareTitle);
    shareRow->addWidget(shareSource);
    shareRow->addWidget(shareHint, 1);
    shareRow->addWidget(assignShareBtn);
    root->addWidget(shareFrame);

    auto *footer = new QHBoxLayout;
    auto *applyBtn = new QPushButton("Apply Mapping", dlg);
    auto *closeBtn = new QPushButton("Close", dlg);
    footer->addStretch(1);
    footer->addWidget(applyBtn);
    footer->addWidget(closeBtn);
    root->addLayout(footer);

    connect(refreshBtn, &QPushButton::clicked, dlg, [this, refreshStatus]() {
        if (m_zoomClient) {
            m_zoomClient->refreshParticipants();
            m_zoomClient->refreshOutputs();
        }
        refreshObsAuditInventory();
        refreshStatus();
    });
    connect(sourceBtn, &QPushButton::clicked, dlg, [this, refreshStatus]() {
        provisionPlaceholderSources();
        onObsLog("Requested CoreVideo Sources scene and 8 participant sources in OBS.");
        refreshStatus();
    });
    connect(looksBtn, &QPushButton::clicked, dlg, [this, refreshStatus]() {
        provisionLookScenes();
        refreshStatus();
    });
    connect(auditRepairBtn, &QPushButton::clicked, dlg, [this, refreshStatus]() {
        reconcileObsSceneGraph();
        QTimer::singleShot(6600, this, [this, refreshStatus]() {
            refreshObsAuditInventory();
            updateSceneSyncStatus();
            refreshStatus();
        });
    });
    connect(repairBtn, &QPushButton::clicked, dlg, [this, refreshStatus]() {
        repairCoreVideoDuplicates();
        refreshStatus();
    });
    connect(openSourcesBtn, &QPushButton::clicked, dlg, [this]() {
        provisionPlaceholderSources();
        QTimer::singleShot(500, this, [this]() {
            if (m_obsClient && m_obsClient->isConnected())
                m_obsClient->setCurrentScene(QStringLiteral("CoreVideo Sources"));
        });
    });
    connect(assignShareBtn, &QPushButton::clicked, dlg, [this, refreshStatus]() {
        provisionPlaceholderSources();
        if (m_zoomClient)
            m_zoomClient->assignScreenShare(QStringLiteral("Zoom Screen Share"));
        refreshStatus();
    });
    connect(applyBtn, &QPushButton::clicked, dlg, [this, combos]() {
        for (int i = 0; i < combos.size(); ++i) {
            const int participantId = combos[i]->currentData().toInt();
            if (participantId == -1)
                clearSlotAssignment(i);
            else
                onSlotAssigned(i, participantId);
        }
    });
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::close);

    dlg->show();
}

void MainWindow::onPageSelected(Sidebar::Page p)
{
    using P = Sidebar::Page;
    switch (p) {
    case P::Templates:
    case P::Participants:
        m_rightStack->setCurrentIndex(m_pageTemplates);  break;
    case P::Themes:
        m_rightStack->setCurrentIndex(m_pageThemes);     break;
    case P::Scenes:
        m_rightStack->setCurrentIndex(m_pageScenes);
        m_obsClient->refreshInventory();
        break;
    case P::Overlays:
        m_rightStack->setCurrentIndex(m_pageOverlays);   break;
    case P::Macros:
        m_rightStack->setCurrentIndex(m_pageMacros);     break;
    case P::Settings:
        m_rightStack->setCurrentIndex(m_pageSettings);   break;
    }
}

void MainWindow::onTemplateSelected(const LayoutTemplate &tmpl)
{
    // Selecting a template stages it on PVW. It does NOT go on air until
    // the operator takes — that's the whole point of the M/E model.
    m_working.tmpl = tmpl;
    m_working.id   = tmpl.id;
    m_working.name = tmpl.name;
    m_working.templateId = tmpl.id;
    m_bus->stageLook(m_working);
    onObsLog(QStringLiteral("Staged on PVW: %1 — press TAKE to go on air.").arg(tmpl.name));
}

void MainWindow::onLookSelected(const Look &look)
{
    // Stage the full Look on PVW: layout + identity + (until overlays land
    // in a later slice) apply the Look's theme app-wide. Existing slot
    // assignments are carried over so swapping a Look doesn't blank the
    // participants the operator already placed.
    m_working            = lookWithCurrentAssignments(look);
    m_working.templateId = look.tmpl.id.isEmpty() ? look.templateId
                                                  : look.tmpl.id;
    m_bus->stageLook(m_working);

    if (!look.themeId.isEmpty()) {
        for (const auto &t : ShowTheme::builtIns()) {
            if (t.id == look.themeId) { onThemeSelected(t); break; }
        }
    }

    onObsLog(QStringLiteral("Staged Look on PVW: %1 — press TAKE to go on air.")
                 .arg(look.name));
    renderLookToOBS(m_working, false);
}

void MainWindow::onCreateLookRequested()
{
    if (!m_working.isValid()) {
        onObsLog("Create Look skipped: no staged layout.");
        return;
    }

    bool ok = false;
    const QString defaultName = m_working.name.isEmpty()
        ? QStringLiteral("Custom Look")
        : QStringLiteral("%1 Custom").arg(m_working.name);
    const QString name = QInputDialog::getText(this,
                                               QStringLiteral("New Look"),
                                               QStringLiteral("Look name"),
                                               QLineEdit::Normal,
                                               defaultName,
                                               &ok).trimmed();
    if (!ok || name.isEmpty())
        return;

    Look custom = m_working;
    custom.id = QStringLiteral("custom-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    custom.name = name;
    custom.category = QStringLiteral("Custom");
    custom.description = QStringLiteral("User-created look");
    custom.templateId = custom.tmpl.id.isEmpty() ? custom.templateId : custom.tmpl.id;

    m_customLooks.append(custom);
    saveCustomLooks();
    refreshLookPanel();
    onLookSelected(custom);
    onObsLog(QStringLiteral("Created custom Look: %1.").arg(custom.name));
}

void MainWindow::onSaveLookRequested()
{
    if (!m_working.isValid()) {
        onObsLog("Save Look skipped: no staged Look.");
        return;
    }

    const auto existing = std::find_if(m_customLooks.begin(), m_customLooks.end(),
        [this](const Look &look) { return look.id == m_working.id; });

    if (existing != m_customLooks.end()) {
        Look updated = m_working;
        updated.category = updated.category.isEmpty()
            ? QStringLiteral("Custom")
            : updated.category;
        updated.description = updated.description.isEmpty()
            ? QStringLiteral("User-created look")
            : updated.description;
        updated.templateId = updated.tmpl.id.isEmpty()
            ? updated.templateId
            : updated.tmpl.id;
        *existing = updated;
        saveCustomLooks();
        refreshLookPanel();
        renderLookToOBS(updated, false);
        onObsLog(QStringLiteral("Saved custom Look: %1.").arg(updated.name));
        return;
    }

    bool ok = false;
    const QString name = QInputDialog::getText(this,
                                               QStringLiteral("Save Look"),
                                               QStringLiteral("Custom Look name"),
                                               QLineEdit::Normal,
                                               QStringLiteral("%1 Custom").arg(m_working.name),
                                               &ok).trimmed();
    if (!ok || name.isEmpty())
        return;

    Look custom = m_working;
    custom.id = QStringLiteral("custom-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    custom.name = name;
    custom.category = QStringLiteral("Custom");
    custom.description = QStringLiteral("User-created look");
    custom.templateId = custom.tmpl.id.isEmpty() ? custom.templateId : custom.tmpl.id;

    m_customLooks.append(custom);
    saveCustomLooks();
    refreshLookPanel();
    onLookSelected(custom);
    onObsLog(QStringLiteral("Saved staged Look as custom Look: %1.").arg(custom.name));
}

void MainWindow::onSetBackgroundRequested()
{
    if (!m_working.isValid()) {
        onObsLog("Background skipped: no staged Look.");
        return;
    }

    const QString file = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("Choose Look Background"),
        QString(),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp *.webp);;All files (*.*)"));
    if (file.isEmpty())
        return;

    m_working.backgroundImagePath = file;
    m_bus->stageLook(m_working);

    bool updated = false;
    for (auto &look : m_customLooks) {
        if (look.id != m_working.id)
            continue;
        look.backgroundImagePath = file;
        look.slotAssignments = m_working.slotAssignments;
        look.overlays = m_working.overlays;
        updated = true;
        break;
    }
    if (updated) {
        saveCustomLooks();
        refreshLookPanel();
    } else {
        onObsLog("Background staged. Use New Look to save it as a reusable custom Look.");
    }

    renderLookToOBS(m_working, false);
}

void MainWindow::onDesignLookRequested()
{
    if (!m_working.isValid()) {
        onObsLog("Design skipped: no staged Look.");
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Look Designer"));
    dlg.resize(1120, 680);
    dlg.setStyleSheet(
        "QDialog { background: #141418; color: #f4f4ff; }"
        "QLabel { color: #d8d8ea; font-size: 12px; }"
        "QLabel#designerTitle { color: white; font-size: 18px; font-weight: 800; }"
        "QLabel#designerHint { color: #8d8da8; font-size: 11px; }"
        "QFrame#stage { background: #202028; border: 1px solid #323246; border-radius: 8px; }"
        "QFrame#inspector { background: #1a1a22; border-left: 1px solid #303044; }"
        "QDoubleSpinBox { background: #101018; color: #f0f0ff; border: 1px solid #34344a; border-radius: 4px; padding: 4px; }"
        "QPushButton { background: #262638; color: white; border: 1px solid #3a3a55; border-radius: 6px; padding: 7px 10px; }"
        "QPushButton:hover { background: #303050; }"
        "QCheckBox { color: #d8d8ea; spacing: 8px; }");

    auto *root = new QHBoxLayout(&dlg);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *stage = new QFrame(&dlg);
    stage->setObjectName("stage");
    auto *stageV = new QVBoxLayout(stage);
    stageV->setContentsMargins(24, 20, 24, 24);
    stageV->setSpacing(12);
    auto *title = new QLabel(m_working.name, stage);
    title->setObjectName("designerTitle");
    auto *hint = new QLabel(QStringLiteral("Design the reusable broadcast look; participant slots remain linked to shared OBS sources."), stage);
    hint->setObjectName("designerHint");
    auto *designerPreview = new PreviewCanvas(stage);
    designerPreview->setMinimumSize(720, 405);
    designerPreview->setTemplate(m_working.tmpl);
    designerPreview->setParticipants(participantsForLook(m_working));
    designerPreview->setOverlays(m_working.overlays);
    designerPreview->setBackgroundImage(m_working.backgroundImagePath);
    designerPreview->setTileStyle(m_working.tileStyle);
    designerPreview->setLayoutEditingEnabled(true);
    stageV->addWidget(title);
    stageV->addWidget(hint);
    stageV->addWidget(designerPreview, 1);
    root->addWidget(stage, 1);

    auto *inspector = new QFrame(&dlg);
    inspector->setObjectName("inspector");
    inspector->setFixedWidth(320);
    auto *layout = new QVBoxLayout(inspector);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(10);
    auto *inspectorTitle = new QLabel(QStringLiteral("Gallery"), inspector);
    inspectorTitle->setObjectName("designerTitle");
    auto *inspectorHint = new QLabel(QStringLiteral("Canvas, tile, and participant display"), inspector);
    inspectorHint->setObjectName("designerHint");
    layout->addWidget(inspectorTitle);
    layout->addWidget(inspectorHint);

    LayoutTemplate editedTemplate = m_working.tmpl;
    auto *slotSelector = new QComboBox(&dlg);
    for (const TemplateSlot &slot : editedTemplate.slotList) {
        const QString label = slot.label.trimmed().isEmpty()
            ? QStringLiteral("Slot %1").arg(slot.index + 1)
            : QStringLiteral("Slot %1 - %2").arg(slot.index + 1).arg(slot.label);
        slotSelector->addItem(label, slot.index);
    }

    auto makeGeometrySpin = [&dlg]() {
        auto *spin = new QDoubleSpinBox(&dlg);
        spin->setRange(0.0, 1.0);
        spin->setDecimals(3);
        spin->setSingleStep(0.01);
        return spin;
    };
    auto *slotX = makeGeometrySpin();
    auto *slotY = makeGeometrySpin();
    auto *slotW = makeGeometrySpin();
    auto *slotH = makeGeometrySpin();

    auto selectedSlotPosition = [&]() {
        const int index = slotSelector->currentData().toInt();
        for (int i = 0; i < editedTemplate.slotList.size(); ++i) {
            if (editedTemplate.slotList[i].index == index)
                return i;
        }
        return -1;
    };
    auto loadSelectedSlot = [&]() {
        const int pos = selectedSlotPosition();
        const bool ok = pos >= 0;
        for (auto *spin : {slotX, slotY, slotW, slotH})
            spin->blockSignals(true);
        if (ok) {
            const TemplateSlot &slot = editedTemplate.slotList[pos];
            slotX->setValue(slot.x);
            slotY->setValue(slot.y);
            slotW->setValue(slot.width);
            slotH->setValue(slot.height);
        }
        for (auto *spin : {slotX, slotY, slotW, slotH})
            spin->blockSignals(false);
        if (ok)
            designerPreview->setSelectedSlot(editedTemplate.slotList[pos].index);
    };
    auto applySelectedSlot = [&]() {
        const int pos = selectedSlotPosition();
        if (pos < 0)
            return;
        TemplateSlot &slot = editedTemplate.slotList[pos];
        slot.x = qBound(0.0, slotX->value(), 0.99);
        slot.y = qBound(0.0, slotY->value(), 0.99);
        slot.width = qBound(0.01, slotW->value(), 1.0 - slot.x);
        slot.height = qBound(0.01, slotH->value(), 1.0 - slot.y);
        designerPreview->setTemplate(editedTemplate);
        designerPreview->setSelectedSlot(slot.index);
    };
    connect(slotSelector, qOverload<int>(&QComboBox::currentIndexChanged), &dlg, loadSelectedSlot);
    connect(slotX, qOverload<double>(&QDoubleSpinBox::valueChanged), &dlg, applySelectedSlot);
    connect(slotY, qOverload<double>(&QDoubleSpinBox::valueChanged), &dlg, applySelectedSlot);
    connect(slotW, qOverload<double>(&QDoubleSpinBox::valueChanged), &dlg, applySelectedSlot);
    connect(slotH, qOverload<double>(&QDoubleSpinBox::valueChanged), &dlg, applySelectedSlot);
    connect(designerPreview, &PreviewCanvas::slotClicked, &dlg, [&](int slotIndex) {
        const int row = slotSelector->findData(slotIndex);
        if (row >= 0)
            slotSelector->setCurrentIndex(row);
    });
    connect(designerPreview, &PreviewCanvas::slotGeometryChanged,
            &dlg, [&](const LayoutTemplate &tmpl, int slotIndex) {
        editedTemplate = tmpl;
        const int row = slotSelector->findData(slotIndex);
        if (row >= 0 && slotSelector->currentIndex() != row)
            slotSelector->setCurrentIndex(row);
        loadSelectedSlot();
    });
    loadSelectedSlot();

    auto *borderWidth = new QDoubleSpinBox(&dlg);
    borderWidth->setRange(0.0, 16.0);
    borderWidth->setSingleStep(0.5);
    borderWidth->setValue(m_working.tileStyle.borderWidth);
    auto *radius = new QDoubleSpinBox(&dlg);
    radius->setRange(0.0, 80.0);
    radius->setSingleStep(1.0);
    radius->setValue(m_working.tileStyle.cornerRadius);
    radius->setEnabled(false);
    radius->setToolTip(QStringLiteral("Rounded corners need an OBS mask/matte implementation before they can be WYSIWYG."));
    auto *opacity = new QDoubleSpinBox(&dlg);
    opacity->setRange(0.1, 1.0);
    opacity->setSingleStep(0.05);
    opacity->setValue(m_working.tileStyle.opacity);

    auto *shadow = new QCheckBox(QStringLiteral("Drop shadow"), &dlg);
    shadow->setChecked(m_working.tileStyle.dropShadow);
    auto *names = new QCheckBox(QStringLiteral("Show name tags"), &dlg);
    names->setChecked(m_working.tileStyle.showNameTag);
    auto *videoOnly = new QCheckBox(QStringLiteral("Exclude participants without video"), &dlg);
    videoOnly->setChecked(m_working.tileStyle.excludeNoVideo);

    QColor selectedCanvas = m_working.tileStyle.canvasColor;
    QColor selectedBorder = m_working.tileStyle.borderColor;
    auto *canvasColor = new QPushButton(selectedCanvas.name(), &dlg);
    connect(canvasColor, &QPushButton::clicked, &dlg, [&]() {
        const QColor c = QColorDialog::getColor(selectedCanvas, &dlg, QStringLiteral("Canvas Color"));
        if (!c.isValid()) return;
        selectedCanvas = c;
        canvasColor->setText(c.name());
        TileStyle s = m_working.tileStyle;
        s.canvasColor = selectedCanvas;
        designerPreview->setTileStyle(s);
    });
    auto *borderColor = new QPushButton(selectedBorder.name(), &dlg);
    connect(borderColor, &QPushButton::clicked, &dlg, [&]() {
        const QColor c = QColorDialog::getColor(selectedBorder, &dlg, QStringLiteral("Tile Border"));
        if (!c.isValid()) return;
        selectedBorder = c;
        borderColor->setText(c.name());
        TileStyle s = m_working.tileStyle;
        s.canvasColor = selectedCanvas;
        s.borderColor = selectedBorder;
        designerPreview->setTileStyle(s);
    });

    auto refreshDesignerPreview = [&]() {
        TileStyle s = m_working.tileStyle;
        s.canvasColor = selectedCanvas;
        s.borderColor = selectedBorder;
        s.borderWidth = borderWidth->value();
        s.cornerRadius = radius->value();
        s.opacity = opacity->value();
        s.dropShadow = shadow->isChecked();
        s.showNameTag = names->isChecked();
        s.excludeNoVideo = videoOnly->isChecked();
        designerPreview->setTileStyle(s);
    };
    connect(borderWidth, qOverload<double>(&QDoubleSpinBox::valueChanged), &dlg, refreshDesignerPreview);
    connect(radius, qOverload<double>(&QDoubleSpinBox::valueChanged), &dlg, refreshDesignerPreview);
    connect(opacity, qOverload<double>(&QDoubleSpinBox::valueChanged), &dlg, refreshDesignerPreview);
    connect(shadow, &QCheckBox::toggled, &dlg, refreshDesignerPreview);
    connect(names, &QCheckBox::toggled, &dlg, refreshDesignerPreview);
    connect(videoOnly, &QCheckBox::toggled, &dlg, refreshDesignerPreview);

    auto draftLookForDesigner = [&]() {
        applySelectedSlot();
        Look draft = m_working;
        draft.tmpl = editedTemplate;
        draft.tileStyle.canvasColor = selectedCanvas;
        draft.tileStyle.borderColor = selectedBorder;
        draft.tileStyle.borderWidth = borderWidth->value();
        draft.tileStyle.cornerRadius = 0.0;
        draft.tileStyle.opacity = opacity->value();
        draft.tileStyle.dropShadow = shadow->isChecked();
        draft.tileStyle.showNameTag = names->isChecked();
        draft.tileStyle.excludeNoVideo = videoOnly->isChecked();
        return lookWithCurrentAssignments(draft);
    };

    auto addRow = [&](const QString &label, QWidget *control) {
        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel(label, &dlg));
        row->addWidget(control);
        layout->addLayout(row);
    };
    auto addTitle = [&](const QString &text) {
        auto *label = new QLabel(text, &dlg);
        label->setStyleSheet("QLabel { color: #ffffff; font-weight: 800; padding-top: 8px; }");
        layout->addWidget(label);
    };
    addTitle(QStringLiteral("Layout"));
    addRow(QStringLiteral("Slot"), slotSelector);
    addRow(QStringLiteral("X"), slotX);
    addRow(QStringLiteral("Y"), slotY);
    addRow(QStringLiteral("Width"), slotW);
    addRow(QStringLiteral("Height"), slotH);
    addTitle(QStringLiteral("Style"));
    addRow(QStringLiteral("Canvas color"), canvasColor);
    addRow(QStringLiteral("Border width"), borderWidth);
    addRow(QStringLiteral("Corner radius"), radius);
    auto *radiusHint = new QLabel(QStringLiteral("Rounded corners are disabled until OBS mask rendering is implemented."), &dlg);
    radiusHint->setObjectName("designerHint");
    radiusHint->setWordWrap(true);
    layout->addWidget(radiusHint);
    addRow(QStringLiteral("Tile opacity"), opacity);
    addRow(QStringLiteral("Border color"), borderColor);
    layout->addWidget(shadow);
    layout->addWidget(names);
    layout->addWidget(videoOnly);

    auto *obsTitle = new QLabel(QStringLiteral("OBS Round Trip"), &dlg);
    obsTitle->setStyleSheet("QLabel { color: #ffffff; font-weight: 800; padding-top: 8px; }");
    layout->addWidget(obsTitle);
    auto *obsStatus = new QLabel(QStringLiteral("Render the draft to OBS preview, then verify the OBS scene graph before saving."), &dlg);
    obsStatus->setObjectName("designerHint");
    obsStatus->setWordWrap(true);
    layout->addWidget(obsStatus);
    auto *obsButtons = new QHBoxLayout;
    auto *renderDraftBtn = new QPushButton(QStringLiteral("Render Draft"), &dlg);
    auto *verifyDraftBtn = new QPushButton(QStringLiteral("Verify OBS"), &dlg);
    auto *repairDraftBtn = new QPushButton(QStringLiteral("Repair Geometry"), &dlg);
    auto *inspectObsBtn = new QPushButton(QStringLiteral("Inspect"), &dlg);
    obsButtons->addWidget(renderDraftBtn);
    obsButtons->addWidget(verifyDraftBtn);
    obsButtons->addWidget(repairDraftBtn);
    obsButtons->addWidget(inspectObsBtn);
    layout->addLayout(obsButtons);

    auto updateDraftObsStatus = [this, obsStatus](const Look &draft) {
        if (!m_obsClient || !m_obsClient->isConnected()) {
            obsStatus->setText(QStringLiteral("OBS is not connected."));
            return;
        }
        refreshObsAuditInventory();
        m_obsClient->requestSceneItems(obsSceneNameForLook(draft));
        QVector<LookRenderPlan> plans;
        const auto plan = obsRenderer().renderPlanForLook(draft, false, slotLabelsForLook(draft));
        if (plan.valid)
            plans << plan;
        const auto audit = m_obsClient->coreVideoSceneAudit(sourceNamesForSlots(8), plans);
        const QString state = audit.isClean()
            ? QStringLiteral("OBS scene graph is synced.")
            : QStringLiteral("OBS still has drift.");
        QStringList detail;
        if (!audit.inventoryReady)
            detail << QStringLiteral("inventory loading");
        if (!audit.missingScenes.isEmpty())
            detail << QStringLiteral("%1 missing scene(s)").arg(audit.missingScenes.size());
        if (!audit.missingInputs.isEmpty())
            detail << QStringLiteral("%1 missing input(s)").arg(audit.missingInputs.size());
        if (!audit.missingSceneItems.isEmpty())
            detail << QStringLiteral("%1 missing scene item(s)").arg(audit.missingSceneItems.size());
        if (!audit.staleDesignLayers.isEmpty())
            detail << QStringLiteral("%1 stale design layer(s)").arg(audit.staleDesignLayers.size());
        if (!audit.geometryDrift.isEmpty())
            detail << QStringLiteral("%1 geometry drift(s)").arg(audit.geometryDrift.size());
        const QStringList actions = obsAuditActionDetails(audit, 3);
        if (!actions.isEmpty())
            detail << actions;
        obsStatus->setText(detail.isEmpty()
            ? state
            : QStringLiteral("%1\n%2").arg(state, detail.join(QStringLiteral("\n"))));
    };
    connect(renderDraftBtn, &QPushButton::clicked, &dlg, [this, obsStatus, draftLookForDesigner, updateDraftObsStatus]() {
        if (!m_obsClient || !m_obsClient->isConnected()) {
            obsStatus->setText(QStringLiteral("OBS is not connected. Connect OBS before rendering the draft."));
            return;
        }
        const Look draft = draftLookForDesigner();
        obsStatus->setText(QStringLiteral("Rendering '%1' to OBS preview...").arg(draft.name));
        renderLookToOBS(draft, false);
        QTimer::singleShot(7200, this, [updateDraftObsStatus, draft]() {
            updateDraftObsStatus(draft);
        });
    });
    connect(verifyDraftBtn, &QPushButton::clicked, &dlg, [draftLookForDesigner, updateDraftObsStatus]() {
        updateDraftObsStatus(draftLookForDesigner());
    });
    connect(repairDraftBtn, &QPushButton::clicked, &dlg, [this, obsStatus, draftLookForDesigner, updateDraftObsStatus]() {
        if (!m_obsClient || !m_obsClient->isConnected()) {
            obsStatus->setText(QStringLiteral("OBS is not connected. Connect OBS before repairing geometry."));
            return;
        }
        const Look draft = draftLookForDesigner();
        obsStatus->setText(QStringLiteral("Repairing OBS geometry for '%1'...").arg(draft.name));
        repairLookGeometry(draft);
        QTimer::singleShot(3200, this, [updateDraftObsStatus, draft]() {
            updateDraftObsStatus(draft);
        });
    });
    connect(inspectObsBtn, &QPushButton::clicked, &dlg, [this]() {
        openObsSyncInspector();
    });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    layout->addStretch(1);
    layout->addWidget(buttons);
    root->addWidget(inspector);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    if (dlg.exec() != QDialog::Accepted)
        return;

    m_working.tileStyle.canvasColor = selectedCanvas;
    m_working.tileStyle.borderColor = selectedBorder;
    m_working.tileStyle.borderWidth = borderWidth->value();
    m_working.tileStyle.cornerRadius = 0.0;
    m_working.tileStyle.opacity = opacity->value();
    m_working.tileStyle.dropShadow = shadow->isChecked();
    m_working.tileStyle.showNameTag = names->isChecked();
    m_working.tileStyle.excludeNoVideo = videoOnly->isChecked();
    m_working.tmpl = editedTemplate;
    if (m_working.templateId.isEmpty())
        m_working.templateId = editedTemplate.id;
    m_working = lookWithCurrentAssignments(m_working);
    m_bus->stageLook(m_working);

    for (auto &look : m_customLooks) {
        if (look.id != m_working.id)
            continue;
        look.tmpl = m_working.tmpl;
        look.templateId = m_working.templateId;
        look.tileStyle = m_working.tileStyle;
        look.slotAssignments = m_working.slotAssignments;
        saveCustomLooks();
        refreshLookPanel();
        break;
    }

    renderLookToOBS(m_working, false);
    onObsLog(QStringLiteral("Updated Look design: %1.").arg(m_working.name));
}

void MainWindow::onThemeSelected(const ShowTheme &theme)
{
    qApp->setStyleSheet(sidecar_stylesheet(&theme));
    if (m_liveCanvas)  m_liveCanvas->setAccent(theme.accent);
    if (m_sceneCanvas) m_sceneCanvas->setAccent(theme.accent);
    onObsLog(QStringLiteral("Theme applied: %1.").arg(theme.name));
}

void MainWindow::onTake()
{
    if (!m_working.isValid()) { onObsLog("Nothing staged on PVW."); return; }
    m_bus->take();
    onObsLog(QStringLiteral("TAKE → %1 on air.").arg(m_bus->program().name));
}

void MainWindow::onAuto()
{
    if (!m_working.isValid()) { onObsLog("Nothing staged on PVW."); return; }
    const int ms = m_autoDurationCombo
        ? m_autoDurationCombo->currentData().toInt()
        : 1500;
    m_bus->autoTake(ms);
    onObsLog(QStringLiteral("AUTO %1ms → %2 on air.")
                 .arg(ms).arg(m_bus->preview().name));
}

void MainWindow::onFTB()
{
    const bool wasActive = m_bus->isFTBActive();
    m_bus->ftbToggle(500);
    onObsLog(wasActive ? "FTB OFF — restoring program."
                       : "FTB ON — fading to black.");
}

void MainWindow::onSwapBuses()
{
    m_bus->swap();
    onObsLog("Swapped PGM ⇄ PVW (off-air only).");
}

void MainWindow::onApplyLayout()
{
    if (!m_currentTemplate.isValid()) { onObsLog("No template on PGM."); return; }
    if (!m_obsClient->isConnected())  { onObsLog("Not connected — preview only."); return; }

    renderLookToOBS(m_bus->program(), true);
}

void MainWindow::onRenderPreview()
{
    if (!m_working.isValid()) { onObsLog("Nothing staged on PVW."); return; }
    if (!m_obsClient || !m_obsClient->isConnected()) {
        onObsLog("Not connected to OBS; staged Look remains sidecar-only.");
        return;
    }

    renderLookToOBS(m_working, false);
}

void MainWindow::onEngineToggle()
{
    m_engineOn = true;
    m_engineBtn->setObjectName("engineOnBtn");
    m_engineBtn->setText("Syncing");
    m_engineBtn->style()->unpolish(m_engineBtn);
    m_engineBtn->style()->polish(m_engineBtn);
    reconcileObsSceneGraph();
    QTimer::singleShot(6500, this, [this]() {
        m_engineOn = false;
        m_engineBtn->setObjectName("engineOffBtn");
        m_engineBtn->style()->unpolish(m_engineBtn);
        m_engineBtn->style()->polish(m_engineBtn);
        updateSceneSyncStatus();
    });
}

void MainWindow::onObsConnect()
{
    if (m_obsClient->isConnected()
        || m_obsClient->state() == OBSClient::State::Connecting
        || m_obsClient->state() == OBSClient::State::Reconnecting) {
        m_obsClient->disconnectFromOBS();
        return;
    }

    // Pre-populate dialog from settings page values (if user has saved)
    OBSConnectDialog dlg(m_settingsPage->obsConfig(), this);
    if (dlg.exec() != QDialog::Accepted) return;
    m_obsConfig = dlg.config();

    QSettings s;
    s.setValue("obs/host",          m_obsConfig.host);
    s.setValue("obs/port",          m_obsConfig.port);
    s.setValue("obs/password",      m_obsConfig.password);
    s.setValue("obs/autoReconnect", m_obsConfig.autoReconnect);

    m_obsClient->connectToOBS(m_obsConfig);
}

void MainWindow::onObsState(OBSClient::State s)
{
    m_obsStatusLabel->setText(m_obsClient->stateLabel());
    const bool ok = (s == OBSClient::State::Connected);
    m_obsBtn->setProperty("connected", ok ? "true" : "false");
    m_obsBtn->setText(ok ? "OBS  ●" : "OBS  ○");
    m_obsBtn->style()->unpolish(m_obsBtn);
    m_obsBtn->style()->polish(m_obsBtn);

    QString color = "#8080a0";
    switch (s) {
    case OBSClient::State::Connected:                          color = "#20c460"; break;
    case OBSClient::State::Connecting:
    case OBSClient::State::Authenticating:
    case OBSClient::State::Reconnecting:                       color = "#e0a020"; break;
    case OBSClient::State::Failed:                             color = "#e04040"; break;
    case OBSClient::State::Disconnected:                       color = "#8080a0"; break;
    }
    m_obsStatusLabel->setStyleSheet(
        QString("color: %1; font-size: 11px; background: transparent;").arg(color));
    updateSceneSyncStatus();
}

void MainWindow::updateSceneSyncStatus()
{
    if (!m_sceneSyncStatusLabel || !m_obsClient || !m_settingsPage)
        return;

    if (!m_obsClient->isConnected()) {
        m_obsSyncState = ObsSyncState::Offline;
        m_sceneSyncStatusLabel->setText("Sync offline");
        m_sceneSyncStatusLabel->setToolTip("OBS is not connected.");
        m_sceneSyncStatusLabel->setStyleSheet("color: #8080a0; font-size: 11px; background: transparent;");
        if (m_engineBtn && !m_engineOn) {
            m_engineBtn->setText(QStringLiteral("Sync OBS"));
            m_engineBtn->setToolTip(QStringLiteral("Connect OBS before syncing or repairing scenes."));
        }
        return;
    }

    if (m_obsSyncState == ObsSyncState::Error) {
        m_sceneSyncStatusLabel->setText(QStringLiteral("Sync error"));
        m_sceneSyncStatusLabel->setToolTip(m_lastSyncError.isEmpty()
            ? QStringLiteral("OBS reported a request failure.")
            : m_lastSyncError);
        m_sceneSyncStatusLabel->setStyleSheet("color: #e04040; font-size: 11px; background: transparent;");
        return;
    }

    if (m_obsSyncState == ObsSyncState::Applying) {
        m_sceneSyncStatusLabel->setText(QStringLiteral("Applying %1").arg(m_lastRenderedLookName.left(24)));
        m_sceneSyncStatusLabel->setToolTip(QStringLiteral("Rendering Look '%1' into OBS scene '%2'.")
                                           .arg(m_lastRenderedLookName, m_lastRenderedSceneName));
        m_sceneSyncStatusLabel->setStyleSheet("color: #e0a020; font-size: 11px; background: transparent;");
        return;
    }

    const auto audit = m_obsClient->coreVideoSceneAudit(sourceNamesForSlots(8), lookRenderPlans());
    const bool complete = audit.isClean();

    m_obsSyncState = complete ? ObsSyncState::Synced : ObsSyncState::Dirty;
    const QString stateLabel = obsAuditPrimaryOutcome(audit, m_lastRenderedLookName);

    m_sceneSyncStatusLabel->setText(QStringLiteral("%1  %2/%3 scenes  %4/%5 sources  %6/%7 items")
        .arg(stateLabel)
        .arg(audit.presentScenes)
        .arg(audit.expectedScenes)
        .arg(audit.presentInputs)
        .arg(audit.expectedInputs)
        .arg(audit.presentSceneItems)
        .arg(audit.expectedSceneItems));

    m_sceneSyncStatusLabel->setStyleSheet(
        QStringLiteral("color: %1; font-size: 11px; background: transparent;")
            .arg(complete ? QStringLiteral("#20c460") : QStringLiteral("#e0a020")));

    QStringList detail = obsAuditReportText(audit, 10).split(QStringLiteral("\n"));
    detail.prepend(obsAuditRepairHint(audit));
    if (!m_lastRenderedLookName.isEmpty())
        detail.prepend(QStringLiteral("Last rendered Look: %1 (%2)")
                           .arg(m_lastRenderedLookName, m_lastRenderedSceneName));
    m_sceneSyncStatusLabel->setToolTip(detail.join("\n"));

    if (m_engineBtn && !m_engineOn) {
        m_engineBtn->setText(complete ? QStringLiteral("Sync OBS")
                                      : QStringLiteral("Repair OBS"));
        m_engineBtn->setToolTip(obsAuditRepairHint(audit));
    }
}

void MainWindow::validateObsSceneGraphStatus(const QString &context, bool writeLog)
{
    if (!m_obsClient || !m_obsClient->isConnected()) {
        m_obsSyncState = ObsSyncState::Offline;
        if (writeLog)
            onObsLog(QStringLiteral("%1 validation skipped: OBS is not connected.").arg(context));
        updateSceneSyncStatus();
        return;
    }

    refreshObsAuditInventory();
    const auto audit = m_obsClient->coreVideoSceneAudit(sourceNamesForSlots(8), lookRenderPlans());
    const bool clean = audit.isClean();
    m_obsSyncState = clean ? ObsSyncState::Synced : ObsSyncState::Dirty;
    if (!clean)
        m_lastSyncError = obsAuditReportText(audit, 8);
    else
        m_lastSyncError.clear();

    if (writeLog) {
        onObsLog(QStringLiteral("%1 validation: %2")
                     .arg(context, obsAuditSummaryText(audit)));
        const QStringList actions = obsAuditActionDetails(audit, 5);
        if (actions.isEmpty()) {
            onObsLog(QStringLiteral("%1 validation: OBS scene graph matches Sidecar.").arg(context));
        } else {
            onObsLog(QStringLiteral("%1 validation actions: %2")
                         .arg(context, actions.join(QStringLiteral("; "))));
        }
    }

    updateSceneSyncStatus();
}

void MainWindow::onObsLog(const QString &msg)
{
    if (!m_logView) return;
    m_logView->appendPlainText(
        QString("[%1] %2").arg(QDateTime::currentDateTime().toString("HH:mm:ss"), msg));
}

void MainWindow::onScenesReceived(const QStringList &scenes)
{
    m_lastScenes = scenes;
    m_scenesPanel->setScenes(scenes);
    updateSceneSyncStatus();

    const QString target = m_settingsPage->targetScene();
    if (scenes.contains(target)) {
        m_obsClient->requestSceneItems(target);
    } else if (!scenes.isEmpty()) {
        onObsLog(QStringLiteral("Scene '%1' not found. Available: %2")
                     .arg(target, scenes.join(", ")));
    }
}

void MainWindow::onSceneActivated(const QString &name)
{
    stageLookFromObsScene(name);
    m_obsClient->setCurrentScene(name);
    onObsLog(QStringLiteral("Switched to scene: %1").arg(name));
}

void MainWindow::onMacroTriggered(const Macro &macro)
{
    if (!m_obsClient->isConnected()) {
        onObsLog(QStringLiteral("Macro '%1' — not connected to OBS.").arg(macro.label));
        return;
    }
    m_obsClient->executeMacro(macro);
    onObsLog(QStringLiteral("Macro '%1' triggered (%2 steps).")
                 .arg(macro.label).arg(macro.steps.size()));
}

void MainWindow::onVirtualCamToggle()
{
    if (m_obsClient->isVirtualCamActive())
        m_obsClient->stopVirtualCam();
    else
        m_obsClient->startVirtualCam();
}

void MainWindow::onVirtualCamState(bool active)
{
    m_vcamBtn->setText(active ? "⏺  V-Cam ON" : "⏺  V-Cam OFF");
    m_vcamBtn->setProperty("primary", active ? "true" : "false");
    m_vcamBtn->style()->unpolish(m_vcamBtn);
    m_vcamBtn->style()->polish(m_vcamBtn);
}

void MainWindow::onSettingsChanged()
{
    // Re-read the saved OBS config so the next connect picks it up
    m_obsConfig = m_settingsPage->obsConfig();
    onObsLog("Settings saved.");
}

void MainWindow::onPhaseSelected(ShowPhase phase)
{
    const QStringList labels = {"pre_show", "live", "post_show"};
    m_controlServer->notifyPhaseChanged(labels[int(phase)]);
    m_phase = phase;
    m_preShowBtn->setChecked(phase == ShowPhase::PreShow);
    m_liveBtn->setChecked(phase == ShowPhase::Live);
    m_postShowBtn->setChecked(phase == ShowPhase::PostShow);

    // Highlight top bar border red during LIVE
    const QString border = (phase == ShowPhase::Live) ? "#cc2020" : "#181828";
    m_topBar->setStyleSheet(
        QString("#topBar { background: #0d0d12; border-bottom: 2px solid %1; }").arg(border));

    const QStringList humanLabels = {"PRE-SHOW", "LIVE", "POST-SHOW"};
    onObsLog(QString("Show phase → %1").arg(humanLabels[int(phase)]));
}

void MainWindow::onSlotAssigned(int slotIndex, int participantId)
{
    const QVector<int> participantSlots = participantSlotIndexesForLook(m_working);
    if (!participantSlots.contains(slotIndex)) {
        onObsLog(QStringLiteral("Slot %1 is not participant-mappable for Look '%2'.")
                     .arg(slotIndex + 1).arg(m_working.name));
        return;
    }

    // Evict the old occupant of this slot, then assign the new one
    for (auto &p : m_participants) {
        if (p.slotAssign == slotIndex)
            p.slotAssign = -1;
    }
    for (auto &p : m_participants) {
        if (p.id == participantId)
            p.slotAssign = slotIndex;
    }

    // Update the staged Look's slot assignments and re-stage on PVW. PGM
    // is unchanged until the operator takes.
    m_working.slotAssignments.erase(
        std::remove_if(m_working.slotAssignments.begin(), m_working.slotAssignments.end(),
                       [slotIndex, participantId](const SlotAssignment &s) {
                           return s.slotIndex == slotIndex
                               || s.participantId == participantId;
                       }),
        m_working.slotAssignments.end());
    m_working.slotAssignments.append({slotIndex, participantId});
    updateParticipantSyncedLowerThirds();
    m_bus->stageLook(m_working);

    m_lastSyncedSlotParticipants.remove(slotIndex);
    syncZoomOutputAssignments();
    if (m_obsClient && m_obsClient->isConnected())
        renderLookToOBS(m_working, false);

    // Refresh panel to show updated slot labels
    m_participantPanel->setParticipants(m_participants);

    onObsLog(QString("Slot %1 ← %2")
             .arg(slotIndex + 1)
             .arg([&]() -> QString {
                 for (const auto &p : m_participants)
                     if (p.id == participantId) return p.name;
                 return QString::number(participantId);
             }()));
}

void MainWindow::clearSlotAssignment(int slotIndex)
{
    const QVector<int> participantSlots = participantSlotIndexesForLook(m_working);
    if (!participantSlots.contains(slotIndex))
        return;

    for (auto &p : m_participants) {
        if (p.slotAssign == slotIndex)
            p.slotAssign = -1;
    }

    m_working.slotAssignments.erase(
        std::remove_if(m_working.slotAssignments.begin(), m_working.slotAssignments.end(),
                       [slotIndex](const SlotAssignment &s) {
                           return s.slotIndex == slotIndex;
                       }),
        m_working.slotAssignments.end());
    updateParticipantSyncedLowerThirds();
    if (m_bus)
        m_bus->stageLook(m_working);

    m_lastSyncedSlotParticipants.remove(slotIndex);
    syncZoomOutputAssignments();
    if (m_obsClient && m_obsClient->isConnected())
        renderLookToOBS(m_working, false);

    if (m_participantPanel)
        m_participantPanel->setParticipants(m_participants);

    onObsLog(QStringLiteral("Slot %1 cleared.").arg(slotIndex + 1));
}

void MainWindow::onSlotClicked(int slotIndex)
{
    if (!participantSlotIndexesForLook(m_working).contains(slotIndex)) return;

    // Context-sensitive right panel: clicking a slot focuses the Templates
    // page (which hosts the participants list) and arms click-to-assign so
    // the user can pick a participant without dragging.
    m_assignTargetSlot = slotIndex;
    m_sidebar->setActivePage(Sidebar::Page::Templates);
    m_rightStack->setCurrentIndex(m_pageTemplates);
    m_participantPanel->setAssignTarget(slotIndex);
    onObsLog(QString("Selected Slot %1 — click a participant to assign.")
                 .arg(slotIndex + 1));
}

void MainWindow::onSlotRoutingCycle(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= 8) return;
    const AudioRouting next =
        nextAudioRouting(m_slotRouting.value(slotIndex, AudioRouting::Mixed));
    if (next == AudioRouting::Mixed)
        m_slotRouting.remove(slotIndex);
    else
        m_slotRouting.insert(slotIndex, next);

    // Repaint both canvases so the badge updates.
    if (m_liveCanvas)  m_liveCanvas->setSlotRouting(m_slotRouting);
    if (m_sceneCanvas) m_sceneCanvas->setSlotRouting(m_slotRouting);

    // Push the new routing to the plugin. Drop the slot from the sync cache
    // so the next call re-emits assignOutput even when the participant id
    // hasn't changed — without this the cache would short-circuit the resend.
    m_lastSyncedSlotParticipants.remove(slotIndex);
    syncZoomOutputAssignments();

    onObsLog(QString("Slot %1 audio routing → %2")
                 .arg(slotIndex + 1).arg(audioRoutingLabel(next)));
}

void MainWindow::onParticipantAssignClicked(int participantId)
{
    // assignRequested already handles the assign-mode path. This slot is
    // a hook for any future "select participant first" flow.
    Q_UNUSED(participantId);
}

void MainWindow::openCommandPalette()
{
    populateCommandPalette();
    m_commandPalette->run();
}

void MainWindow::populateCommandPalette()
{
    auto *cp = m_commandPalette;
    cp->clearCommands();

    // Navigation
    cp->addCommand("Go to Templates", "Navigate", [this]() {
        m_sidebar->setActivePage(Sidebar::Page::Templates);
        m_rightStack->setCurrentIndex(m_pageTemplates);
    });
    cp->addCommand("Go to Themes", "Navigate", [this]() {
        m_sidebar->setActivePage(Sidebar::Page::Themes);
        m_rightStack->setCurrentIndex(m_pageThemes);
    });
    cp->addCommand("Go to Scenes", "Navigate", [this]() {
        m_sidebar->setActivePage(Sidebar::Page::Scenes);
        m_rightStack->setCurrentIndex(m_pageScenes);
        m_obsClient->refreshInventory();
    });
    cp->addCommand("Go to Macros", "Navigate", [this]() {
        m_sidebar->setActivePage(Sidebar::Page::Macros);
        m_rightStack->setCurrentIndex(m_pageMacros);
    });
    cp->addCommand("Go to Settings", "Navigate", [this]() {
        m_sidebar->setActivePage(Sidebar::Page::Settings);
        m_rightStack->setCurrentIndex(m_pageSettings);
    });

    // Connection / toggles
    cp->addCommand(m_obsClient->isConnected() ? "Disconnect from OBS"
                                              : "Connect to OBS",
                   "OBS", [this]() { onObsConnect(); });
    cp->addCommand(m_obsClient->isVirtualCamActive() ? "Stop Virtual Camera"
                                                    : "Start Virtual Camera",
                   "OBS", [this]() { onVirtualCamToggle(); });
    cp->addCommand("TAKE (commit PVW → PGM)", "Switcher",
                   [this]() { onTake(); });
    cp->addCommand("AUTO transition (current duration)", "Switcher",
                   [this]() { onAuto(); });
    cp->addCommand("AUTO 0.5s", "Switcher",
                   [this]() { m_bus->autoTake(500); });
    cp->addCommand("AUTO 1.0s", "Switcher",
                   [this]() { m_bus->autoTake(1000); });
    cp->addCommand("AUTO 2.0s", "Switcher",
                   [this]() { m_bus->autoTake(2000); });
    cp->addCommand("AUTO 5.0s", "Switcher",
                   [this]() { m_bus->autoTake(5000); });
    cp->addCommand(m_bus->isFTBActive() ? "FTB OFF — restore program"
                                        : "FTB ON — fade to black",
                   "Switcher", [this]() { onFTB(); });
    cp->addCommand("SWAP buses (PGM ⇄ PVW)", "Switcher",
                   [this]() { onSwapBuses(); });

    // Overlays — fire / clear on PVW
    for (const auto &ov : Overlay::builtInPresets()) {
        const Overlay snap = ov;
        cp->addCommand(QString("Stage overlay: %1").arg(Overlay::humanLabel(ov)),
                       "Overlay", [this, snap]() {
            m_working.overlays.append(snap);
            m_bus->stageLook(m_working);
            if (m_overlayPanel) m_overlayPanel->setActiveOverlays(m_working.overlays);
        });
    }
    cp->addCommand("Clear PVW overlays", "Overlay", [this]() {
        m_working.overlays.clear();
        m_bus->stageLook(m_working);
        if (m_overlayPanel) m_overlayPanel->setActiveOverlays(m_working.overlays);
    });
    cp->addCommand("Re-apply current PGM to OBS", "OBS",
                   [this]() { onApplyLayout(); });
    cp->addCommand("Sync OBS scene graph", "OBS",
                   [this]() { reconcileObsSceneGraph(); });
    cp->addCommand("Validate live OBS scene graph", "OBS",
                   [this]() { openObsSyncInspector(); });
    cp->addCommand("Open OBS Sync Inspector", "OBS",
                   [this]() { openObsSyncInspector(); });
    cp->addCommand("Repair CoreVideo duplicate OBS scenes", "OBS",
                   [this]() { repairCoreVideoDuplicates(); });

    // Phase
    cp->addCommand("Phase: Pre-show", "Phase",
                   [this]() { onPhaseSelected(ShowPhase::PreShow); });
    cp->addCommand("Phase: Live", "Phase",
                   [this]() { onPhaseSelected(ShowPhase::Live); });
    cp->addCommand("Phase: Post-show", "Phase",
                   [this]() { onPhaseSelected(ShowPhase::PostShow); });

    // Scenes (from last OBS sync)
    for (const QString &s : m_lastScenes) {
        const QString sc = s;
        cp->addCommand(QString("Switch to scene: %1").arg(sc), "Scene",
                       [this, sc]() { onSceneActivated(sc); });
    }

    // Looks (broadcast-ready presets) — stage or take in one keystroke
    for (const auto &lk : allLooks()) {
        const Look look = lk;
        const QString name = lk.name;
        cp->addCommand(QString("Stage Look on PVW: %1").arg(name), "Look",
                       [this, look]() {
            onLookSelected(look);
        });
    }
    for (const auto &lk : allLooks()) {
        const Look look = lk;
        const QString name = lk.name;
        cp->addCommand(QString("Take Look to PGM: %1").arg(name), "Look",
                       [this, look]() {
            onLookSelected(look);
            onTake();
        });
    }

    // Templates
    for (const auto &t : TemplateManager::instance().templates()) {
        const QString id = t.id;
        const QString name = t.name;
        cp->addCommand(QString("Stage on PVW: %1").arg(name), "Template",
                       [this, id]() {
            if (const auto *tt = TemplateManager::instance().findById(id))
                onTemplateSelected(*tt);
        });
    }
    // Stage + TAKE in one step
    for (const auto &t : TemplateManager::instance().templates()) {
        const QString id = t.id;
        const QString name = t.name;
        cp->addCommand(QString("Take to PGM: %1").arg(name), "Template",
                       [this, id]() {
            if (const auto *tt = TemplateManager::instance().findById(id)) {
                onTemplateSelected(*tt);
                onTake();
            }
        });
    }

    // Themes
    for (const auto &t : ShowTheme::builtIns()) {
        const ShowTheme theme = t;
        cp->addCommand(QString("Theme: %1").arg(theme.name), "Theme",
                       [this, theme]() { onThemeSelected(theme); });
    }

    // Participants — assign to currently selected slot (or slot 1 fallback)
    for (const auto &p : m_participants) {
        const int pid = p.id;
        const QString name = p.name;
        cp->addCommand(QString("Assign %1 to current slot").arg(name),
                       "Participant", [this, pid]() {
            int slot = m_assignTargetSlot;
            if (slot < 0) slot = 0;
            if (slot < m_working.tmpl.slotList.size())
                onSlotAssigned(slot, pid);
        });
    }
}
