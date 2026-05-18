#pragma once
#include "audio-routing.h"
#include "layout-template.h"
#include "look.h"
#include "overlay.h"
#include <QHash>
#include <QWidget>
#include <QVector>
#include <QColor>
#include <QPoint>

// Renders a broadcast preview panel — draws participant slots according to
// the active LayoutTemplate using mock or live thumbnail data.
// Accepts drops from participant cards (MIME: "application/x-cv-participant").
class PreviewCanvas : public QWidget {
    Q_OBJECT
public:
    struct Participant {
        QString name;
        QString initials;
        QColor  avatarColor = QColor(60, 80, 200);
        bool    isTalking   = false;
        bool    hasVideo    = false;
    };

    explicit PreviewCanvas(QWidget *parent = nullptr);

    void setTemplate(const LayoutTemplate &tmpl);
    void setParticipants(const QVector<Participant> &participants);
    void setOverlays(const QVector<Overlay> &overlays);
    void setBackgroundImage(const QString &path);
    void setTileStyle(const TileStyle &style);
    // Accent color used when an overlay doesn't specify one — usually the
    // active theme accent. Defaults to a neutral blue.
    void setAccent(const QColor &c);
    // 1.0 = fully visible, 0.0 = black. Used by MEBus for AUTO / FTB.
    void setOpacity(float opacity);
    // Per-slot audio routing badge. Slots missing from the map render as
    // Mixed. Only the PVW/PGM canvas in MainWindow shows these — the badge
    // is purely an operator affordance and does not affect rendering.
    void setSlotRouting(const QHash<int, AudioRouting> &routing);
    void setLayoutEditingEnabled(bool enabled);
    void setSelectedSlot(int slotIndex);

signals:
    void slotAssigned(int slotIndex, int participantId);
    void slotClicked(int slotIndex);
    void slotGeometryChanged(const LayoutTemplate &tmpl, int slotIndex);
    // Right-click on a slot — used to cycle the slot's audio routing.
    void slotRoutingCycleRequested(int slotIndex);

protected:
    void paintEvent(QPaintEvent *) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dragMoveEvent(QDragMoveEvent *e) override;
    void dragLeaveEvent(QDragLeaveEvent *e) override;
    void dropEvent(QDropEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;

private:
    enum class EditGesture { None, Move, Resize };

    LayoutTemplate       m_tmpl;
    QVector<Participant> m_parts;
    QVector<Overlay>     m_overlays;
    QString              m_backgroundImagePath;
    QPixmap              m_backgroundImage;
    TileStyle            m_tileStyle;
    QColor               m_accent = QColor(0x29, 0x79, 0xff);
    float                m_opacity = 1.0f;
    QHash<int, AudioRouting> m_slotRouting;
    int                  m_hoveredSlot = -1;
    int                  m_pressedSlot = -1;
    int                  m_selectedSlot = -1;
    bool                 m_layoutEditing = false;
    EditGesture          m_editGesture = EditGesture::None;
    QPoint               m_pressPos;
    TemplateSlot         m_startSlot;

    int    slotAtPoint(QPoint pt) const;
    int    slotPositionForIndex(int slotIndex) const;
    QRectF resizeHandleRect(const QRectF &slot) const;
    bool   pointInResizeHandle(QPoint pt, int slotIndex) const;
    void   drawSlot(QPainter &p, const QRectF &rect, int index) const;
    void   drawAvatar(QPainter &p, QPointF center, float r,
                      const Participant &part) const;
    QRectF canvasRect() const;
    QRectF slotRect(const TemplateSlot &s) const;
    void   drawOverlay(QPainter &p, const Overlay &ov, const QRectF &canvas) const;
};
