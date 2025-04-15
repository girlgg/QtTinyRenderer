#include "UI/EdgesWidgets/EditorToolBar.h"
#include <QLayout>

EditorToolBar::EditorToolBar(QWidget *parent) :
    QToolBar(parent) {
    setObjectName("ToolBar");
    setAttribute(Qt::WA_TranslucentBackground);
}

EditorToolBar::EditorToolBar(const QString &title, QWidget *parent)
    : EditorToolBar(parent){
  setWindowTitle(title);
}

EditorToolBar::~EditorToolBar() {
}

void EditorToolBar::setToolBarSpacing(int spacing) {
    layout()->setSpacing(spacing);
}
