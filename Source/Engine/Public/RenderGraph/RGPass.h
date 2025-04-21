#pragma once

#include <QSharedPointer>
#include <QString>

class RGBuilder;
class ResourceManager;
class World;
class QRhi;
class QRhiCommandBuffer;

class RGPass {
public:
    RGPass(QString name) : mName(std::move(name)) {
    }

    virtual ~RGPass() = default;

    const QString &name() const { return mName; }

    virtual void setup(RGBuilder &builder) = 0;

    virtual void execute(QRhiCommandBuffer *cmdBuffer) = 0;

    // TODO: Add input/output resource tracking

protected:
    QString mName;
    QRhi *mRhi = nullptr;
    QSharedPointer<ResourceManager> mResourceManager;
    QSharedPointer<World> mWorld;
    friend class RenderGraph;
    friend class RGBuilder;
};
