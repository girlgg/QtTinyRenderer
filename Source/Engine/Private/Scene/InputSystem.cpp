#include "Scene/InputSystem.h"

#include <kernel/qevent.h>
#include <QtWidgets/QApplication>
#include <QWidget>

InputSystem &InputSystem::get() {
    static InputSystem instance;
    return instance;
}

void InputSystem::setMouseCaptured(bool captured) {
    mMouseCaptured = captured;

    QWidget *widget = QApplication::activeWindow();
    if (widget) {
        widget->setCursor(captured ? Qt::BlankCursor : Qt::ArrowCursor);
        widget->setMouseTracking(captured);

        if (captured) {
            mMouseDelta = {0, 0};
        }
    }
}

InputSystem::~InputSystem() {
}

bool InputSystem::getKey(Qt::Key key) const {
    int want_key = key;
    return mKeyStates.contains(want_key) && mKeyStates[want_key];
}

bool InputSystem::getMouseButton(Qt::MouseButton button) const {
    return mMouseStates.contains(button) && mMouseStates[button];
}

QPointF InputSystem::getMouseDelta() const {
    return mMouseDelta;
}

QPointF InputSystem::consumeMouseDelta() {
    QPointF delta = mMouseDelta;
    mMouseDelta = QPointF(0, 0);
    return delta;
}

bool InputSystem::eventFilter(QObject *watched, QEvent *event) {
    switch (event->type()) {
        case QEvent::KeyPress: {
            int pressedKey = static_cast<QKeyEvent *>(event)->key();
            mKeyStates[pressedKey] = true;
            if (pressedKey == Qt::Key_F) {
                setMouseCaptured(!mMouseCaptured);
            }
            break;
        }
        case QEvent::KeyRelease:
            mKeyStates[static_cast<QKeyEvent *>(event)->key()] = false;
            break;
        case QEvent::MouseMove: {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
            QPointF currentPos = mouseEvent->position();

            mMouseDelta = currentPos - mLastMousePos;
            mLastMousePos = currentPos;
            break;
        }
        case QEvent::MouseButtonPress:
            mMouseStates[static_cast<QMouseEvent *>(event)->button()] = true;
            break;
        case QEvent::MouseButtonRelease:
            mMouseStates[static_cast<QMouseEvent *>(event)->button()] = false;
            break;
    }
    return QObject::eventFilter(watched, event);
}

InputSystem::InputSystem() {
}
