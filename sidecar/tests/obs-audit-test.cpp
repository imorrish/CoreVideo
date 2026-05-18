#include "look-render-plan.h"
#include "obs-client.h"

#include <QCoreApplication>
#include <iostream>

static int fail(const char *message)
{
    std::cerr << message << "\n";
    return 1;
}

static Look makeLook()
{
    Look look;
    look.name = "Audit Test";
    look.templateId = "single";
    look.tmpl.id = "single";
    look.tmpl.name = "Single";
    look.tmpl.slotList.append({0, 0.125, 0.25, 0.5, 0.5, "Speaker", false});
    look.tileStyle.borderWidth = 0.0;
    look.tileStyle.showNameTag = false;
    look.tileStyle.dropShadow = false;
    look.tileStyle.opacity = 1.0;
    return look;
}

static OBSClient::SceneItem item(int id,
                                 const QString &source,
                                 double x = 0.0,
                                 double y = 0.0,
                                 double w = 0.0,
                                 double h = 0.0,
                                 const QString &boundsType = {},
                                 int index = -1,
                                 bool enabled = true)
{
    OBSClient::SceneItem sceneItem;
    sceneItem.sceneItemId = id;
    sceneItem.sourceName = source;
    sceneItem.enabled = enabled;
    sceneItem.sceneItemIndex = index;
    sceneItem.positionX = x;
    sceneItem.positionY = y;
    sceneItem.boundsWidth = w;
    sceneItem.boundsHeight = h;
    sceneItem.boundsType = boundsType;
    return sceneItem;
}

static OBSClient::CoreVideoSceneAudit auditWithSlotItem(const OBSClient::SceneItem &slotItem)
{
    LookRenderConfig cfg;
    cfg.sourcePattern = "Zoom Participant %1";
    cfg.fallbackSceneName = "CoreVideo Main";

    const Look look = makeLook();
    const LookRenderPlan plan = renderPlanForLook(cfg, look, false);
    const QString source = "Zoom Participant 1";
    const QString placeholder = "CoreVideo Placeholder Slot 1";
    const QString canvas = plan.designLayerNames.value(0);

    QHash<QString, QVector<OBSClient::SceneItem>> sceneItems;
    sceneItems.insert("CoreVideo Sources", {item(1, source)});
    sceneItems.insert("CoreVideo Slot 1", {item(2, placeholder), item(3, source)});
    sceneItems.insert(plan.sceneName, {item(4, canvas), slotItem});

    OBSClient client;
    client.seedAuditCacheForTesting(
        QSet<QString>{"CoreVideo Sources", "CoreVideo Slot 1", plan.sceneName},
        QSet<QString>{source, placeholder, canvas},
        sceneItems);

    return client.coreVideoSceneAudit({source}, {plan});
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const auto clean = auditWithSlotItem(item(10,
                                             "CoreVideo Slot 1",
                                             240.0,
                                             270.0,
                                             960.0,
                                             540.0,
                                             "OBS_BOUNDS_SCALE_INNER",
                                             20));
    if (!clean.isClean() || !clean.geometryDrift.isEmpty())
        return fail("matching OBS geometry should audit clean");

    const auto wrongPosition = auditWithSlotItem(item(10,
                                                     "CoreVideo Slot 1",
                                                     300.0,
                                                     270.0,
                                                     960.0,
                                                     540.0,
                                                     "OBS_BOUNDS_SCALE_INNER",
                                                     20));
    if (wrongPosition.geometryDrift.isEmpty()
        || !wrongPosition.geometryDrift.first().contains("position")) {
        return fail("wrong position should report geometry drift");
    }

    const auto wrongSize = auditWithSlotItem(item(10,
                                                 "CoreVideo Slot 1",
                                                 240.0,
                                                 270.0,
                                                 900.0,
                                                 540.0,
                                                 "OBS_BOUNDS_SCALE_INNER",
                                                 20));
    if (wrongSize.geometryDrift.isEmpty()
        || !wrongSize.geometryDrift.first().contains("size")) {
        return fail("wrong bounds size should report geometry drift");
    }

    const auto wrongBounds = auditWithSlotItem(item(10,
                                                   "CoreVideo Slot 1",
                                                   240.0,
                                                   270.0,
                                                   960.0,
                                                   540.0,
                                                   "OBS_BOUNDS_STRETCH",
                                                   20));
    if (wrongBounds.geometryDrift.isEmpty()
        || !wrongBounds.geometryDrift.first().contains("bounds")) {
        return fail("wrong bounds type should report geometry drift");
    }

    const auto wrongLayer = auditWithSlotItem(item(10,
                                                  "CoreVideo Slot 1",
                                                  240.0,
                                                  270.0,
                                                  960.0,
                                                  540.0,
                                                  "OBS_BOUNDS_SCALE_INNER",
                                                  21));
    if (wrongLayer.geometryDrift.isEmpty()
        || !wrongLayer.geometryDrift.first().contains("layer")) {
        return fail("wrong layer index should report geometry drift");
    }

    const auto disabled = auditWithSlotItem(item(10,
                                                "CoreVideo Slot 1",
                                                240.0,
                                                270.0,
                                                960.0,
                                                540.0,
                                                "OBS_BOUNDS_SCALE_INNER",
                                                20,
                                                false));
    if (disabled.geometryDrift.isEmpty()
        || !disabled.geometryDrift.first().contains("visibility")) {
        return fail("disabled expected slot should report geometry drift");
    }

    return 0;
}
