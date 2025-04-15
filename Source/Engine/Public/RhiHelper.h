#pragma once

#include <QWindow>
#include <rhi/qrhi.h>

#ifndef QT_NO_OPENGL
#include <QOffscreenSurface>
#endif

#include "Graphics/Vulkan/QRhiVulkanExHelper.h"

class QRhiSignal {
public:
    QRhiSignal() = default;

    void request() {
        bDirty = true;
    }

    bool ensure() {
        bool tmp = bDirty;
        bDirty = false;
        return tmp;
    }

    bool peek() {
        return bDirty;
    }

private:
    bool bDirty = false;
};

namespace RhiHelper {
    struct InitParams {
        QRhi::Implementation backend = QRhi::Vulkan;
        QRhi::Flags rhiFlags = QRhi::Flag();
        QRhiSwapChain::Flags swapChainFlags = QRhiSwapChain::Flag::SurfaceHasNonPreMulAlpha;
        QRhi::BeginFrameFlags beginFrameFlags;
        QRhi::EndFrameFlags endFrameFlags;
        int sampleCount = 1;
        bool enableStat = false;
    };

    static QSharedPointer<QRhi> create(QRhi::Implementation inBackend = QRhi::Vulkan,
                        QRhi::Flags inFlags = QRhi::Flag(), QWindow *inWindow = nullptr) {
        QSharedPointer<QRhi> rhi;
        if (inBackend == QRhi::Null) {
            QRhiNullInitParams params;
            rhi.reset(QRhi::create(QRhi::Null, &params, inFlags));
        }
#ifndef QT_NO_OPENGL
        if (inBackend == QRhi::OpenGLES2) {
            QRhiGles2InitParams params;
            params.fallbackSurface = QRhiGles2InitParams::newFallbackSurface();
            params.window = inWindow;
            rhi.reset(QRhi::create(QRhi::OpenGLES2, &params, inFlags));
        }
#endif

#if QT_CONFIG(vulkan)
        if (inBackend == QRhi::Vulkan) {
            QVulkanInstance *vkInstance = QRhiVulkanExHelper::instance();
            QRhiVulkanInitParams params;
            if (inWindow) {
                inWindow->setVulkanInstance(vkInstance);
                params.window = inWindow;
            }
            params.inst = vkInstance;
            auto importedHandles = QRhiVulkanExHelper::createVulkanNativeHandles(params);
            rhi.reset(QRhi::create(QRhi::Vulkan, &params, inFlags, &importedHandles));
        }
#endif

#ifdef Q_OS_WIN
        if (inBackend == QRhi::D3D11) {
            QRhiD3D11InitParams params;
            params.enableDebugLayer = true;
            rhi.reset(QRhi::create(QRhi::D3D11, &params, inFlags));
        }
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
        if (inBackend == QRhi::D3D12) {
            QRhiD3D12InitParams params;
            params.enableDebugLayer = true;
            rhi.reset(QRhi::create(QRhi::D3D12, &params, inFlags));
        }
#endif

#endif

#if defined(Q_OS_MACOS) || defined(Q_OS_IOS)
        if (inBackend == QRhi::Metal) {
            QRhiMetalInitParams params;
            rhi.reset(static_cast<QRhi*>(QRhi::create(QRhi::Metal, &params, inFlags)));
        }
#endif
        return rhi;
    }
};
