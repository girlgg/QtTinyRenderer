#pragma once
#include <QStatusBar>

class QLabel;

class EditorStatusBar : public QStatusBar {
    Q_OBJECT

public:
    explicit EditorStatusBar(QWidget *parent = nullptr);

    ~EditorStatusBar() override;

public slots:
    void fpsUpdate(float deltaTime, int fps);

private:
    QLabel *EditorStatusLabel;
    QLabel *EditorFpsLabel;
};

