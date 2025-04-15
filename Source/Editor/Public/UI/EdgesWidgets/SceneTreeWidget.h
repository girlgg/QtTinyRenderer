#pragma once
#include <QTreeWidget>

class SceneTreeWidget : public QTreeWidget {
    Q_OBJECT

public:
    explicit SceneTreeWidget(QWidget *parent = nullptr);

    void refreshSceneTree();

signals:
    void objectSelected(int objId);

private slots:
    void handleSelectionChanged();
};
