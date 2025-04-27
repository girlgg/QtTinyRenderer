#pragma once
#include <QTreeWidget>

#include "ECSCore.h"

class World;

class SceneTreeWidget : public QTreeWidget {
    Q_OBJECT

public:
    explicit SceneTreeWidget(QWidget *parent = nullptr);

    void setWorld(QSharedPointer<World> world);

public slots:
    void refreshSceneTree();

private slots:
    void handleSelectionChanged();

signals:
    void objectSelected(EntityID objId);

private:
    QSharedPointer<World> mWorld;
};
