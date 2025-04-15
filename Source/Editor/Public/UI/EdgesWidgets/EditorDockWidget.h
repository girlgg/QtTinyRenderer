#pragma once

#include <QDockWidget>

class EditorDockWidget : public QDockWidget {
Q_OBJECT

public:
    explicit EditorDockWidget(QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
    explicit EditorDockWidget(const QString &title, QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
    ~EditorDockWidget() override;
};
