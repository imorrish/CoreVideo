#include "zoom-iso-recorder.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QStringList>
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/platform.h>
#include <algorithm>
#include <cstring>

static constexpr qint64 kIsoMinimumFreeBytes = 2ll * 1024ll * 1024ll * 1024ll;
static constexpr qint64 kIsoWarningFreeBytes = 10ll * 1024ll * 1024ll * 1024ll;
static constexpr int kFfmpegOutputTailChars = 2048;

static QString default_iso_dir()
{
    const QString docs = QStandardPaths::writableLocation(
        QStandardPaths::DocumentsLocation);
    const QString base = docs.isEmpty() ? QDir::homePath() : docs;
    return QDir(base).absoluteFilePath("CoreVideo ISOs");
}

static QString sanitized(const std::string &value, const QString &fallback)
{
    QString out = QString::fromStdString(value).trimmed();
    if (out.isEmpty()) out = fallback;
    for (QChar &ch : out) {
        if (!ch.isLetterOrNumber() && ch != '-' && ch != '_' && ch != '.')
            ch = '_';
    }
    while (out.contains("__")) out.replace("__", "_");
    return out.left(80);
}

static const char *assignment_label(AssignmentMode mode)
{
    switch (mode) {
    case AssignmentMode::ActiveSpeaker: return "active_speaker";
    case AssignmentMode::SpotlightIndex: return "spotlight";
    case AssignmentMode::ScreenShare: return "screen_share";
    case AssignmentMode::Participant:
    default: return "participant";
    }
}

static QString bytes_text(qint64 bytes)
{
    const double gb = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
    if (gb >= 1.0)
        return QString("%1 GB").arg(gb, 0, 'f', 1);
    const double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
    return QString("%1 MB").arg(mb, 0, 'f', 1);
}

static std::string normalized_video_encoder(const std::string &encoder);
static bool is_hardware_encoder(const std::string &encoder);
static bool ffmpeg_encoder_available(const QString &ffmpeg_path,
                                     const std::string &encoder,
                                     std::string *error);
static QStringList ffmpeg_video_encoder_args(const std::string &encoder);

ZoomIsoRecorder &ZoomIsoRecorder::instance()
{
    static ZoomIsoRecorder inst;
    return inst;
}

ZoomIsoRecorder::~ZoomIsoRecorder()
{
    stop();
}

bool ZoomIsoRecorder::WavFile::open(const QString &path,
                                    uint32_t rate,
                                    uint16_t channel_count)
{
    close();
#if defined(_WIN32)
    file = _wfopen(reinterpret_cast<const wchar_t *>(path.utf16()), L"wb");
#else
    file = fopen(path.toUtf8().constData(), "wb");
#endif
    if (!file) return false;
    sample_rate = rate;
    channels = std::max<uint16_t>(channel_count, 1);
    data_bytes = 0;

    const uint16_t audio_format = 1;
    const uint16_t bits_per_sample = 16;
    const uint32_t byte_rate = sample_rate * channels * bits_per_sample / 8;
    const uint16_t block_align = channels * bits_per_sample / 8;
    const uint32_t riff_size = 36;
    const uint32_t data_size = 0;

    fwrite("RIFF", 1, 4, file);
    fwrite(&riff_size, 4, 1, file);
    fwrite("WAVEfmt ", 1, 8, file);
    const uint32_t fmt_size = 16;
    fwrite(&fmt_size, 4, 1, file);
    fwrite(&audio_format, 2, 1, file);
    fwrite(&channels, 2, 1, file);
    fwrite(&sample_rate, 4, 1, file);
    fwrite(&byte_rate, 4, 1, file);
    fwrite(&block_align, 2, 1, file);
    fwrite(&bits_per_sample, 2, 1, file);
    fwrite("data", 1, 4, file);
    fwrite(&data_size, 4, 1, file);
    return true;
}

void ZoomIsoRecorder::WavFile::write(const uint8_t *pcm, uint32_t byte_len)
{
    if (!file || !pcm || byte_len == 0) return;
    fwrite(pcm, 1, byte_len, file);
    data_bytes += byte_len;
}

void ZoomIsoRecorder::WavFile::close()
{
    if (!file) return;
    const uint32_t riff_size = 36 + data_bytes;
    fseek(file, 4, SEEK_SET);
    fwrite(&riff_size, 4, 1, file);
    fseek(file, 40, SEEK_SET);
    fwrite(&data_bytes, 4, 1, file);
    fclose(file);
    file = nullptr;
    data_bytes = 0;
}

bool ZoomIsoRecorder::start(const ZoomIsoRecordConfig &config,
                            std::string *error)
{
    ZoomIsoRecordConfig normalized = config;
    if (normalized.output_dir.empty())
        normalized.output_dir = default_iso_dir().toStdString();
    if (normalized.ffmpeg_path.empty())
        normalized.ffmpeg_path = "ffmpeg";
    const std::string requested_encoder =
        normalized_video_encoder(normalized.video_encoder);
    normalized.video_encoder =
        requested_encoder;
    const QString ffmpegProgram = QString::fromStdString(normalized.ffmpeg_path);
    const QFileInfo ffmpegInfo(ffmpegProgram);
    if ((ffmpegInfo.isRelative() &&
         QStandardPaths::findExecutable(ffmpegProgram).isEmpty()) ||
        (!ffmpegInfo.isRelative() && !ffmpegInfo.exists())) {
        if (error) {
            *error = "FFmpeg was not found on PATH. Set ffmpeg_path to a "
                     "valid ffmpeg executable.";
        }
        return false;
    }
    std::string encoder_error;
    QString status_warning;
    if (!ffmpeg_encoder_available(ffmpegProgram, normalized.video_encoder,
                                  &encoder_error)) {
        if (is_hardware_encoder(normalized.video_encoder) &&
            ffmpeg_encoder_available(ffmpegProgram, "libx264", nullptr)) {
            status_warning = QString("Requested hardware encoder '%1' was not "
                                     "available in FFmpeg; falling back to CPU "
                                     "libx264 for this ISO run.")
                                 .arg(QString::fromStdString(normalized.video_encoder));
            blog(LOG_WARNING, "[obs-zoom-plugin] %s",
                 status_warning.toUtf8().constData());
            normalized.video_encoder = "libx264";
        } else {
            if (error)
                *error = encoder_error;
            return false;
        }
    }

    QDir dir(QString::fromStdString(normalized.output_dir));
    if (!dir.exists() && !dir.mkpath(".")) {
        if (error) *error = "Could not create ISO recording directory.";
        return false;
    }
    const QStorageInfo storage(dir.absolutePath());
    if (storage.isValid() && storage.isReady()) {
        const qint64 available = storage.bytesAvailable();
        if (available < kIsoMinimumFreeBytes) {
            if (error) {
                *error = QString("Only %1 is free in the ISO output folder. "
                                 "Free at least %2 before starting ISO recording.")
                             .arg(bytes_text(available),
                                  bytes_text(kIsoMinimumFreeBytes))
                             .toStdString();
            }
            return false;
        }
        if (available < kIsoWarningFreeBytes) {
            blog(LOG_WARNING,
                 "[obs-zoom-plugin] ISO recording starting with low disk space: available=%s dir=%s",
                 bytes_text(available).toUtf8().constData(),
                 normalized.output_dir.c_str());
        }
    } else {
        blog(LOG_WARNING,
             "[obs-zoom-plugin] ISO recording could not validate disk space for dir=%s",
             normalized.output_dir.c_str());
    }

    {
        std::lock_guard<std::mutex> lock(m_mtx);
        for (auto &entry : m_sessions)
            close_session(entry.second);
        m_sessions.clear();
        m_config = normalized;
        m_requested_video_encoder = requested_encoder;
        m_status_warning = status_warning;
        m_started_program_recording = false;
        m_active.store(true, std::memory_order_release);
    }

    if (normalized.record_program && !obs_frontend_recording_active()) {
        obs_frontend_recording_start();
        std::lock_guard<std::mutex> lock(m_mtx);
        m_started_program_recording = true;
    }

    blog(LOG_INFO,
         "[obs-zoom-plugin] ISO recording started: dir=%s ffmpeg=%s encoder=%s",
         normalized.output_dir.c_str(), normalized.ffmpeg_path.c_str(),
         normalized.video_encoder.c_str());
    return true;
}

void ZoomIsoRecorder::stop()
{
    if (!m_active.exchange(false, std::memory_order_acq_rel)) return;
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        for (auto &entry : m_sessions)
            close_session(entry.second);
        m_sessions.clear();
        if (m_started_program_recording && obs_frontend_recording_active())
            obs_frontend_recording_stop();
        m_started_program_recording = false;
    }
    blog(LOG_INFO, "[obs-zoom-plugin] ISO recording stopped");
}

QJsonArray ZoomIsoRecorder::status_json()
{
    QJsonArray arr;
    std::lock_guard<std::mutex> lock(m_mtx);
    for (auto &entry : m_sessions) {
        Session &s = entry.second;
        refresh_ffmpeg_status_locked(s);
        QJsonObject obj;
        obj["source_uuid"] = QString::fromStdString(s.source_uuid);
        obj["source"] = QString::fromStdString(s.source_name);
        obj["display_name"] = QString::fromStdString(s.display_name);
        obj["assignment"] = assignment_label(s.assignment);
        obj["configured_participant_id"] =
            static_cast<double>(s.configured_participant_id);
        obj["resolved_participant_id"] =
            static_cast<double>(s.resolved_participant_id);
        obj["width"] = static_cast<int>(s.width);
        obj["height"] = static_cast<int>(s.height);
        obj["video_frames"] = static_cast<int>(s.video_frames);
        obj["audio_chunks"] = static_cast<int>(s.audio_chunks);
        const uint64_t now_ns = os_gettime_ns();
        obj["elapsed_ms"] = s.started_ns > 0 && now_ns >= s.started_ns
            ? static_cast<double>((now_ns - s.started_ns) / 1000000ULL)
            : 0.0;
        obj["last_video_age_ms"] = s.last_video_ns > 0 && now_ns >= s.last_video_ns
            ? static_cast<double>((now_ns - s.last_video_ns) / 1000000ULL)
            : -1.0;
        obj["last_audio_age_ms"] = s.last_audio_ns > 0 && now_ns >= s.last_audio_ns
            ? static_cast<double>((now_ns - s.last_audio_ns) / 1000000ULL)
            : -1.0;
        obj["ffmpeg_running"] =
            s.ffmpeg && s.ffmpeg->state() == QProcess::Running;
        obj["ffmpeg_error"] = s.ffmpeg_error;
        obj["ffmpeg_exit_code"] = s.ffmpeg_exit_code;
        obj["ffmpeg_exit_status"] = s.ffmpeg_exit_status;
        obj["ffmpeg_output_tail"] = s.ffmpeg_output_tail;
        obj["requested_video_encoder"] =
            QString::fromStdString(s.requested_video_encoder);
        obj["video_encoder"] = QString::fromStdString(s.video_encoder);
        obj["encoder_fallback"] = s.encoder_fallback;
        obj["video_bytes"] = QFileInfo(s.video_path).exists()
            ? static_cast<double>(QFileInfo(s.video_path).size())
            : 0.0;
        obj["audio_bytes"] = QFileInfo(s.audio_path).exists()
            ? static_cast<double>(QFileInfo(s.audio_path).size())
            : 0.0;
        obj["video_path"] = s.video_path;
        obj["audio_path"] = s.audio_path;
        arr.append(obj);
    }
    return arr;
}

QJsonObject ZoomIsoRecorder::status_overview()
{
    QJsonObject obj;
    std::lock_guard<std::mutex> lock(m_mtx);
    obj["active"] = m_active.load(std::memory_order_acquire);
    obj["output_dir"] = QString::fromStdString(m_config.output_dir);
    obj["ffmpeg_path"] = QString::fromStdString(m_config.ffmpeg_path);
    obj["requested_video_encoder"] =
        QString::fromStdString(m_requested_video_encoder.empty()
            ? m_config.video_encoder
            : m_requested_video_encoder);
    obj["video_encoder"] = QString::fromStdString(m_config.video_encoder);
    obj["encoder_fallback"] =
        !m_requested_video_encoder.empty() &&
        m_requested_video_encoder != m_config.video_encoder;
    obj["hardware_encoder"] = is_hardware_encoder(m_config.video_encoder);
    obj["record_program"] = m_config.record_program;
    obj["program_recording_started_by_corevideo"] = m_started_program_recording;
    obj["session_count"] = static_cast<int>(m_sessions.size());
    obj["warning"] = m_status_warning;

    const QDir dir(QString::fromStdString(m_config.output_dir));
    const QStorageInfo storage(dir.absolutePath());
    if (storage.isValid() && storage.isReady()) {
        obj["disk_available_bytes"] = static_cast<double>(storage.bytesAvailable());
        obj["disk_warning"] = storage.bytesAvailable() < kIsoWarningFreeBytes;
    } else {
        obj["disk_available_bytes"] = -1.0;
        obj["disk_warning"] = true;
    }
    return obj;
}

void ZoomIsoRecorder::on_output_updated(const ZoomOutputInfo &info)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    if (info.source_uuid.empty()) return;
    m_outputs[info.source_uuid] = info;
    if (!should_record(info, info.participant_id))
        close_session_locked(info.source_uuid);
}

void ZoomIsoRecorder::on_output_removed(const std::string &source_uuid)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    close_session_locked(source_uuid);
    m_outputs.erase(source_uuid);
}

bool ZoomIsoRecorder::should_record(const ZoomOutputInfo &info,
                                    uint32_t resolved_participant_id) const
{
    if (!m_active.load(std::memory_order_acquire)) return false;
    if (info.source_uuid.empty()) return false;
    if (info.assignment == AssignmentMode::ScreenShare) return false;
    if (resolved_participant_id == 0 &&
        info.assignment != AssignmentMode::ActiveSpeaker &&
        info.assignment != AssignmentMode::SpotlightIndex)
        return false;
    return true;
}

ZoomIsoRecorder::Session &
ZoomIsoRecorder::ensure_session_locked(const ZoomOutputInfo &info,
                                       uint32_t resolved_participant_id,
                                       uint32_t width,
                                       uint32_t height,
                                       uint64_t timestamp_ns)
{
    auto it = m_sessions.find(info.source_uuid);
    const bool needs_new = it == m_sessions.end() ||
        it->second.resolved_participant_id != resolved_participant_id ||
        it->second.width != width ||
        it->second.height != height;
    if (!needs_new) return it->second;

    if (it != m_sessions.end()) {
        close_session(it->second);
        m_sessions.erase(it);
    }

    Session session;
    session.source_uuid = info.source_uuid;
    session.source_name = info.source_name;
    session.display_name = info.display_name.empty()
        ? info.source_name : info.display_name;
    session.assignment = info.assignment;
    session.configured_participant_id = info.participant_id;
    session.resolved_participant_id = resolved_participant_id;
    session.width = width;
    session.height = height;
    session.started_ns = timestamp_ns;

    QDir root(QString::fromStdString(m_config.output_dir));
    const QString stamp = QDateTime::currentDateTimeUtc()
        .toString("yyyyMMdd-HHmmss-zzz");
    const QString name = sanitized(session.display_name,
        QStringLiteral("source"));
    const QString participant =
        resolved_participant_id == 0
            ? QStringLiteral("unresolved")
            : QString::number(resolved_participant_id);
    const QString base = QString("%1_%2_%3_%4")
        .arg(stamp, name, QString::fromLatin1(assignment_label(info.assignment)),
             participant);
    session.base_path = root.absoluteFilePath(base);
    session.video_path = session.base_path + ".mp4";
    session.audio_path = session.base_path + ".wav";
    session.requested_video_encoder = m_requested_video_encoder.empty()
        ? normalized_video_encoder(m_config.video_encoder)
        : m_requested_video_encoder;
    session.video_encoder = normalized_video_encoder(m_config.video_encoder);
    session.encoder_fallback =
        session.requested_video_encoder != session.video_encoder;

    session.ffmpeg = std::make_unique<QProcess>();
    session.ffmpeg->setProgram(QString::fromStdString(m_config.ffmpeg_path));
    QStringList args = {
        "-hide_banner", "-loglevel", "warning", "-y",
        "-f", "rawvideo",
        "-pix_fmt", "yuv420p",
        "-s", QString("%1x%2").arg(width).arg(height),
        "-r", "30",
        "-i", "pipe:0",
        "-an",
    };
    args.append(ffmpeg_video_encoder_args(session.video_encoder));
    args.append({
        "-movflags", "+faststart",
        session.video_path,
    });
    session.ffmpeg->setArguments(args);
    session.ffmpeg->setProcessChannelMode(QProcess::MergedChannels);
    session.ffmpeg->start(QIODevice::ReadWrite);
    if (!session.ffmpeg->waitForStarted(2000)) {
        session.ffmpeg_error = session.ffmpeg->errorString();
        session.ffmpeg_error_logged = true;
        blog(LOG_WARNING,
             "[obs-zoom-plugin] ISO ffmpeg failed to start for %s: %s",
             session.source_name.c_str(),
             session.ffmpeg->errorString().toUtf8().constData());
    }

    blog(LOG_INFO,
         "[obs-zoom-plugin] ISO session started: source=%s participant=%u encoder=%s video=%s audio=%s",
         session.source_name.c_str(), session.resolved_participant_id,
         session.video_encoder.c_str(),
         session.video_path.toUtf8().constData(),
         session.audio_path.toUtf8().constData());

    auto inserted = m_sessions.emplace(info.source_uuid, std::move(session));
    return inserted.first->second;
}

void ZoomIsoRecorder::close_session_locked(const std::string &source_uuid)
{
    auto it = m_sessions.find(source_uuid);
    if (it == m_sessions.end()) return;
    close_session(it->second);
    m_sessions.erase(it);
}

void ZoomIsoRecorder::close_session(Session &session)
{
    session.wav.close();
    if (session.ffmpeg) {
        refresh_ffmpeg_status_locked(session);
        session.ffmpeg->closeWriteChannel();
        if (!session.ffmpeg->waitForFinished(5000))
            session.ffmpeg->kill();
        if (session.ffmpeg->state() != QProcess::NotRunning)
            session.ffmpeg->waitForFinished(2000);
        refresh_ffmpeg_status_locked(session);
    }
    blog(LOG_INFO,
         "[obs-zoom-plugin] ISO session closed: source=%s participant=%u frames=%u audio_chunks=%u",
         session.source_name.c_str(), session.resolved_participant_id,
         session.video_frames, session.audio_chunks);
}

void ZoomIsoRecorder::refresh_ffmpeg_status_locked(Session &session)
{
    if (!session.ffmpeg)
        return;

    const QByteArray output = session.ffmpeg->readAll();
    if (!output.isEmpty()) {
        session.ffmpeg_output_tail += QString::fromUtf8(output).trimmed();
        if (session.ffmpeg_output_tail.size() > kFfmpegOutputTailChars)
            session.ffmpeg_output_tail =
                session.ffmpeg_output_tail.right(kFfmpegOutputTailChars);
    }

    if (session.ffmpeg->state() != QProcess::NotRunning)
        return;

    session.ffmpeg_exit_code = session.ffmpeg->exitCode();
    session.ffmpeg_exit_status =
        session.ffmpeg->exitStatus() == QProcess::CrashExit
            ? QStringLiteral("crashed")
            : QStringLiteral("normal");

    if (!session.ffmpeg_error.isEmpty())
        return;

    if (session.ffmpeg->exitStatus() == QProcess::CrashExit) {
        mark_ffmpeg_failure_locked(session, QStringLiteral("FFmpeg crashed."));
    } else if (session.ffmpeg->exitCode() != 0) {
        mark_ffmpeg_failure_locked(
            session,
            QString("FFmpeg exited with code %1.")
                .arg(session.ffmpeg->exitCode()));
    }
}

void ZoomIsoRecorder::mark_ffmpeg_failure_locked(Session &session,
                                                const QString &message)
{
    if (session.ffmpeg_error.isEmpty())
        session.ffmpeg_error = message;
    if (!session.ffmpeg || session.ffmpeg_error_logged)
        return;

    blog(LOG_WARNING,
         "[obs-zoom-plugin] ISO ffmpeg failure for %s: %s%s%s",
         session.source_name.c_str(),
         session.ffmpeg_error.toUtf8().constData(),
         session.ffmpeg_output_tail.isEmpty() ? "" : " output=",
         session.ffmpeg_output_tail.toUtf8().constData());
    session.ffmpeg_error_logged = true;
}

bool ZoomIsoRecorder::write_ffmpeg_locked(Session &session,
                                         const uint8_t *data,
                                         uint32_t byte_len)
{
    if (!session.ffmpeg || !data || byte_len == 0)
        return false;

    const char *cursor = reinterpret_cast<const char *>(data);
    qint64 remaining = byte_len;
    while (remaining > 0) {
        const qint64 written = session.ffmpeg->write(cursor, remaining);
        if (written <= 0) {
            mark_ffmpeg_failure_locked(
                session,
                QString("FFmpeg pipe write failed: %1")
                    .arg(session.ffmpeg->errorString()));
            refresh_ffmpeg_status_locked(session);
            return false;
        }
        cursor += written;
        remaining -= written;
    }

    return true;
}

void ZoomIsoRecorder::record_video_frame(const ZoomOutputInfo &info,
                                         uint32_t resolved_participant_id,
                                         uint32_t width,
                                         uint32_t height,
                                         const uint8_t *y,
                                         const uint8_t *u,
                                         const uint8_t *v,
                                         uint32_t stride_y,
                                         uint32_t stride_uv,
                                         uint64_t timestamp_ns)
{
    if (!y || !u || !v || width == 0 || height == 0) return;
    std::lock_guard<std::mutex> lock(m_mtx);
    if (!should_record(info, resolved_participant_id)) return;
    Session &session = ensure_session_locked(info, resolved_participant_id,
                                             width, height, timestamp_ns);
    refresh_ffmpeg_status_locked(session);
    if (!session.ffmpeg ||
        session.ffmpeg->state() != QProcess::Running) {
        if (session.ffmpeg_error.isEmpty())
            mark_ffmpeg_failure_locked(
                session, QStringLiteral("FFmpeg is not running."));
        return;
    }

    for (uint32_t row = 0; row < height; ++row) {
        if (!write_ffmpeg_locked(session, y + row * stride_y, width))
            return;
    }
    for (uint32_t row = 0; row < height / 2; ++row) {
        if (!write_ffmpeg_locked(session, u + row * stride_uv, width / 2))
            return;
    }
    for (uint32_t row = 0; row < height / 2; ++row) {
        if (!write_ffmpeg_locked(session, v + row * stride_uv, width / 2))
            return;
    }
    ++session.video_frames;
    session.last_video_ns = timestamp_ns;
}

void ZoomIsoRecorder::record_audio_frame(const ZoomOutputInfo &info,
                                         uint32_t resolved_participant_id,
                                         const uint8_t *pcm,
                                         uint32_t byte_len,
                                         uint32_t sample_rate,
                                         uint16_t channels,
                                         uint64_t timestamp_ns)
{
    if (!pcm || byte_len == 0 || sample_rate == 0) return;
    std::lock_guard<std::mutex> lock(m_mtx);
    if (!should_record(info, resolved_participant_id)) return;
    auto it = m_sessions.find(info.source_uuid);
    if (it == m_sessions.end() ||
        it->second.resolved_participant_id != resolved_participant_id) {
        return;
    }
    Session &session = it->second;
    if (!session.wav.file &&
        !session.wav.open(session.audio_path, sample_rate, channels)) {
        blog(LOG_WARNING,
             "[obs-zoom-plugin] ISO WAV open failed: %s",
             session.audio_path.toUtf8().constData());
        return;
    }
    if (session.wav.sample_rate == sample_rate &&
        session.wav.channels == std::max<uint16_t>(channels, 1)) {
        session.wav.write(pcm, byte_len);
        ++session.audio_chunks;
        session.last_audio_ns = timestamp_ns;
    }
}

static std::string normalized_video_encoder(const std::string &encoder)
{
    if (encoder == "h264_nvenc" || encoder == "h264_qsv" ||
        encoder == "h264_amf" || encoder == "libx264") {
        return encoder;
    }
    return "libx264";
}

static bool is_hardware_encoder(const std::string &encoder)
{
    return encoder == "h264_nvenc" || encoder == "h264_qsv" ||
        encoder == "h264_amf";
}

static bool ffmpeg_encoder_available(const QString &ffmpeg_path,
                                     const std::string &encoder,
                                     std::string *error)
{
    QProcess probe;
    probe.setProgram(ffmpeg_path);
    probe.setArguments({"-hide_banner", "-encoders"});
    probe.setProcessChannelMode(QProcess::MergedChannels);
    probe.start(QIODevice::ReadOnly);
    if (!probe.waitForStarted(2000)) {
        if (error) {
            *error = "FFmpeg failed to start while checking encoders: " +
                     probe.errorString().toStdString();
        }
        return false;
    }
    if (!probe.waitForFinished(5000))
        probe.kill();

    const QString encoders = QString::fromUtf8(probe.readAll());
    if (encoders.contains(QString::fromStdString(encoder)))
        return true;

    if (error) {
        *error = "FFmpeg encoder '" + encoder +
                 "' is not available in the selected ffmpeg build.";
    }
    return false;
}

static QStringList ffmpeg_video_encoder_args(const std::string &encoder)
{
    if (encoder == "libx264") {
        return {
            "-c:v", "libx264",
            "-preset", "veryfast",
            "-crf", "18",
        };
    }

    return {
        "-c:v", QString::fromStdString(encoder),
        "-b:v", "12M",
        "-maxrate", "20M",
        "-bufsize", "24M",
    };
}
