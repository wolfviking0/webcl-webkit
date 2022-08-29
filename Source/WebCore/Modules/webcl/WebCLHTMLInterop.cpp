/*
 * Copyright (C) 2013 Samsung Electronics Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY SAMSUNG ELECTRONICS CORPORATION AND ITS
 * CONTRIBUTORS "AS IS", AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SAMSUNG
 * ELECTRONICS CORPORATION OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES(INCLUDING
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS, OR BUSINESS INTERRUPTION), HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT(INCLUDING
 * NEGLIGENCE OR OTHERWISE ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEBCL)

#include "WebCLHTMLInterop.h"

#include "CachedImage.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "SharedBuffer.h"

namespace WebCore {

void WebCLHTMLInterop::extractDataFromCanvas(HTMLCanvasElement* canvas, void*& hostPtr, size_t& canvasSize, ExceptionObject& exception)
{
    if (!canvas) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_HOST_PTR, exception);
        return;
    }
    Vector<uint8_t> data;
    CCerror error = ComputeContext::CCPackImageData(canvas->copiedImage(), GraphicsContext3D::HtmlDomCanvas, canvas->width(), canvas->height(), data);
    if (error != ComputeContext::SUCCESS) {
        setExceptionFromComputeErrorCode(error, exception);
        return;
    }
    hostPtr = data.data();
    canvasSize = data.size();
    if (!hostPtr || !canvasSize) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_HOST_PTR, exception);
        return;
    }
}

void WebCLHTMLInterop::extractDataFromImage(HTMLImageElement* image, void*& hostPtr, size_t& imageSize, ExceptionObject& exception)
{
    if (!image || !image->cachedImage()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_HOST_PTR, exception);
        return;
    }
    Vector<uint8_t> data;
    CCerror error = ComputeContext::CCPackImageData(image->cachedImage()->image(), GraphicsContext3D::HtmlDomImage, image->width(), image->height(), data);
    if (error != ComputeContext::SUCCESS) {
        setExceptionFromComputeErrorCode(error, exception);
        return;
    }
    hostPtr = data.data();
    imageSize = data.size();
    if (!hostPtr || !imageSize) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_HOST_PTR, exception);
        return;
    }
}

void WebCLHTMLInterop::extractDataFromImageData(ImageData* srcPixels, void*& hostPtr, size_t& pixelSize, ExceptionObject& exception)
{
    if (!srcPixels && !srcPixels->data() && !srcPixels->data()->data()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_HOST_PTR, exception);
        return;
    }
    pixelSize = srcPixels->data()->length();
    hostPtr = static_cast<void*>(srcPixels->data()->data());
    if (!hostPtr || !pixelSize) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_HOST_PTR, exception);
        return;
    }
}

void WebCLHTMLInterop::extractDataFromVideo(HTMLVideoElement* video, void*& hostPtr, size_t& videoSize, ExceptionObject& exception)
{
    if (!video) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_HOST_PTR, exception);
        return;
    }

    RefPtr<Image> image = videoFrameToImage(video);
    if (!image) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_HOST_PTR, exception);
        return;
    }

    Vector<uint8_t> data;
    CCerror error = ComputeContext::CCPackImageData(image.get(), GraphicsContext3D::HtmlDomVideo, video->videoWidth(), video->videoHeight(), data);
    if (error != ComputeContext::SUCCESS) {
        setExceptionFromComputeErrorCode(error, exception);
        return;
    }
    hostPtr = data.data();
    videoSize = data.size();

    if (!hostPtr || !videoSize) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_HOST_PTR, exception);
        return;
    }
}

PassRefPtr<Image> WebCLHTMLInterop::videoFrameToImage(HTMLVideoElement* video)
{
    if (!video || !video->videoWidth() || !video->videoHeight())
        return nullptr;
    IntSize size(video->videoWidth(), video->videoHeight());
    ImageBuffer* imageBufferObject = m_generatedImageCache.imageBuffer(size);
    if (!imageBufferObject)
        return nullptr;
    IntRect destRect(0, 0, size.width(), size.height());
    // FIXME: Turn this into a GPU-GPU texture copy instead of CPU readback.
    video->paintCurrentFrameInContext(imageBufferObject->context(), destRect);
    return imageBufferObject->copyImage();
}

WebCLHTMLInterop::WebCLHTMLInterop(int capacity)
    : m_generatedImageCache(capacity)
{
}

// Caching ImageBuffer
// FIXME :: Reuse WebGL code from WebGLRenderingContext.
WebCLHTMLInterop::LRUImageBufferCache::LRUImageBufferCache(int capacity)
    : m_buffers(std::make_unique<std::unique_ptr<ImageBuffer>[]>(capacity))
    , m_capacity(capacity)
{
}

void WebCLHTMLInterop::LRUImageBufferCache::bubbleToFront(int idx)
{
    for (int i = idx; i > 0; --i)
        m_buffers[i].swap(m_buffers[i-1]);
}

ImageBuffer* WebCLHTMLInterop::LRUImageBufferCache::imageBuffer(const IntSize& size)
{
    int i;
    for (i = 0; i < m_capacity; ++i) {
        ImageBuffer* buf = m_buffers[i].get();
        if (!buf)
            break;
        if (buf->logicalSize() != size)
            continue;
        bubbleToFront(i);
        return buf;
    }

    std::unique_ptr<ImageBuffer> temp = ImageBuffer::create(size, 1);
    if (!temp)
        return nullptr;
    i = std::min(m_capacity - 1, i);
    m_buffers[i] = std::move(temp);

    ImageBuffer* buf = m_buffers[i].get();
    bubbleToFront(i);
    return buf;
}

}
#endif
