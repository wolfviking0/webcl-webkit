/*
 * Copyright (C) 2011, 2012, 2013 Samsung Electronics Corporation. All rights reserved.
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

#ifndef WebCLContext_h
#define WebCLContext_h

#if ENABLE(WEBCL)

#include "WebCLCommandQueue.h"
#include "WebCLDevice.h"
#include "WebCLGetInfo.h"
#include "WebCLInputChecker.h"
#include "WebCLMemoryInitializer.h"
#include "WebCLMemoryObject.h"
#include "WebCLObject.h"
#include "WebCLProgram.h"
#include <runtime/ArrayBufferView.h>
#include <wtf/HashSet.h>

namespace WebCore {

class HTMLCanvasElement;
class HTMLImageElement;
class HTMLVideoElement;
class ImageBuffer;
class ImageData;
class WebCL;
class WebCLBuffer;
class WebCLCommandQueue;
class WebCLDevice;
class WebCLEvent;
class WebCLImage;
class WebCLImageDescriptor;
class WebCLSampler;
class WebCLUserEvent;
class WebCLHTMLInterop;
#if ENABLE(WEBGL)
class WebGLBuffer;
class WebGLRenderingContext;
class WebGLRenderbuffer;
class WebGLTexture;
#endif

// NOTE: WebCLObject used by WebCLContext is a bit different, because the
// other WebCL classes have as platformObject() a native opencl type. However
// WebCLContext has as platformObject() an abstraction called ComputeContext

class WebCLContext : public WebCLObjectImpl<ComputeContext> {
public:
    virtual ~WebCLContext();

    static RefPtr<WebCLContext> create(WebCL*, WebGLRenderingContext*, WebCLPlatform*, const Vector<RefPtr<WebCLDevice> >&, ExceptionObject&);

    RefPtr<WebCLCommandQueue> createCommandQueue(WebCLDevice*, CCenum commandQueueProperty, ExceptionObject&);

    RefPtr<WebCLImage> createImage(CCenum flag, WebCLImageDescriptor*, ArrayBufferView*, ExceptionObject&);

    RefPtr<WebCLProgram> createProgram(const String& programSource, ExceptionObject&);

    RefPtr<WebCLSampler> createSampler(CCbool normalizedCoords, CCenum addressingMode, CCenum filterMode, ExceptionObject&);

    RefPtr<WebCLUserEvent> createUserEvent(ExceptionObject&);

    WebCLGetInfo getInfo(CCenum flag, ExceptionObject&);

    Vector<RefPtr<WebCLImageDescriptor> > getSupportedImageFormats(ExceptionObject&);
    Vector<RefPtr<WebCLImageDescriptor> > getSupportedImageFormats(CCenum memFlag, ExceptionObject&);

    RefPtr<WebCLBuffer> createBuffer(CCenum memFlags, CCuint sizeInBytes, ArrayBufferView*, ExceptionObject&);
    RefPtr<WebCLBuffer> createBuffer(CCenum memFlag, ImageData&, ExceptionObject&);
    RefPtr<WebCLBuffer> createBuffer(CCenum memFlag, HTMLCanvasElement&, ExceptionObject&);
    RefPtr<WebCLBuffer> createBuffer(CCenum memFlag, HTMLImageElement&, ExceptionObject&);

    RefPtr<WebCLImage> createImage(CCenum memFlag, ImageData&, ExceptionObject&);
    RefPtr<WebCLImage> createImage(CCenum memFlag, HTMLCanvasElement&, ExceptionObject&);
    RefPtr<WebCLImage> createImage(CCenum memFlag, HTMLImageElement&, ExceptionObject&);
    RefPtr<WebCLImage> createImage(CCenum memFlag, HTMLVideoElement&, ExceptionObject&);

#if ENABLE(WEBGL)
    RefPtr<WebCLBuffer> createFromGLBuffer(CCenum, WebGLBuffer&, ExceptionObject&);
    RefPtr<WebCLImage> createFromGLRenderbuffer(CCenum, WebGLRenderbuffer&, ExceptionObject&);
    RefPtr<WebCLImage> createFromGLTexture(CCenum memoryFlags, CCenum textureTarget, GCGLint miplevel, WebGLTexture&, ExceptionObject&);
#endif

    ComputeContext* computeContext() const { return platformObject(); }

    void trackReleaseableWebCLObject(WeakPtr<WebCLObject>);
    void releaseAll();
    static unsigned bytesPerChannelType(CCenum);
    static unsigned numberOfChannelsForChannelOrder(CCenum);

    const Vector<RefPtr<WebCLDevice> >& devices() const
    {
        return m_devices;
    }
    WebGLRenderingContext* getGLContext(ExceptionObject&) const;
    bool isGLCapableContext() const;

    WebCLHTMLInterop* getInteropObject() const
    {
        return m_HTMLInteropObject;
    }
    bool supportsWidthHeight(CCuint width, CCuint height, RefPtr<WebCLDevice>) const;

private:
    typedef HashMap<WebCLDevice*, std::pair<CCuint, CCuint> > MaximumImageWidthAndHeightForDeviceMap;
    WebCLContext(WebCL*, RefPtr<ComputeContext>, const Vector<RefPtr<WebCLDevice> >&, WebGLRenderingContext*, HashSet<String>&, MaximumImageWidthAndHeightForDeviceMap&);

    RefPtr<WebCLImage> createImage2DBase(CCenum flags, CCuint width, CCuint height, CCuint rowPitch, CCuint channelOrder, CCuint channelType, void*, ExceptionObject&);
    RefPtr<WebCLBuffer> createBufferBase(CCenum memoryFlags, CCuint size, void* data, ExceptionObject&);

    bool isExtensionEnabled(const String& name) const;
    bool supportsWidthHeight(CCuint width, CCuint height) const;

    friend bool WebCLCommandQueue::isExtensionEnabled(WebCLContext*, const String&) const;
    friend bool WebCLMemoryObject::isExtensionEnabled(WebCLContext*, const String&) const;
    friend bool WebCLProgram::isExtensionEnabled(WebCLContext*, const String&);

    void postCreateCommandQueue(WebCLCommandQueue*, ExceptionObject&);
    void postCreateBuffer(WebCLBuffer*, ExceptionObject&);

    WebCL* m_webCL;
    Vector<RefPtr<WebCLDevice> > m_devices;
    WebGLRenderingContext* m_glContext;

    Vector<WeakPtr<WebCLObject> > m_descendantWebCLObjects;

    WebCLMemoryInitializer m_memoryInitializer;
    HashSet<String> m_enabledExtensions;
    WebCLHTMLInterop* m_HTMLInteropObject;
    MaximumImageWidthAndHeightForDeviceMap m_contextDeviceMaxValues;
};

} // namespace WebCore

#endif // ENABLE(WEBCL)
#endif // WebCLContext_h
