#include "UI/EdgesWidgets/SceneTreeWidget.h"

SceneTreeWidget::SceneTreeWidget(QWidget *parent)
    : QTreeWidget(parent) {
    setHeaderLabel("场景对象");
    setSelectionMode(SingleSelection);
    connect(this, &QTreeWidget::itemSelectionChanged, this, &SceneTreeWidget::handleSelectionChanged);
}

void SceneTreeWidget::refreshSceneTree() {
    clear();
    // QVector<SceneObjectInfo> objects;
    // SceneManager::get().getObjects(objects);
    //
    // for (const auto &obj: objects) {
    //     auto *item = new QTreeWidgetItem(this);
    //     item->setText(0, obj.name);
    //     item->setData(0, Qt::UserRole, obj.id);
    // }
}

void SceneTreeWidget::handleSelectionChanged() {
    auto items = selectedItems();
    emit objectSelected(items.isEmpty() ? -1 : items.first()->data(0, Qt::UserRole).toInt());
}
