#pragma once

#include <QDialog>

class QLabel;
class QTableWidget;
class QTimer;

class ZoomDiagnosticsDialog : public QDialog {
public:
    explicit ZoomDiagnosticsDialog(QWidget *parent = nullptr);

private:
    void refresh();

    QLabel *m_summary = nullptr;
    QTableWidget *m_outputs = nullptr;
    QTableWidget *m_events = nullptr;
    QTimer *m_timer = nullptr;
};
