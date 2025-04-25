#include "RenderGraph/RGResource.h"

bool RGTexture::build(QRhi *inRhi) {
    if (!mSize.isValid() || mSize.width() <= 0 || mSize.height() <= 0) {
        qWarning("RGTexture::build - Cannot build texture '%s' with invalid size: %dx%d", qPrintable(name()),
                 mSize.width(), mSize.height());
        return false;
    }
    if (mRhiTexture && mRhiTexture->pixelSize() == mSize && mRhiTexture->format() == mFormat &&
        mRhiTexture->sampleCount() == mSampleCount && mRhiTexture->flags() == mFlags) {
        qDebug() << "RGTexture::build - RHI object for" << name() << "already exists and matches. Skipping build.";
        return true;
    }
    qInfo() << "RGTexture::build - Creating/Recreating RHI Texture for:" << name() << "Size:" << mSize << "Format:" <<
            mFormat;
    if (mRhiTexture) mRhiTexture.reset();

    mRhiTexture.reset(inRhi->newTexture(mFormat, mSize, mSampleCount, mFlags));
    if (mRhiTexture) {
        mRhiTexture->setName(name().toUtf8());
        if (mRhiTexture->create()) {
            return true;
        }
        qWarning("RGTexture::build - QRhiTexture::create() failed for %s", qPrintable(name()));
        mRhiTexture.reset();
        return false;
    }
    qWarning("RGTexture::build - Failed to allocate QRhiTexture object for %s", qPrintable(name()));
    return false;
}

bool RGBuffer::build(QRhi *inRhi) {
    if (mRhiBuffer && mRhiBuffer->type() == mBufType && mRhiBuffer->usage() == mUsage && mRhiBuffer->size() == mSize) {
        return true;
    }
    qInfo() << "RGBuffer::build - Creating/Recreating RHI Buffer for:" << name() << "Size:" << mSize << "Type:" <<
            mBufType;
    if (mRhiBuffer) mRhiBuffer.reset();

    mRhiBuffer.reset(inRhi->newBuffer(mBufType, mUsage, mSize));
    if (mRhiBuffer) {
        mRhiBuffer->setName(name().toUtf8());
        if (mRhiBuffer->create()) {
            return true;
        }
        qWarning("RGBuffer::build - QRhiBuffer::create() failed for %s", qPrintable(name()));
        mRhiBuffer.reset();
        return false;
    }
    qWarning("RGBuffer::build - Failed to allocate QRhiBuffer object for %s", qPrintable(name()));
    return false;
}

bool RGRenderBuffer::build(QRhi *inRhi) {
    if (!mSize.isValid() || mSize.width() <= 0 || mSize.height() <= 0) {
        qWarning("RGRenderBuffer::build - Cannot build render buffer '%s' with invalid size: %dx%d", qPrintable(name()),
                 mSize.width(), mSize.height());
        return false;
    }
    if (mRhiRenderBuffer && mRhiRenderBuffer->type() == mRbType && mRhiRenderBuffer->pixelSize() == mSize &&
        mRhiRenderBuffer->sampleCount() == mSampleCount && mRhiRenderBuffer->flags() == mFlags) {
        return true;
    }
    qInfo() << "RGRenderBuffer::build - Creating/Recreating RHI RenderBuffer for:" << name() << "Size:" << mSize <<
            "Type:" << mRbType;
    if (mRhiRenderBuffer) mRhiRenderBuffer.reset();

    mRhiRenderBuffer.reset(inRhi->newRenderBuffer(mRbType, mSize, mSampleCount, mFlags));
    if (mRhiRenderBuffer) {
        mRhiRenderBuffer->setName(name().toUtf8());
        if (mRhiRenderBuffer->create()) {
            return true;
        }
        qWarning("RGRenderBuffer::build - QRhiRenderBuffer::create() failed for %s", qPrintable(name()));
        mRhiRenderBuffer.reset();
        return false;
    }
    qWarning("RGRenderBuffer::build - Failed to allocate QRhiRenderBuffer object for %s", qPrintable(name()));
    return false;
}

RGRenderTarget::RGRenderTarget(const QString &name, const QVector<RGTextureRef> &colorAttachmentRefs,
                               RGResourceRef depthStencilRef)
    : RGResource(name, Type::RenderTarget),
      mColorAttachmentRefs(colorAttachmentRefs),
      mDepthStencilAttachmentRef(depthStencilRef) {
    if (mColorAttachmentRefs.isEmpty() && !mDepthStencilAttachmentRef.isValid()) {
        qWarning(
            "RGRenderTarget '%s' created with NO attachments and NO external descriptor. Build will likely fail.",
            qPrintable(name));
    } else {
        qInfo("RGRenderTarget '%s' created for internal rendering (will create QRhiRenderTarget).",
              qPrintable(name));
    }
}

RGRenderTarget::RGRenderTarget(const QString &name, QRhiRenderPassDescriptor *externalRpDesc)
    : RGResource(name, Type::RenderTarget),
      mColorAttachmentRefs({}),
      mRpDesc(externalRpDesc),
      mIsExternalRpDesc(true) {
    if (!mRpDesc) {
        qCritical("RGRenderTarget '%s' created as external but with a NULL externalRpDesc!", qPrintable(name));
    } else {
        qInfo("RGRenderTarget '%s' created using an EXTERNAL RenderPassDescriptor.", qPrintable(name));
    }
}

bool RGRenderTarget::build(QRhi *inRhi) {
    if (mIsExternalRpDesc) {
        if (!mRpDesc) {
            qCritical("RGRenderTarget::build - External target '%s' has lost its external RenderPassDescriptor!",
                      qPrintable(name()));
            return false;
        }
        qInfo() << "RGRenderTarget::build - Skipping build for" << name() << "as it uses an external RpDesc.";
        if (mRhiRenderTarget) mRhiRenderTarget.reset();
        if (mRpDescOwned) mRpDescOwned.reset();
        return true;
    }
    // --- 构建内部渲染目标 ---
    qInfo() << "RGRenderTarget::build - Building internal QRhiRenderTarget for:" << name();

    if (mRpDescOwned) {
        mRpDescOwned.reset();
    }
    mRpDesc = nullptr;
    if (mRhiRenderTarget) {
        mRhiRenderTarget.reset();
    }

    // 验证依赖
    bool dependenciesMet = true;
    QRhiTextureRenderTargetDescription rtDesc;
    QVector<QRhiColorAttachment> colorAttachments;
    colorAttachments.reserve(mColorAttachmentRefs.size());

    for (const RGTextureRef &colorRef: qAsConst(mColorAttachmentRefs)) {
        if (!colorRef.isValid() || !colorRef.mResource) {
            qWarning("RGRenderTarget::build '%s': Invalid color attachment Ref.", qPrintable(name()));
            dependenciesMet = false;
            break;
        }
        QRhiTexture *colorTex = colorRef.get();
        if (!colorTex) {
            qWarning("RGRenderTarget::build '%s': Dependency Color Texture '%s' is not built yet.", qPrintable(name()),
                     qPrintable(colorRef.mResource->name()));
            dependenciesMet = false;
            break;
        }
        colorAttachments.append(QRhiColorAttachment(colorTex));
    }
    if (!dependenciesMet) return false;
    rtDesc.setColorAttachments(colorAttachments.constBegin(), colorAttachments.constEnd());

    if (mDepthStencilAttachmentRef.isValid()) {
        if (!mDepthStencilAttachmentRef.mResource) {
            qWarning("RGRenderTarget::build '%s': Invalid depth/stencil attachment Ref.", qPrintable(name()));
            return false;
        }
        // 只允许使用buffer，不允许使用Texture
        if (mDepthStencilAttachmentRef.mResource->type() == Type::RenderBuffer) {
            RGRenderBuffer *rgDsBuffer = static_cast<RGRenderBuffer *>(mDepthStencilAttachmentRef.mResource.get());
            QRhiRenderBuffer *dsBuffer = rgDsBuffer->mRhiRenderBuffer.get();
            if (dsBuffer) {
                rtDesc.setDepthStencilBuffer(dsBuffer);
                qInfo("  Attached DepthStencil Buffer: %s", qPrintable(rgDsBuffer->name()));
            } else {
                qWarning("RGRenderTarget::build '%s': Dependency Depth/Stencil Buffer '%s' is not built yet.",
                         qPrintable(name()), qPrintable(rgDsBuffer->name()));
                dependenciesMet = false;
            }
        } else {
            qWarning(
                "RGRenderTarget::build '%s': Depth/Stencil attachment '%s' is a Texture. QRhiTextureRenderTarget requires a RenderBuffer for depth/stencil.",
                qPrintable(name()), qPrintable(mDepthStencilAttachmentRef.mResource->name()));
            dependenciesMet = false;
        }
    }
    if (!dependenciesMet) return false;

    // 创建QRhiRenderTarget对象
    mRhiRenderTarget.reset(inRhi->newTextureRenderTarget(rtDesc));
    if (!mRhiRenderTarget) {
        qWarning("RGRenderTarget::build - Failed to allocate QRhiTextureRenderTarget object for %s",
                 qPrintable(name()));
        return false;
    }
    mRhiRenderTarget->setName(name().toUtf8());
    QRhiTextureRenderTarget *textureRT = dynamic_cast<QRhiTextureRenderTarget *>(mRhiRenderTarget.get());
    if (!textureRT) {
        qWarning(
            "RGRenderTarget::build - Failed to cast QRhiRenderTarget to QRhiTextureRenderTarget for %s. Cannot create compatible RpDesc.",
            qPrintable(name()));
        mRhiRenderTarget.reset(); // Release the unusable RT object
        return false;
    }
    mRpDescOwned.reset(textureRT->newCompatibleRenderPassDescriptor());
    if (!mRpDescOwned) {
        qWarning("RGRenderTarget::build - Failed to create compatible RenderPassDescriptor for %s", qPrintable(name()));
        mRhiRenderTarget.reset();
        return false;
    }
    mRpDescOwned->setName(name().toUtf8() + "_RpDesc");
    mRpDesc = mRpDescOwned.get();
    mRhiRenderTarget->setRenderPassDescriptor(mRpDesc);
    qInfo() << "  Successfully created RHI RenderTarget:" << name() << "and its owned RenderPassDescriptor.";
    return true;
}

QRhiRenderPassDescriptor *RGRenderTarget::renderPassDescriptor() const {
    if (!mRpDesc) {
        qWarning("RGRenderTarget::renderPassDescriptor() called for '%s', but mRpDesc is null. External: %d",
                 qPrintable(name()), mIsExternalRpDesc);
    }
    return mRpDesc;
}

QSize RGRenderTarget::getPixelSize() const {
    if (!mColorAttachmentRefs.isEmpty() && mColorAttachmentRefs[0].isValid()) {
        return mColorAttachmentRefs[0].pixelSize();
    }
    if (mDepthStencilAttachmentRef.isValid()) {
        if (auto dsRbRef = qSharedPointerCast<RGRenderBuffer>(mDepthStencilAttachmentRef.mResource)) {
            return dsRbRef->size();
        }
        if (auto dsTexRef = qSharedPointerCast<RGTexture>(mDepthStencilAttachmentRef.mResource)) {
            return dsTexRef->size();
        }
    } else if (mRhiRenderTarget) {
        return mRhiRenderTarget->pixelSize();
    }
    qWarning("RGRenderTarget::getPixelSize() - Could not determine size for '%s'.", qPrintable(name()));
    return QSize();
}

int RGRenderTarget::getSampleCount() const {
    int sampleCount = 1;

    if (mDepthStencilAttachmentRef.isValid()) {
        if (auto dsRbRef = qSharedPointerCast<RGRenderBuffer>(mDepthStencilAttachmentRef.mResource)) {
            sampleCount = dsRbRef->sampleCount();
        } else if (auto dsTexRef = qSharedPointerCast<RGTexture>(mDepthStencilAttachmentRef.mResource)) {
            sampleCount = dsTexRef->sampleCount();
        }
    } else if (mRhiRenderTarget) {
        sampleCount = mRhiRenderTarget->sampleCount();
    }
    if (sampleCount <= 0) sampleCount = 1;
    return sampleCount;
}

RGPipeline::RGPipeline(const QString &name, RGShaderResourceBindingsRef srbLayoutRef, RGRenderTargetRef renderTargetRef,
                       const QVector<QRhiShaderStage> &shaderStages, const QRhiVertexInputLayout &vertexInputLayout,
                       int sampleCount,
                       QRhiGraphicsPipeline::Topology topology, QRhiGraphicsPipeline::CullMode cullMode,
                       QRhiGraphicsPipeline::FrontFace frontFace, bool depthTest, bool depthWrite,
                       QRhiGraphicsPipeline::CompareOp depthOp,
                       bool stencilTest, QRhiGraphicsPipeline::StencilOpState stencilFront,
                       QRhiGraphicsPipeline::StencilOpState stencilBack, quint32 stencilReadMask,
                       quint32 stencilWriteMask,
                       const QVector<QRhiGraphicsPipeline::TargetBlend> &targetBlends,
                       QRhiGraphicsPipeline::PolygonMode polygonMode,
                       float lineWidth, int patchControlPoints,
                       int depthBias, float slopeScaledDepthBias) : RGResource(name, Type::Pipeline),
                                                                    mSrbLayoutRef(srbLayoutRef),
                                                                    mRenderTargetRef(renderTargetRef),
                                                                    mShaderStages(shaderStages),
                                                                    mVertexInputLayout(vertexInputLayout),
                                                                    mSampleCount(sampleCount),
                                                                    mTopology(topology),
                                                                    mCullMode(cullMode),
                                                                    mFrontFace(frontFace),
                                                                    mDepthTest(depthTest),
                                                                    mDepthWrite(depthWrite),
                                                                    mDepthOp(depthOp),
                                                                    mStencilTest(stencilTest),
                                                                    mStencilFront(stencilFront),
                                                                    mStencilBack(stencilBack),
                                                                    mStencilReadMask(stencilReadMask),
                                                                    mStencilWriteMask(stencilWriteMask),
                                                                    mTargetBlends(targetBlends),
                                                                    mPolygonMode(polygonMode),
                                                                    mLineWidth(lineWidth),
                                                                    mPatchControlPoints(patchControlPoints),
                                                                    mDepthBias(depthBias),
                                                                    mSlopeScaledDepthBias(slopeScaledDepthBias) {
    if (!mSrbLayoutRef.isValid()) {
        qWarning() << "RGPipeline" << name << "created with invalid SRB Layout Ref.";
    }
    if (!renderTargetRef.isValid()) {
        qWarning() << "RGPipeline" << name << "created with null RenderPassDescriptor.";
    }
}

bool RGPipeline::build(QRhi *inRhi) {
    qInfo() << "RGPipeline::build - Building RHI Pipeline for:" << name();
    // 验证依赖(SRB 布局和RenderTarget Description)
    bool dependenciesMet = true;
    QRhiShaderResourceBindings *srbLayoutPtr = srbLayout();
    QRhiRenderPassDescriptor *rpDescPtr = rpDesc();

    if (!srbLayoutPtr) {
        qWarning("RGPipeline::build '%s': Dependency SRB Layout RHI object ('%s') is not built yet.",
                 qPrintable(name()),
                 qPrintable(mSrbLayoutRef.mResource ? mSrbLayoutRef.mResource->name() : "INVALID_REF"));
        dependenciesMet = false;
    }
    if (!rpDescPtr) {
        qWarning(
            "RGPipeline::build '%s': Failed to get RenderPassDescriptor from RenderTarget description ('%s'). Is the RT description valid?",
            qPrintable(name()),
            qPrintable(mRenderTargetRef.mResource ? mRenderTargetRef.mResource->name() : "INVALID_REF"));
        dependenciesMet = false;
    }
    for (const auto &stage: qAsConst(mShaderStages)) {
        if (!stage.shader().isValid()) {
            qWarning("RGPipeline::build '%s': Contains invalid shader stage (Type: %d).", qPrintable(name()),
                     stage.type());
            dependenciesMet = false;
            break;
        }
    }

    if (!dependenciesMet) {
        qWarning("Skipping build of Pipeline '%s' due to missing dependencies.", qPrintable(name()));
        return false;
    }
    // 创建 QRhiGraphicsPipeline 对象
    if (mRhiGraphicsPipeline) mRhiGraphicsPipeline.reset();
    mRhiGraphicsPipeline.reset(inRhi->newGraphicsPipeline());
    if (mRhiGraphicsPipeline) {
        mRhiGraphicsPipeline->setName(name().toUtf8());

        mRhiGraphicsPipeline->setRenderPassDescriptor(rpDescPtr);
        mRhiGraphicsPipeline->setShaderResourceBindings(srbLayoutPtr);
        mRhiGraphicsPipeline->setShaderStages(mShaderStages.constBegin(), mShaderStages.constEnd());
        mRhiGraphicsPipeline->setVertexInputLayout(mVertexInputLayout);
        mRhiGraphicsPipeline->setSampleCount(mSampleCount);
        mRhiGraphicsPipeline->setTopology(mTopology);
        mRhiGraphicsPipeline->setCullMode(mCullMode);
        mRhiGraphicsPipeline->setFrontFace(mFrontFace);
        mRhiGraphicsPipeline->setDepthTest(mDepthTest);
        mRhiGraphicsPipeline->setDepthWrite(mDepthWrite);
        mRhiGraphicsPipeline->setDepthOp(mDepthOp);
        mRhiGraphicsPipeline->setStencilTest(mStencilTest);
        mRhiGraphicsPipeline->setStencilFront(mStencilFront);
        mRhiGraphicsPipeline->setStencilBack(mStencilBack);
        mRhiGraphicsPipeline->setStencilReadMask(mStencilReadMask);
        mRhiGraphicsPipeline->setStencilWriteMask(mStencilWriteMask);
        if (!targetBlends().isEmpty()) {
            mRhiGraphicsPipeline->setTargetBlends(mTargetBlends.constBegin(), mTargetBlends.constEnd());
        }
        mRhiGraphicsPipeline->setPolygonMode(mPolygonMode);
        mRhiGraphicsPipeline->setLineWidth(mLineWidth);
        if (patchControlPoints() > 0) {
            mRhiGraphicsPipeline->setPatchControlPointCount(mPatchControlPoints);
        }
        mRhiGraphicsPipeline->setDepthBias(mDepthBias);
        mRhiGraphicsPipeline->setSlopeScaledDepthBias(mSlopeScaledDepthBias);
        if (mRhiGraphicsPipeline->create()) {
            qInfo() << "  Successfully created RHI pipeline:" << name();
            return true;
        }
        qWarning("RGPipeline::build - QRhiGraphicsPipeline::create() failed for %s", qPrintable(name()));
        mRhiGraphicsPipeline.reset();
        return false;
    }
    qWarning("Failed to create QRhiGraphicsPipeline object for %s", qPrintable(name()));
    return false;
}

QRhiRenderPassDescriptor *RGPipeline::rpDesc() const {
    if (mRenderTargetRef.isValid() && mRenderTargetRef.mResource) {
        return static_cast<RGRenderTarget *>(mRenderTargetRef.mResource.get())->renderPassDescriptor();
    }
    qWarning(
        "RGPipeline::rpDesc() - Cannot get descriptor, RenderTargetRef is invalid or resource is null for pipeline '%s'.",
        qPrintable(name()));
    return nullptr;
}

bool RGShaderResourceBindings::build(QRhi *inRhi) {
    if (mRhiShaderResourceBindings) {
        return true;
    }
    mRhiShaderResourceBindings.reset(inRhi->newShaderResourceBindings());
    if (mRhiShaderResourceBindings) {
        mRhiShaderResourceBindings->setName(name().toUtf8());
        mRhiShaderResourceBindings->setBindings(mBindings.constBegin(), mBindings.constEnd());
        if (mRhiShaderResourceBindings->create()) {
            return true;
        }
        mRhiShaderResourceBindings.reset();
        return false;
    }
    qWarning("RGShaderResourceBindings::build - Failed to allocate QRhiShaderResourceBindings object for layout %s",
             qPrintable(name()));
    return false;
}

bool RGSampler::build(QRhi *inRhi) {
    if (mRhiSampler && mRhiSampler->magFilter() == mMagFilter && mRhiSampler->minFilter() == mMinFilter && mRhiSampler->
        mipmapMode() == mMipmapMode && mRhiSampler->addressU() == mAddressU && mRhiSampler->addressV() == mAddressV &&
        mRhiSampler->addressW() == mAddressW) {
        return true;
    }

    if (mRhiSampler) mRhiSampler.reset();

    mRhiSampler.reset(inRhi->newSampler(mMagFilter, mMinFilter, mMipmapMode, mAddressU, mAddressV, mAddressW));
    if (mRhiSampler) {
        mRhiSampler->setName(name().toUtf8());
        if (mRhiSampler->create()) {
            return true;
        }
        qWarning("RGSampler::build - QRhiSampler::create() failed for %s", qPrintable(name()));
        mRhiSampler.reset();
        return false;
    }
    qWarning("RGSampler::build - Failed to allocate QRhiSampler object for %s", qPrintable(name()));
    return false;
}
