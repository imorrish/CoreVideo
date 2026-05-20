#pragma once

#include <QWidget>

class QLabel;
class QTableWidget;
class QTimer;

class ZoomDiagnosticsDialog : public QWidget {
public:
    explicit ZoomDiagnosticsDialog(QWidget *parent = nullptr);

private:
    void refresh();
    void export_diagnostics();

    QLabel *m_summary = nullptr;
    QTableWidget *m_outputs = nullptr;
    QTableWidget *m_events = nullptr;
    QTimer *m_timer = nullptr;
};
