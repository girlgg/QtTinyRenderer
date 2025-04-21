#pragma once

#include <QHash>
#include <QSharedPointer>
#include <QSizeF>

class RGPass;
class RGResource;
class QRhiCommandBuffer;
class World;
class QRhi;
class ResourceManager;

class RenderGraph {
public:
    RenderGraph(QRhi *rhi, QSharedPointer<ResourceManager> resManager, QSharedPointer<World> world,
                const QSize &outputSize);

    ~RenderGraph();

    // 添加完pass后调用
    void compile();

    // 编译后调用
    void execute();

    RGResource *findResource(const QString &name) const;

    RGResource *registerResource(QSharedPointer<RGResource> resource);

    QRhi *getRhi() const { return mRhi; }
    QSharedPointer<ResourceManager> getResourceManager() const { return mResourceManager; }
    QSharedPointer<World> getWorld() const { return mWorld; }
    const QSize &getOutputSize() const { return mOutputSize; }
    QRhiCommandBuffer *getCommandBuffer() const { return mCommandBuffer; }

    template<typename PassType, typename... Args>
    PassType *addPass(const QString &name, Args &&... inArgs);

private:
    void createRhiResource(RGResource *resource);

    void releaseRhiResource(RGResource *resource);

    QRhi *mRhi;
    QSharedPointer<ResourceManager> mResourceManager;
    QSharedPointer<World> mWorld;
    QSize mOutputSize;

    QVector<QSharedPointer<RGPass> > mPasses;
    QHash<QString, QSharedPointer<RGResource> > mResources;
    QVector<RGPass *> mExecutionOrder;

    QRhiCommandBuffer *mCommandBuffer = nullptr;
};

template<typename PassType, typename... Args>
PassType *RenderGraph::addPass(const QString &name, Args &&... inArgs) {
    PassType *pass = new PassType(name, std::forward<Args>(inArgs)...);
    pass->mRhi = mRhi;
    pass->mResourceManager = mResourceManager;
    pass->mWorld = mWorld;
    mPasses.append(QSharedPointer<RGPass>(pass));
    return pass;
}
