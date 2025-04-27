#include "UI/EdgesWidgets/SceneTreeWidget.h"

#include "ECSCore.h"
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

    for (EntityID entity: mWorld->view<RenderableComponent, TransformComponent>()) {
        auto *item = new QTreeWidgetItem(this);

        // NameComponent* nameComp = mWorld->getComponent<NameComponent>(entity);
        // if (nameComp && !nameComp->name.isEmpty()) {
        //     item->setText(0, QString("%1 (ID: %2)").arg(nameComp->name).arg(entity));
        // } else {
        item->setText(0, QString("Entity (ID: %1)").arg(entity));
        // }

        item->setData(0, Qt::UserRole, QVariant::fromValue(entity)); // Store EntityID

        // Hierarchy (Advanced):
        // entityItemMap[entity] = item;
        // ParentComponent* parentComp = mWorld->getComponent<ParentComponent>(entity);
        // if (parentComp && entityItemMap.contains(parentComp->parentEntity)) {
        //     entityItemMap[parentComp->parentEntity]->addChild(item);
        // } else {
        //     addTopLevelItem(item);
        // }
        addTopLevelItem(item);
    }
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
