#pragma once

#include "obs-client.h"
#include <QString>
#include <QStringList>

QString obsAuditSummaryText(const OBSClient::CoreVideoSceneAudit &audit);
QStringList obsAuditActionDetails(const OBSClient::CoreVideoSceneAudit &audit,
                                  int maxItems = 8);
QString obsAuditReportText(const OBSClient::CoreVideoSceneAudit &audit,
                           int maxItems = 12);
