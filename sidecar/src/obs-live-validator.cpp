#include "obs-live-validator.h"

#include "look-library.h"
#include "obs-audit-report.h"
#include "template-manager.h"

#include <QCoreApplication>
#include <QFile>
#include <QTextStream>
#include <QTimer>
#include <utility>

OBSLiveValidator::OBSLiveValidator(Config config, QObject *parent)
    : QObject(parent),
      m_obsConfig(std::move(config.obs)),
      m_rendererConfig(std::move(config.renderer)),
      m_repair(config.repair),
      m_timeoutMs(config.timeoutMs),
      m_reportPath(std::move(config.reportPath))
{
    m_rendererConfig.normalizeBroadcastCanvas();
    connect(&m_client, &OBSClient::connected, this, &OBSLiveValidator::onConnected);
    connect(&m_client, &OBSClient::errorOccurred, this, [](const QString &message) {
        QTextStream(stderr) << "OBS error: " << message << "\n";
    });
    connect(&m_client, &OBSClient::requestFailed, this, [](const QString &message) {
        QTextStream(stderr) << "OBS request failed: " << message << "\n";
    });
}

void OBSLiveValidator::start()
{
    m_client.connectToOBS(m_obsConfig);
    QTimer::singleShot(m_timeoutMs, this, [this]() {
        QTextStream(stderr) << "OBS validation timed out.\n";
        QCoreApplication::exit(2);
    });
}

void OBSLiveValidator::onConnected()
{
    TemplateManager::instance().loadBuiltIn();
    LookLibrary::instance().loadBuiltIn();

    OBSLookRenderer renderer(&m_client, m_rendererConfig);
    if (m_repair) {
        renderer.provisionPlaceholders(8);
        renderer.provisionLooks(LookLibrary::instance().looks());
        QTimer::singleShot(4600, this, [this]() {
            m_client.hideStaleCoreVideoDesignLayers(renderPlans());
            requestAuditInventory();
        });
        QTimer::singleShot(7600, this, &OBSLiveValidator::finish);
        return;
    }

    requestAuditInventory();
    QTimer::singleShot(2400, this, &OBSLiveValidator::finish);
}

QVector<LookRenderPlan> OBSLiveValidator::renderPlans() const
{
    QVector<LookRenderPlan> plans;
    const OBSLookRenderer renderer(const_cast<OBSClient *>(&m_client), m_rendererConfig);
    for (const Look &look : LookLibrary::instance().looks()) {
        const LookRenderPlan plan = renderer.renderPlanForLook(look, false);
        if (plan.valid)
            plans.append(plan);
    }
    return plans;
}

void OBSLiveValidator::requestAuditInventory()
{
    m_client.refreshInventory();
    m_client.requestSceneItems(QStringLiteral("CoreVideo Sources"));
    m_client.requestSceneItems(QStringLiteral("CoreVideo Screen Share"));
    for (int i = 0; i < 8; ++i)
        m_client.requestSceneItems(QStringLiteral("CoreVideo Slot %1").arg(i + 1));
    for (const LookRenderPlan &plan : renderPlans())
        m_client.requestSceneItems(plan.sceneName);
}

void OBSLiveValidator::finish()
{
    requestAuditInventory();
    QTimer::singleShot(800, this, [this]() {
        const OBSLookRenderer renderer(&m_client, m_rendererConfig);
        const auto audit = m_client.coreVideoSceneAudit(
            renderer.sourceNamesForSlots(8),
            renderPlans());
        const QString report = obsAuditReportText(audit, 20) + QStringLiteral("\n");
        if (!m_reportPath.trimmed().isEmpty()) {
            QFile file(m_reportPath);
            if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                QTextStream(&file) << report;
            } else {
                QTextStream(stderr) << "Could not write OBS validation report: "
                                    << m_reportPath << "\n";
            }
        }
        QTextStream(stdout) << report;
        QCoreApplication::exit(audit.isClean() ? 0 : 1);
    });
}
