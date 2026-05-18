#pragma once

#include "obs-client.h"
#include "obs-look-renderer.h"
#include <QObject>
#include <QVector>

class QCoreApplication;

class OBSLiveValidator : public QObject {
    Q_OBJECT
public:
    struct Config {
        OBSClient::Config obs;
        OBSLookRenderer::Config renderer;
        bool repair = false;
        int timeoutMs = 12000;
        QString reportPath;
    };

    explicit OBSLiveValidator(Config config, QObject *parent = nullptr);

public slots:
    void start();

private:
    void onConnected();
    void requestAuditInventory();
    void finish();
    QVector<LookRenderPlan> renderPlans() const;

    OBSClient m_client;
    OBSClient::Config m_obsConfig;
    OBSLookRenderer::Config m_rendererConfig;
    bool m_repair = false;
    int m_timeoutMs = 12000;
    QString m_reportPath;
};
