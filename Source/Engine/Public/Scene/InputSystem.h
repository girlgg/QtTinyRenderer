#pragma once

#include <QObject>
#include <QHash>
#include <QPointF>
#include "Interface/IInputProvider.h"

class InputSystem : public QObject, public IInputProvider {
    Q_OBJECT

public:
    static InputSystem &get();

    void setMouseCaptured(bool captured);

    bool isMouseCaptured() const { return mMouseCaptured; }

    ~InputSystem();

    bool getKey(Qt::Key key) const;

    bool getMouseButton(Qt::MouseButton button) const;

    QPointF getMouseDelta() const;

    QPointF consumeMouseDelta();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    InputSystem();

    QHash<int, bool> mKeyStates;
    QHash<Qt::MouseButton, bool> mMouseStates;
    QPointF mLastMousePos;
    QPointF mMouseDelta;
    QPointF mWindowCenter;

    bool mMouseCaptured = false;
};
