#pragma once
#include <rhi/qrhi.h>

#include "RGResource.h"

class RGResourceRef {
public:
    RGResourceRef(RGResource *res = nullptr) : mResource(res) {
    }

    bool isValid() const { return mResource != nullptr; }

protected:
    RGResource *mResource = nullptr;
    friend class RGBuilder;
    friend class RenderGraph;
};

class RGTextureRef : public RGResourceRef {
public:
    RGTextureRef(RGTexture *tex = nullptr);

    QRhiTexture *get() const;

    QSize pixelSize() const;

    QRhiTexture::Format format() const;
};

class RGBufferRef : public RGResourceRef {
public:
    RGBufferRef(RGBuffer *buf = nullptr);

    QRhiBuffer *get() const;

    quint32 size() const;
};
