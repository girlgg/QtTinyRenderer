#pragma once

#include <QHash>
#include <QSharedPointer>
#include <QSizeF>
#include <QDebug>
class QRhiSwapChain;
class QRhiRenderPassDescriptor;
class RGPass;
class RGResource;
class QRhiCommandBuffer;
class World;
class QRhi;
class ResourceManager;

class RenderGraph {
public:
    RenderGraph(QRhi *rhi,
                QSharedPointer<ResourceManager> resManager,
                QSharedPointer<World> world,
                const QSize &outputSize,
                QRhiRenderPassDescriptor *swapChainRpDesc);

    ~RenderGraph();

    template<typename PassType, typename... Args>
    PassType *addPass(const QString &name, Args &&... inArgs);

    // 添加完pass后调用
    void compile();

    // 编译后调用
    void execute(QRhiSwapChain *swapChain);

    bool isCompiled() const { return mCompiled; }

    // --- 资源管理 ---
    QSharedPointer<RGResource> findResource(const QString &name) const;

    /**
     * @brief 将一个资源描述注册到 Render Graph 中。
     *
     * 如果已存在同名资源，则会发出警告并返回已存在的资源指针。否则将新的资源添加到图的资源池中。
     * 图通过 QSharedPointer 管理资源的生命周期。
     *
     * @param resource 指向要注册的资源描述的共享指针 (例如 RGTexture, RGBuffer)。资源必须有一个有效的非空名称。
     * @return 指向注册成功（或已存在）的资源的原始指针，如果输入无效则返回 nullptr。
     */
    RGResource *registerResource(const QSharedPointer<RGResource> &resource);

    bool removeResource(const QString &name);

    // --- Getters ---
    QRhi *getRhi() const { return mRhi; }
    QSharedPointer<ResourceManager> getResourceManager() const { return mResourceManager; }
    QSharedPointer<World> getWorld() const { return mWorld; }
    const QSize &getOutputSize() const { return mOutputSize; }
    QRhiRenderPassDescriptor *getSwapChainRpDesc() const { return mSwapChainRpDesc; }

    QRhiCommandBuffer *getCommandBuffer() const;

    QRhiSwapChain *getCurrentSwapChain() const { return mCurrentSwapChain; }

    // --- Setters ---
    void setCommandBuffer(QRhiCommandBuffer *cmdBuffer);

    void setOutputSize(const QSize &size);

private:
    void createRhiResource(RGResource *resource);

    void releaseRhiResource(RGResource *resource);

    // --- 所需状态 ---
    QRhi *mRhi;
    QSharedPointer<ResourceManager> mResourceManager;
    QSharedPointer<World> mWorld;
    QSize mOutputSize;
    QRhiRenderPassDescriptor *mSwapChainRpDesc;

    // --- 图形结构 ---
    QVector<QSharedPointer<RGPass> > mPasses;
    QHash<QString, QSharedPointer<RGResource> > mResources;
    QVector<RGPass *> mExecutionOrder;

    // --- 执行状态 ---
    QRhiCommandBuffer *mCommandBuffer = nullptr;
    QRhiSwapChain *mCurrentSwapChain = nullptr;
    bool mCompiled = false;
};

template<typename PassType, typename... Args>
PassType *RenderGraph::addPass(const QString &name, Args &&... inArgs) {
    // 检查是否已经有了pass
    for (const auto &existingPass: std::as_const(mPasses)) {
        if (existingPass && existingPass->name() == name) {
            qWarning("RenderGraph::addPass - Pass with name '%s' already exists.", qPrintable(name));
            if (auto castedPass = dynamic_cast<PassType *>(existingPass.get())) {
                return castedPass;
            }
            qCritical("RenderGraph::addPass - Existing pass '%s' has a different type than requested (%s)!",
                      qPrintable(name), typeid(PassType).name());
            return nullptr;
        }
    }

    QSharedPointer<PassType> pass = QSharedPointer<PassType>::create(name, std::forward<Args>(inArgs)...);

    pass->mRhi = mRhi;
    pass->mResourceManager = mResourceManager;
    pass->mWorld = mWorld;
    pass->mGraph = this;

    mPasses.append(pass);
    qInfo() << "RenderGraph: Added pass '" << name << "' of type" << typeid(PassType).name();

    mCompiled = false;

    return pass.get();
}
