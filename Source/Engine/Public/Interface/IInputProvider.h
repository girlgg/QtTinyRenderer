#pragma once

#include <QPointF>

class IInputProvider {
public:
    virtual bool getKey(Qt::Key key) const = 0;
    virtual bool getMouseButton(Qt::MouseButton button) const = 0;
    virtual QPointF consumeMouseDelta() = 0;
    virtual ~IInputProvider() = default;
};
