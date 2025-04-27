#include "UI/EdgesWidgets/SceneTreeWidget.h"

#include "ECSCore.h"
#include "Component/LightComponent.h"
#include "Component/RenderableComponent.h"
#include "Component/TransformComponent.h"
#include "Scene/World.h"

SceneTreeWidget::SceneTreeWidget(QWidget *parent)
    : QTreeWidget(parent) {
    setHeaderLabel("场景对象");
    setSelectionMode(SingleSelection);
    connect(this, &QTreeWidget::itemSelectionChanged, this, &SceneTreeWidget::handleSelectionChanged);
}

void SceneTreeWidget::setWorld(QSharedPointer<World> world) {
    mWorld = world;
}

void SceneTreeWidget::refreshSceneTree() {
    clear();
    if (!mWorld) {
        qWarning("SceneTreeWidget::refreshSceneTree - World is not set.");
        return;
    }

    blockSignals(true);
    clearSelection();

    QHash<EntityID, QTreeWidgetItem *> entityItemMap;

    for (EntityID entity: mWorld->view<RenderableComponent, TransformComponent>()) {
        auto *rc = mWorld->getComponent<RenderableComponent>(entity);
        if (rc && rc->isVisible) {
            auto *item = new QTreeWidgetItem(this);

            item->setText(0, QString("Entity (ID: %1)").arg(entity));

            item->setData(0, Qt::UserRole, QVariant::fromValue(entity));

            addTopLevelItem(item);
            entityItemMap[entity] = item;
        }
    }

    for (EntityID entity: mWorld->view<LightComponent, TransformComponent>()) {
        if (entityItemMap.contains(entity)) {
            continue;
        }

        auto *lc = mWorld->getComponent<LightComponent>(entity);
        if (lc) {
            auto *item = new QTreeWidgetItem();

            QString typeStr;
            switch (lc->type) {
                case LightType::Point:
                    typeStr = "Point Light";
                    break;
                case LightType::Directional:
                    typeStr = "Directional Light";
                    break;
            }
            item->setText(0, QString("%1 (ID: %2)").arg(typeStr).arg(entity));

            item->setData(0, Qt::UserRole, QVariant::fromValue(entity));

            addTopLevelItem(item);
            entityItemMap[entity] = item;
        }
    }

    blockSignals(false);
    qDebug() << "Scene tree refreshed with" << topLevelItemCount() << "items.";
}

void SceneTreeWidget::handleSelectionChanged() {
    auto items = selectedItems();
    EntityID selectedId = INVALID_ENTITY;
    if (!items.isEmpty()) {
        bool ok;
        selectedId = items.first()->data(0, Qt::UserRole).toULongLong(&ok);
        if (!ok) {
            selectedId = INVALID_ENTITY;
        }
    }
    qDebug() << "SceneTreeWidget selection changed to EntityID:" << selectedId;
    emit objectSelected(selectedId);
}
