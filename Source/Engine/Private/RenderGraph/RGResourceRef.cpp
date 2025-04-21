#include "RenderGraph/RGResourceRef.h"

RGTextureRef::RGTextureRef(RGTexture *tex) : RGResourceRef(tex) {
}

QRhiTexture *RGTextureRef::get() const {
    return mResource ? static_cast<RGTexture *>(mResource)->mRhiTexture.get() : nullptr;
}

QSize RGTextureRef::pixelSize() const {
    return mResource ? static_cast<RGTexture *>(mResource)->size() : QSize();
}

QRhiTexture::Format RGTextureRef::format() const {
    return mResource ? static_cast<RGTexture *>(mResource)->format() : QRhiTexture::UnknownFormat;
}

RGBufferRef::RGBufferRef(RGBuffer *buf) : RGResourceRef(buf) {
}

QRhiBuffer *RGBufferRef::get() const {
    return mResource ? static_cast<RGBuffer *>(mResource)->mRhiBuffer.get() : nullptr;
}

quint32 RGBufferRef::size() const {
    return mResource ? static_cast<RGBuffer *>(mResource)->size() : 0;
}
