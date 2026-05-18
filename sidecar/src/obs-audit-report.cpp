#include "obs-audit-report.h"

QString obsAuditSummaryText(const OBSClient::CoreVideoSceneAudit &audit)
{
    return QStringLiteral(
        "OBS sync is %1. Inventory: %2. Scenes %3/%4, inputs %5/%6, scene items %7/%8.")
        .arg(audit.isClean() ? QStringLiteral("clean") : QStringLiteral("needs attention"))
        .arg(audit.inventoryReady ? QStringLiteral("ready") : QStringLiteral("loading"))
        .arg(audit.presentScenes).arg(audit.expectedScenes)
        .arg(audit.presentInputs).arg(audit.expectedInputs)
        .arg(audit.presentSceneItems).arg(audit.expectedSceneItems);
}

QStringList obsAuditActionDetails(const OBSClient::CoreVideoSceneAudit &audit,
                                  int maxItems)
{
    QStringList actions;
    auto append = [&](const QString &prefix, const QStringList &items) {
        for (const QString &item : items) {
            if (actions.size() >= maxItems)
                return;
            actions << QStringLiteral("%1: %2").arg(prefix, item);
        }
    };
    append(QStringLiteral("Geometry"), audit.geometryDrift);
    append(QStringLiteral("Missing item"), audit.missingSceneItems);
    append(QStringLiteral("Missing scene"), audit.missingScenes);
    append(QStringLiteral("Missing input"), audit.missingInputs);
    append(QStringLiteral("Stale layer"), audit.staleDesignLayers);
    return actions;
}

QString obsAuditReportText(const OBSClient::CoreVideoSceneAudit &audit,
                           int maxItems)
{
    QStringList lines;
    lines << obsAuditSummaryText(audit);
    const QStringList actions = obsAuditActionDetails(audit, maxItems);
    if (actions.isEmpty()) {
        lines << QStringLiteral("No OBS scene graph drift found.");
    } else {
        lines << QStringLiteral("Actions:");
        for (const QString &action : actions)
            lines << QStringLiteral("- %1").arg(action);
    }
    return lines.join(QStringLiteral("\n"));
}
