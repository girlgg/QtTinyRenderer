#include "RenderGraph/RenderGraph.h"

RenderGraph::RenderGraph(QRhi *rhi, QSharedPointer<ResourceManager> resManager, QSharedPointer<World> world,
                         const QSize &outputSize)
    : mRhi(rhi), mResourceManager(resManager), mWorld(world), mOutputSize(outputSize) {
}

RenderGraph::~RenderGraph() {
}

void RenderGraph::compile() {
}

void RenderGraph::execute() {
}

RGResource * RenderGraph::findResource(const QString &name) const {
}

RGResource * RenderGraph::registerResource(QSharedPointer<RGResource> resource) {
}

void RenderGraph::createRhiResource(RGResource *resource) {
}

void RenderGraph::releaseRhiResource(RGResource *resource) {
}
