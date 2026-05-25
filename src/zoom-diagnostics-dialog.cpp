#include "zoom-diagnostics-dialog.h"
#include "cv-style.h"
#include "obs-zoom-version.h"
#include "zoom-engine-client.h"
#include "zoom-output-manager.h"
#include <QAbstractItemView>
#include <QColor>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QStringList>
#include <QTableWidget>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <unordered_map>

enum OutputDiagnosticColumns {
    DiagOutput = 0,
    DiagParticipant,
    DiagAssignment,
    DiagRequested,
    DiagNegotiated,
    DiagObserved,
    DiagFps,
    DiagAge,
    DiagRetries,
    DiagSdk,
    DiagState,
    DiagOutputColumnCount
};

enum EventDiagnosticColumns {
    EventTime = 0,
    EventStage,
    EventSource,
    EventParticipant,
    EventMessage,
    EventColumnCount
};

static QString state_text(MeetingState state)
{
    switch (state) {
    case MeetingState::Idle: return QStringLiteral("Idle");
    case MeetingState::Joining: return QStringLiteral("Joining");
    case MeetingState::InMeeting: return QStringLiteral("In meeting");
    case MeetingState::Leaving: return QStringLiteral("Leaving");
    case MeetingState::Recovering: return QStringLiteral("Recovering");
    case MeetingState::Failed: return QStringLiteral("Failed");
    }
    return QStringLiteral("Unknown");
}

static QString assignment_text(const ZoomOutputInfo &output)
{
    switch (output.assignment) {
    case AssignmentMode::ActiveSpeaker: return QStringLiteral("Active speaker");
    case AssignmentMode::SpotlightIndex:
        return QString("Spotlight %1").arg(output.spotlight_slot);
    case AssignmentMode::ScreenShare: return QStringLiteral("Screen share");
    case AssignmentMode::Participant:
    default: return QStringLiteral("Participant");
    }
}

static QString resolution_text(VideoResolution resolution)
{
    switch (resolution) {
    case VideoResolution::P360: return QStringLiteral("640x360");
    case VideoResolution::P1080: return QStringLiteral("1920x1080");
    case VideoResolution::P720:
    default: return QStringLiteral("1280x720");
    }
}

static QString observed_text(const ZoomOutputInfo &output)
{
    if (output.observed_width == 0 || output.observed_height == 0)
        return QStringLiteral("No signal");
    return QString("%1x%2").arg(output.observed_width).arg(output.observed_height);
}

static QString negotiated_text(const ZoomOutputInfo &output)
{
    if (output.negotiated_resolution < 0)
        return QStringLiteral("-");
    return resolution_text(static_cast<VideoResolution>(output.negotiated_resolution));
}

static QString sdk_text(const ZoomOutputInfo &output)
{
    QStringList parts;
    if (output.last_video_subscribe_code >= 0) {
        parts << QString("subscribe %1")
            .arg(output.last_video_subscribe_code == 0
                 ? QStringLiteral("ok")
                 : QString::number(output.last_video_subscribe_code));
    }
    if (output.last_set_resolution_code >= 0)
        parts << QString("setRes %1").arg(output.last_set_resolution_code);
    if (output.last_raw_status >= 0)
        parts << QString("raw %1").arg(output.last_raw_status);
    if (output.subscription_downgraded)
        parts << QStringLiteral("downgraded");
    if (!output.last_quality_stage.empty())
        parts << QString::fromStdString(output.last_quality_stage);
    return parts.isEmpty() ? QStringLiteral("-") : parts.join(QStringLiteral(" / "));
}

static QString retry_text(const ZoomOutputInfo &output)
{
    QStringList parts;
    if (output.stale_recovery_attempts > 0)
        parts << QString("stale %1").arg(output.stale_recovery_attempts);
    if (output.quality_upgrade_attempts > 0)
        parts << QString("quality %1").arg(output.quality_upgrade_attempts);
    if (output.stale_recovery_cooldown_ms > 0)
        parts << QString("stale cd %1s").arg(output.stale_recovery_cooldown_ms / 1000);
    if (output.quality_upgrade_cooldown_ms > 0)
        parts << QString("quality cd %1s").arg(output.quality_upgrade_cooldown_ms / 1000);
    return parts.isEmpty() ? QStringLiteral("-") : parts.join(QStringLiteral(" / "));
}

static QString signal_state_text(const ZoomOutputInfo &output)
{
    return QString::fromUtf8(output_health_reason_label(output.health_reason));
}

static QTableWidgetItem *readonly_item(const QString &text)
{
    auto *item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

static QString diagnostics_root_dir()
{
    const QString docs = QStandardPaths::writableLocation(
        QStandardPaths::DocumentsLocation);
    const QString base = docs.isEmpty()
        ? QDir::homePath()
        : docs;
    return QDir(base).absoluteFilePath("CoreVideo Diagnostics");
}

static QString latest_obs_log_path()
{
#if defined(_WIN32)
    const QString appdata = qEnvironmentVariable("APPDATA");
    if (appdata.isEmpty())
        return {};
    QDir logs(QDir(appdata).absoluteFilePath("obs-studio/logs"));
#else
    QDir logs(QDir::home().absoluteFilePath(".config/obs-studio/logs"));
#endif
    const QFileInfoList files = logs.entryInfoList(
        QStringList() << "*.txt",
        QDir::Files,
        QDir::Time);
    return files.isEmpty() ? QString{} : files.first().absoluteFilePath();
}

static QString assignment_id(const ZoomOutputInfo &output)
{
    switch (output.assignment) {
    case AssignmentMode::ActiveSpeaker: return QStringLiteral("active_speaker");
    case AssignmentMode::SpotlightIndex: return QStringLiteral("spotlight");
    case AssignmentMode::ScreenShare: return QStringLiteral("screen_share");
    case AssignmentMode::Participant:
    default: return QStringLiteral("participant");
    }
}

static QString resolution_id(VideoResolution resolution)
{
    switch (resolution) {
    case VideoResolution::P360: return QStringLiteral("360p");
    case VideoResolution::P1080: return QStringLiteral("1080p");
    case VideoResolution::P720:
    default: return QStringLiteral("720p");
    }
}

static QJsonObject output_json(const ZoomOutputInfo &output)
{
    QJsonObject obj;
    obj["source_uuid"] = QString::fromStdString(output.source_uuid);
    obj["source_name"] = QString::fromStdString(output.source_name);
    obj["display_name"] = output.display_name.empty()
        ? QString::fromStdString(output.source_name)
        : QString::fromStdString(output.display_name);
    obj["assignment"] = assignment_id(output);
    obj["participant_id"] = static_cast<double>(output.participant_id);
    obj["spotlight_slot"] = static_cast<double>(output.spotlight_slot);
    obj["failover_participant_id"] =
        static_cast<double>(output.failover_participant_id);
    obj["requested_resolution"] = resolution_id(output.video_resolution);
    obj["requested_width"] = static_cast<double>(
        video_resolution_width(output.video_resolution));
    obj["requested_height"] = static_cast<double>(
        video_resolution_height(output.video_resolution));
    obj["negotiated_resolution"] = output.negotiated_resolution;
    obj["negotiated_resolution_label"] = negotiated_text(output);
    obj["observed_width"] = static_cast<double>(output.observed_width);
    obj["observed_height"] = static_cast<double>(output.observed_height);
    obj["observed_fps"] = output.observed_fps;
    obj["last_frame_age_ms"] = static_cast<double>(output.last_frame_age_ms);
    obj["video_stale"] = output.video_stale;
    obj["health_reason"] = output_health_reason_id(output.health_reason);
    obj["health_label"] = output_health_reason_label(output.health_reason);
    obj["duplicate_participant_assignment"] =
        output.duplicate_participant_assignment;
    obj["stale_recovery_attempts"] =
        static_cast<double>(output.stale_recovery_attempts);
    obj["stale_recovery_cooldown_ms"] =
        static_cast<double>(output.stale_recovery_cooldown_ms);
    obj["quality_upgrade_attempts"] =
        static_cast<double>(output.quality_upgrade_attempts);
    obj["quality_upgrade_cooldown_ms"] =
        static_cast<double>(output.quality_upgrade_cooldown_ms);
    obj["subscribed_age_ms"] = static_cast<double>(output.subscribed_age_ms);
    obj["last_set_resolution_code"] = output.last_set_resolution_code;
    obj["last_video_subscribe_code"] = output.last_video_subscribe_code;
    obj["last_raw_status"] = output.last_raw_status;
    obj["last_quality_stage"] =
        QString::fromStdString(output.last_quality_stage);
    obj["last_quality_event_age_ms"] =
        static_cast<double>(output.last_quality_event_age_ms);
    obj["subscription_downgraded"] = output.subscription_downgraded;
    obj["audio_mode"] = output.audio_mode == AudioChannelMode::Stereo
        ? QStringLiteral("stereo")
        : QStringLiteral("mono");
    obj["isolate_audio"] = output.isolate_audio;
    obj["audience_audio"] = output.audience_audio;
    return obj;
}

static QJsonObject participant_json(const ParticipantInfo &participant)
{
    QJsonObject obj;
    obj["id"] = static_cast<double>(participant.user_id);
    obj["name"] = QString::fromStdString(participant.display_name);
    obj["has_video"] = participant.has_video;
    obj["is_talking"] = participant.is_talking;
    obj["is_muted"] = participant.is_muted;
    obj["is_host"] = participant.is_host;
    obj["is_co_host"] = participant.is_co_host;
    obj["raised_hand"] = participant.raised_hand;
    obj["spotlight_index"] = static_cast<double>(participant.spotlight_index);
    obj["is_sharing_screen"] = participant.is_sharing_screen;
    return obj;
}

static QJsonObject event_json(const ZoomEngineClient::DebugEvent &event)
{
    QJsonObject obj;
    obj["timestamp_ms"] = static_cast<double>(event.timestamp_ms);
    obj["stage"] = QString::fromStdString(event.stage);
    obj["source_uuid"] = QString::fromStdString(event.source_uuid);
    obj["participant_id"] = static_cast<double>(event.participant_id);
    obj["message"] = QString::fromStdString(event.message);
    return obj;
}

static bool write_text_file(const QString &path, const QString &text)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return false;
    QTextStream stream(&file);
    stream << text;
    return true;
}

ZoomDiagnosticsDialog::ZoomDiagnosticsDialog(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("Zoom Diagnostics");
    setMinimumSize(1160, 760);
    resize(1280, 820);

    m_summary = new QLabel(this);
    m_summary->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_outputs = new QTableWidget(this);
    m_outputs->setColumnCount(DiagOutputColumnCount);
    m_outputs->setHorizontalHeaderLabels({
        "Output", "Participant", "Assignment", "Requested", "Negotiated",
        "Observed", "FPS", "Frame Age", "Retries", "SDK", "State"
    });
    m_outputs->horizontalHeader()->setSectionResizeMode(DiagOutput, QHeaderView::Stretch);
    for (int col = DiagParticipant; col < DiagOutputColumnCount; ++col)
        m_outputs->horizontalHeader()->setSectionResizeMode(col, QHeaderView::ResizeToContents);
    m_outputs->verticalHeader()->setVisible(false);
    m_outputs->setSelectionMode(QAbstractItemView::SingleSelection);
    m_outputs->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_outputs->setMinimumHeight(260);

    m_events = new QTableWidget(this);
    m_events->setColumnCount(EventColumnCount);
    m_events->setHorizontalHeaderLabels({
        "Time", "Stage", "Source", "Participant", "Message"
    });
    m_events->horizontalHeader()->setSectionResizeMode(EventMessage, QHeaderView::Stretch);
    for (int col = EventTime; col < EventMessage; ++col)
        m_events->horizontalHeader()->setSectionResizeMode(col, QHeaderView::ResizeToContents);
    m_events->verticalHeader()->setVisible(false);
    m_events->setSelectionMode(QAbstractItemView::SingleSelection);
    m_events->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto *refresh_button = buttons->addButton("Refresh", QDialogButtonBox::ActionRole);
    auto *export_button = buttons->addButton("Export Diagnostics",
                                             QDialogButtonBox::ActionRole);
    connect(refresh_button, &QPushButton::clicked, this, [this]() { refresh(); });
    connect(export_button, &QPushButton::clicked, this,
            [this]() { export_diagnostics(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QWidget::hide);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_summary);
    layout->addWidget(new QLabel("Outputs", this));
    layout->addWidget(m_outputs);
    layout->addWidget(new QLabel("Recent Engine Events", this));
    layout->addWidget(m_events);
    layout->addWidget(buttons);

    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, [this]() { refresh(); });
    m_timer->start();

    setStyleSheet(cv_stylesheet());
    refresh();
}

void ZoomDiagnosticsDialog::refresh()
{
    const auto outputs = ZoomOutputManager::instance().outputs();
    const auto roster = ZoomEngineClient::instance().roster();
    const auto events = ZoomEngineClient::instance().recent_debug_events();

    std::unordered_map<uint32_t, QString> participant_names;
    for (const auto &p : roster) {
        participant_names[p.user_id] = p.display_name.empty()
            ? QString("ID %1").arg(p.user_id)
            : QString::fromStdString(p.display_name);
    }

    m_summary->setText(QString(
        "Meeting: %1   Media: %2   Outputs: %3   Participants: %4   Last error: %5")
        .arg(state_text(ZoomEngineClient::instance().state()))
        .arg(ZoomEngineClient::instance().is_media_active() ? "active" : "inactive")
        .arg(outputs.size())
        .arg(roster.size())
        .arg(QString::fromStdString(ZoomEngineClient::instance().last_error())));

    m_outputs->setRowCount(static_cast<int>(outputs.size()));
    for (int row = 0; row < static_cast<int>(outputs.size()); ++row) {
        const auto &output = outputs[row];
        const QString source_name = output.display_name.empty()
            ? QString::fromStdString(output.source_name)
            : QString::fromStdString(output.display_name);
        const QString participant = output.participant_id == 0
            ? QStringLiteral("-")
            : participant_names.count(output.participant_id)
                ? participant_names[output.participant_id]
                : QString("ID %1").arg(output.participant_id);

        m_outputs->setItem(row, DiagOutput, readonly_item(source_name));
        m_outputs->setItem(row, DiagParticipant, readonly_item(participant));
        m_outputs->setItem(row, DiagAssignment, readonly_item(assignment_text(output)));
        m_outputs->setItem(row, DiagRequested, readonly_item(resolution_text(output.video_resolution)));
        m_outputs->setItem(row, DiagNegotiated, readonly_item(negotiated_text(output)));
        m_outputs->setItem(row, DiagObserved, readonly_item(observed_text(output)));
        m_outputs->setItem(row, DiagFps, readonly_item(QString::number(output.observed_fps, 'f', 1)));
        m_outputs->setItem(row, DiagAge, readonly_item(output.last_frame_age_ms == 0
            ? QStringLiteral("-") : QString("%1 ms").arg(output.last_frame_age_ms)));
        m_outputs->setItem(row, DiagRetries, readonly_item(retry_text(output)));
        auto *sdk_item = readonly_item(sdk_text(output));
        if (output.last_video_subscribe_code > 0)
            sdk_item->setForeground(QColor(255, 107, 107));
        else if (output.subscription_downgraded)
            sdk_item->setForeground(QColor(240, 180, 41));
        m_outputs->setItem(row, DiagSdk, sdk_item);
        auto *state_item = readonly_item(signal_state_text(output));
        if (output.health_reason == ZoomOutputHealthReason::DuplicateAssignment)
            state_item->setForeground(QColor(255, 107, 107));
        else if (output.health_reason == ZoomOutputHealthReason::StaleFrame)
            state_item->setForeground(Qt::red);
        else if (output.health_reason == ZoomOutputHealthReason::ZoomDeliveredLowerResolution)
            state_item->setForeground(QColor(240, 180, 41));
        m_outputs->setItem(row, DiagState, state_item);
    }

    const int max_rows = std::min<int>(static_cast<int>(events.size()), 150);
    m_events->setRowCount(max_rows);
    for (int row = 0; row < max_rows; ++row) {
        const auto &event = events[events.size() - 1 - row];
        m_events->setItem(row, EventTime,
                          readonly_item(QString::number(event.timestamp_ms)));
        m_events->setItem(row, EventStage,
                          readonly_item(QString::fromStdString(event.stage)));
        m_events->setItem(row, EventSource,
                          readonly_item(QString::fromStdString(event.source_uuid)));
        m_events->setItem(row, EventParticipant,
                          readonly_item(event.participant_id == 0
                              ? QStringLiteral("-")
                              : QString::number(event.participant_id)));
        auto *message = readonly_item(QString::fromStdString(event.message));
        message->setToolTip(QString::fromStdString(event.message));
        m_events->setItem(row, EventMessage, message);
    }
}

void ZoomDiagnosticsDialog::export_diagnostics()
{
    const auto outputs = ZoomOutputManager::instance().outputs();
    const auto roster = ZoomEngineClient::instance().roster();
    const auto events = ZoomEngineClient::instance().recent_debug_events();
    const QDateTime now = QDateTime::currentDateTime();
    const QString stamp = now.toString("yyyyMMdd-HHmmss");
    QDir root(diagnostics_root_dir());
    if (!root.exists() && !root.mkpath(".")) {
        QMessageBox::warning(this, "Export Diagnostics",
            QString("Could not create diagnostics folder:\n%1")
                .arg(root.absolutePath()));
        return;
    }

    const QString bundle_path = root.absoluteFilePath("CoreVideo-" + stamp);
    QDir bundle(bundle_path);
    if (!bundle.exists() && !root.mkpath("CoreVideo-" + stamp)) {
        QMessageBox::warning(this, "Export Diagnostics",
            QString("Could not create diagnostics bundle:\n%1")
                .arg(bundle_path));
        return;
    }

    QJsonArray output_array;
    for (const auto &output : outputs)
        output_array.append(output_json(output));

    QJsonArray roster_array;
    for (const auto &participant : roster)
        roster_array.append(participant_json(participant));

    QJsonArray event_array;
    for (const auto &event : events)
        event_array.append(event_json(event));

    const QString obs_log = latest_obs_log_path();
    const QString copied_log_name = obs_log.isEmpty()
        ? QString{}
        : QStringLiteral("obs-latest.log");
    if (!obs_log.isEmpty())
        QFile::copy(obs_log, bundle.absoluteFilePath(copied_log_name));

    QJsonObject summary;
    summary["created_at"] = now.toString(Qt::ISODate);
    summary["plugin_version"] = OBS_ZOOM_PLUGIN_VERSION;
    summary["meeting_state"] = state_text(ZoomEngineClient::instance().state());
    summary["media_active"] = ZoomEngineClient::instance().is_media_active();
    summary["authenticated"] = ZoomEngineClient::instance().is_authenticated();
    summary["active_speaker_id"] =
        static_cast<double>(ZoomEngineClient::instance().active_speaker_id());
    summary["raw_active_speaker_id"] =
        static_cast<double>(ZoomEngineClient::instance().raw_active_speaker_id());
    summary["last_error"] =
        QString::fromStdString(ZoomEngineClient::instance().last_error());
    summary["obs_log_source"] = obs_log;
    summary["obs_log_copy"] = copied_log_name;
    summary["outputs"] = output_array;
    summary["participants"] = roster_array;
    summary["recent_engine_events"] = event_array;

    QFile json_file(bundle.absoluteFilePath("summary.json"));
    if (!json_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, "Export Diagnostics",
            QString("Could not write summary.json in:\n%1")
                .arg(bundle.absolutePath()));
        return;
    }
    json_file.write(QJsonDocument(summary).toJson(QJsonDocument::Indented));
    json_file.close();

    QString text;
    QTextStream out(&text);
    out << "CoreVideo Diagnostics\n";
    out << "Created: " << now.toString(Qt::ISODate) << "\n";
    out << "Plugin version: " << OBS_ZOOM_PLUGIN_VERSION << "\n";
    out << "Meeting: " << state_text(ZoomEngineClient::instance().state()) << "\n";
    out << "Media: "
        << (ZoomEngineClient::instance().is_media_active() ? "active" : "inactive")
        << "\n";
    out << "Authenticated: "
        << (ZoomEngineClient::instance().is_authenticated() ? "yes" : "no")
        << "\n";
    out << "Active speaker: " << ZoomEngineClient::instance().active_speaker_id()
        << "\n";
    out << "Raw active speaker: "
        << ZoomEngineClient::instance().raw_active_speaker_id() << "\n";
    out << "Last error: "
        << QString::fromStdString(ZoomEngineClient::instance().last_error())
        << "\n";
    out << "OBS log: " << (obs_log.isEmpty() ? QStringLiteral("not found") : obs_log)
        << "\n\n";

    out << "Outputs\n";
    for (const auto &output : outputs) {
        out << "- " << (output.display_name.empty()
                ? QString::fromStdString(output.source_name)
                : QString::fromStdString(output.display_name))
            << " | " << assignment_text(output)
            << " | participant " << output.participant_id
            << " | requested " << resolution_text(output.video_resolution)
            << " | negotiated " << negotiated_text(output)
            << " | observed " << observed_text(output)
            << " | fps " << QString::number(output.observed_fps, 'f', 1)
            << " | sdk " << sdk_text(output)
            << " | " << output_health_reason_label(output.health_reason)
            << "\n";
    }

    out << "\nParticipants\n";
    for (const auto &participant : roster) {
        out << "- " << QString::fromStdString(participant.display_name)
            << " (" << participant.user_id << ")"
            << " video=" << (participant.has_video ? "on" : "off")
            << " audio=" << (participant.is_muted ? "muted" : "open")
            << " talking=" << (participant.is_talking ? "yes" : "no")
            << " sharing=" << (participant.is_sharing_screen ? "yes" : "no")
            << "\n";
    }

    out << "\nRecent Engine Events\n";
    const int start = std::max<int>(0, static_cast<int>(events.size()) - 50);
    for (int i = start; i < static_cast<int>(events.size()); ++i) {
        const auto &event = events[i];
        out << "- " << event.timestamp_ms
            << " " << QString::fromStdString(event.stage)
            << " source=" << QString::fromStdString(event.source_uuid)
            << " participant=" << event.participant_id
            << " " << QString::fromStdString(event.message)
            << "\n";
    }

    if (!write_text_file(bundle.absoluteFilePath("summary.txt"), text)) {
        QMessageBox::warning(this, "Export Diagnostics",
            QString("Could not write summary.txt in:\n%1")
                .arg(bundle.absolutePath()));
        return;
    }

    QMessageBox::information(this, "Export Diagnostics",
        QString("Diagnostics exported to:\n%1").arg(bundle.absolutePath()));
}
