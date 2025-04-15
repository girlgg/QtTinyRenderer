#include "UI/EdgesWidgets/EditorStatusBar.h"

#include <QLabel>

EditorStatusBar::EditorStatusBar(QWidget *parent) : QStatusBar(parent) {
    // 设置固定高度
    setFixedHeight(24);

    // 初始化消息标签
    EditorStatusLabel = new QLabel("就绪", this);
    EditorStatusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    // 初始化 FPS 标签
    EditorFpsLabel = new QLabel(this);
    EditorFpsLabel->setObjectName("fpsLabel");
    EditorFpsLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    EditorFpsLabel->setText("FPS: --");

    // 添加部件
    addWidget(EditorStatusLabel, 1); // 可伸缩
    addPermanentWidget(EditorFpsLabel);
}

EditorStatusBar::~EditorStatusBar() {
}

void EditorStatusBar::fpsUpdate(float deltaTime, int fps) {
    if (deltaTime < 1 || fps < 1)
        return;

    QString color;
    if (fps >= 50) color = "#2ecc71";
    else if (fps >= 30) color = "#f39c12";
    else color = "#e74c3c";

    QString text = QString("<span style='color:%1'>FPS: %2 | DeltaTime: %3ms</span>")
            .arg(color)
            .arg(fps, 0, 'f', 1)
            .arg(deltaTime, 0, 'f', 2);

    EditorFpsLabel->setText(text);
}
