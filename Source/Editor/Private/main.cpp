#include <QApplication>
#include <QPushButton>
#include <QDirIterator>
#include "UI/EditorMainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    EditorMainWindow Window;
    Window.show();

    // QDirIterator it(":/", QDirIterator::Subdirectories);
    // while (it.hasNext()) {
    //     QString resourcePath = it.next();
    //     qDebug() << resourcePath;
    // }

    return app.exec();
}
