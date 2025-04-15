#pragma once

#include <QToolBar>

class EditorToolBar : public QToolBar {
    Q_OBJECT

public:
    explicit EditorToolBar(QWidget *parent = nullptr);

    explicit EditorToolBar(const QString &title, QWidget *parent = nullptr);

    ~EditorToolBar() override;

    void setToolBarSpacing(int spacing);
};
