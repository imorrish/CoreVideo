#include "zoom-diagnostics-dialog.h"
#include "cv-style.h"
#include "zoom-engine-client.h"
#include "zoom-output-manager.h"
#include <QAbstractItemView>
#include <QColor>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QStringList>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <unordered_map>

enum OutputDiagnosticColumns {
    DiagOutput = 0,
    DiagParticipant,
    DiagAssignment,
    DiagRequested,
    DiagObserved,
    DiagFps,
    DiagAge,
    DiagRetries,
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
    if (output.observed_width == 0 || output.observed_height == 0)
        return QStringLiteral("Waiting");
    if (output.video_stale)
        return QStringLiteral("Stale");
    if (output_signal_below_requested(output))
        return QStringLiteral("Below requested");
    return QStringLiteral("OK");
}

static QTableWidgetItem *readonly_item(const QString &text)
{
    auto *item = new QTableWidgetItem(text);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
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
        "Output", "Participant", "Assignment", "Requested", "Observed",
        "FPS", "Frame Age", "Retries", "State"
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
    connect(refresh_button, &QPushButton::clicked, this, [this]() { refresh(); });
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
        m_outputs->setItem(row, DiagObserved, readonly_item(observed_text(output)));
        m_outputs->setItem(row, DiagFps, readonly_item(QString::number(output.observed_fps, 'f', 1)));
        m_outputs->setItem(row, DiagAge, readonly_item(output.last_frame_age_ms == 0
            ? QStringLiteral("-") : QString("%1 ms").arg(output.last_frame_age_ms)));
        m_outputs->setItem(row, DiagRetries, readonly_item(retry_text(output)));
        auto *state_item = readonly_item(signal_state_text(output));
        if (output.video_stale)
            state_item->setForeground(Qt::red);
        else if (output_signal_below_requested(output))
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
