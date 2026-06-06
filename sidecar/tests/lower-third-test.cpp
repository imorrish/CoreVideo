#include "lower-third-controller.h"
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

    Look look;
    look.id = "test";
    look.name = "Test";
    look.tileStyle.showNameTag = true;
    look.tmpl.id = "2-up";
    look.tmpl.name = "2-up";
    look.tmpl.slotList.append({0, 0.0, 0.0, 0.5, 1.0, "Slot 1", false});
    look.tmpl.slotList.append({1, 0.5, 0.0, 0.5, 1.0, "Slot 2", false});
    look.slotAssignments.append({0, 101});

    ParticipantInfo p;
    p.id = 101;
    p.name = "Alex Rivera";
    p.color = QColor("#2979ff");
    p.hasVideo = true;

    LowerThirdController ctl;
    const QVector<Overlay> overlays = ctl.participantSyncedOverlays(look, {p});
    if (overlays.size() != 1) {
        std::cerr << "Expected one lower third, got " << overlays.size() << "\n";
        return 1;
    }
    if (!LowerThirdController::isAutoLowerThird(overlays[0])) {
        std::cerr << "Expected generated lower third id\n";
        return 1;
    }
    if (overlays[0].text1 != "Alex Rivera") {
        std::cerr << "Expected participant name in lower third\n";
        return 1;
    }
    if (!overlays[0].text2.isEmpty())
        return fail("Expected no generated subtitle for idle participant");

    p.isTalking = true;
    const QVector<Overlay> speaking = ctl.participantSyncedOverlays(look, {p});
    if (speaking.size() != 1 || speaking[0].text2 != "Speaking")
        return fail("Expected speaking subtitle for talking participant");

    LowerThirdOverride override;
    override.enabled = true;
    override.name = "  Producer Name  ";
    override.subtitle = "  Executive Producer  ";
    ctl.setOverride(101, override);
    const QVector<Overlay> overridden = ctl.participantSyncedOverlays(look, {p});
    if (overridden.size() != 1 ||
        overridden[0].text1 != "Producer Name" ||
        overridden[0].text2 != "Executive Producer") {
        return fail("Expected trimmed manual lower-third override");
    }

    override.name = "   ";
    ctl.setOverride(101, override);
    if (!ctl.participantSyncedOverlays(look, {p}).isEmpty())
        return fail("Blank enabled override should suppress generated lower third");

    ctl.clearOverride(101);
    if (ctl.participantSyncedOverlays(look, {p}).size() != 1)
        return fail("Clearing override should restore generated lower third");

    look.slotAssignments.clear();
    look.slotAssignments.append({0, -1000});
    ParticipantInfo placeholder;
    placeholder.id = -1000;
    placeholder.name = "Placeholder 1";
    placeholder.color = QColor("#1e6ae0");
    placeholder.hasVideo = true;
    const QVector<Overlay> placeholderOverlays =
        ctl.participantSyncedOverlays(look, {placeholder});
    if (placeholderOverlays.size() != 1 ||
        placeholderOverlays[0].text1 != "Placeholder 1") {
        return fail("Placeholder participants should exercise generated lower thirds");
    }

    look.slotAssignments.clear();
    look.slotAssignments.append({0, 101});
    look.tileStyle.showNameTag = false;
    if (!ctl.participantSyncedOverlays(look, {p}).isEmpty())
        return fail("showNameTag=false should disable generated lower thirds");

    return 0;
}
