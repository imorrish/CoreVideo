#include "obs-audit-report.h"

#include <QCoreApplication>
#include <iostream>

static int fail(const char *message)
{
    std::cerr << message << "\n";
    return 1;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    OBSClient::CoreVideoSceneAudit clean;
    clean.inventoryReady = true;
    clean.expectedScenes = clean.presentScenes = 2;
    clean.expectedInputs = clean.presentInputs = 3;
    clean.expectedSceneItems = clean.presentSceneItems = 4;
    const QString cleanReport = obsAuditReportText(clean);
    if (!cleanReport.contains("OBS sync is clean"))
        return fail("clean audit should report clean state");
    if (!cleanReport.contains("No OBS scene graph drift found."))
        return fail("clean audit should report no drift");

    OBSClient::CoreVideoSceneAudit dirty;
    dirty.inventoryReady = true;
    dirty.expectedScenes = 2;
    dirty.presentScenes = 1;
    dirty.expectedInputs = 2;
    dirty.presentInputs = 1;
    dirty.expectedSceneItems = 2;
    dirty.presentSceneItems = 0;
    dirty.missingScenes << "CoreVideo - Test";
    dirty.missingInputs << "Zoom Participant 1";
    dirty.missingSceneItems << "CoreVideo - Test -> CoreVideo Slot 1";
    dirty.geometryDrift << "CoreVideo - Test -> CoreVideo Slot 1: position";
    const QString dirtyReport = obsAuditReportText(dirty, 3);
    if (!dirtyReport.contains("needs attention"))
        return fail("dirty audit should report attention state");
    if (!dirtyReport.contains("Geometry: CoreVideo - Test -> CoreVideo Slot 1: position"))
        return fail("dirty audit should include geometry action");
    if (!dirtyReport.contains("Missing item: CoreVideo - Test -> CoreVideo Slot 1"))
        return fail("dirty audit should include missing item action");
    if (dirtyReport.contains("Missing input: Zoom Participant 1"))
        return fail("dirty audit should honor max action count");

    return 0;
}
