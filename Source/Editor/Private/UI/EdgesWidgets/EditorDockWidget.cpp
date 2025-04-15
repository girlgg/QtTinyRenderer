#include "UI/EdgesWidgets/EditorDockWidget.h"


EditorDockWidget::EditorDockWidget(QWidget *parent, Qt::WindowFlags flags) :
    QDockWidget(parent, flags) {
    setObjectName("DockWidget");
}

EditorDockWidget::EditorDockWidget(const QString &title, QWidget *parent, Qt::WindowFlags flags)
    : EditorDockWidget(parent, flags){
  this->setWindowTitle(title);
}

EditorDockWidget::~EditorDockWidget() {
}
