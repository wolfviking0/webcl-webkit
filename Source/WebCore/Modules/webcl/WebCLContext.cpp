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

#include "config.h"

#if ENABLE(WEBCL)

#include "WebCLContext.h"

#include "ComputeDevice.h"
#include "ComputePlatform.h"
#include "HTMLImageElement.h"
#include "HTMLVideoElement.h"
#include "ImageData.h"
#include "SharedBuffer.h"
#include "WebCL.h"
#include "WebCLBuffer.h"
#include "WebCLCommandQueue.h"
#include "WebCLEvent.h"
#include "WebCLHTMLInterop.h"
#include "WebCLImage.h"
#include "WebCLImageDescriptor.h"
#include "WebCLMemoryInitializer.h"
#include "WebCLProgram.h"
#include "WebCLSampler.h"
#include "WebCLUserEvent.h"
#if ENABLE(WEBGL)
#include "WebGLBuffer.h"
#include "WebGLRenderbuffer.h"
#include "WebGLRenderingContext.h"
#include "WebGLTexture.h"
#endif

namespace WebCore {

WebCLContext::~WebCLContext()
{
    releasePlatformObject();
}

static inline void getEnabledExtensions(WebCL* webCL, WebCLPlatform* platform, const Vector<RefPtr<WebCLDevice> >& devices, HashSet<String>& enabledExtensions)
{
    webCL->getEnabledExtensions(enabledExtensions);
    platform->getEnabledExtensions(enabledExtensions);

    for (size_t i = 0; i < devices.size(); i++)
        devices[i]->getEnabledExtensions(enabledExtensions);
}

RefPtr<WebCLContext> WebCLContext::create(WebCL* webCL, WebGLRenderingContext* glContext, WebCLPlatform* platform, const Vector<RefPtr<WebCLDevice> >& devices, ExceptionObject& exception)
{
    // Check all the enabled extensions and cache it to avoid enabling after context creation.
    HashSet<String> enabledExtensions;
    getEnabledExtensions(webCL, platform, devices, enabledExtensions);
    // If WebGLRenderingContext is sent and KHR_gl_sharing is not Enabled, throw WEBCL_EXTENSION_NOT_ENABLED.
    if (glContext && !enabledExtensions.contains("KHR_gl_sharing")) {
        setExtensionsNotEnabledException(exception);
        return nullptr;
    }

    size_t CCDeviceMaxWidth = 0, CCDeviceMaxHeight = 0;
    Vector<ComputeDevice*> ccDevices;
    MaximumImageWidthAndHeightForDeviceMap contextDeviceMaxValues;
    CCerror error = ComputeContext::SUCCESS;
    for (size_t i = 0; i < devices.size(); ++i) {
        RefPtr<WebCLDevice> device = devices[i];
        if (!device) {
            setExceptionFromComputeErrorCode(ComputeContext::INVALID_DEVICE, exception);
            return nullptr;
        }

        if (device->platform()->platformObject() != platform->platformObject()) {
            setExceptionFromComputeErrorCode(ComputeContext::INVALID_DEVICE, exception);
            return nullptr;
        }

        error = device->platformObject()->getDeviceInfo(ComputeContext::DEVICE_IMAGE2D_MAX_WIDTH, &CCDeviceMaxWidth);
        if (error != ComputeContext::SUCCESS) {
            setExceptionFromComputeErrorCode(error, exception);
            return nullptr;
        }
        error = device->platformObject()->getDeviceInfo(ComputeContext::DEVICE_IMAGE2D_MAX_HEIGHT, &CCDeviceMaxHeight);
        if (error != ComputeContext::SUCCESS) {
            setExceptionFromComputeErrorCode(error, exception);
            return nullptr;
        }

        contextDeviceMaxValues.add(device.get(), std::make_pair(CCDeviceMaxWidth, CCDeviceMaxHeight));
        ccDevices.append(devices[i]->platformObject());
    }

    ComputePlatform* computePlatform = platform ? platform->platformObject() : 0;
    GraphicsContextGL* graphicsContext3D = glContext ? glContext->graphicsContext3D() : 0;

    RefPtr<ComputeContext> computeContext = ComputeContext::create(ccDevices, computePlatform, graphicsContext3D, error);
    if (error != ComputeContext::SUCCESS) {
        setExceptionFromComputeErrorCode(error, exception);
        return nullptr;
    }

    RefPtr<WebCLContext> context = adoptRef(new WebCLContext(webCL, computeContext, devices, glContext, enabledExtensions, contextDeviceMaxValues));
    return context.release();
}

WebCLContext::WebCLContext(WebCL* webCL, RefPtr<ComputeContext> computeContext, const Vector<RefPtr<WebCLDevice> >& devices, WebGLRenderingContext* glContext, HashSet<String>& enabledExtensions, MaximumImageWidthAndHeightForDeviceMap& contextDeviceMaxValues)
    : WebCLObjectImpl(computeContext)
    , m_webCL(webCL)
    , m_devices(devices)
    , m_glContext(glContext)
    , m_memoryInitializer(this)
    , m_enabledExtensions(enabledExtensions)
    , m_HTMLInteropObject(0)
    , m_contextDeviceMaxValues(contextDeviceMaxValues)
{
    webCL->trackReleaseableWebCLObject(createWeakPtr());
    if (isExtensionEnabled("WEBCL_html_video"))
        m_HTMLInteropObject = new WebCLHTMLInterop(4);
}

WebCLGetInfo WebCLContext::getInfo(CCenum paramName, ExceptionObject& exception)
{
    if (isPlatformObjectNeutralized()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_CONTEXT, exception);
        return WebCLGetInfo();
    }

    switch (paramName) {
    case ComputeContext::CONTEXT_NUM_DEVICES:
        return WebCLGetInfo(static_cast<CCuint>(m_devices.size()));
    case ComputeContext::CONTEXT_DEVICES:
        return WebCLGetInfo(m_devices);
    default:
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_VALUE, exception);
    }

    return WebCLGetInfo();
}

bool WebCLContext::isExtensionEnabled(const String& name) const
{
    return m_enabledExtensions.contains(name);
}

RefPtr<WebCLCommandQueue> WebCLContext::createCommandQueue(WebCLDevice* device, CCenum properties, ExceptionObject& exception)
{
    if (isPlatformObjectNeutralized()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_CONTEXT, exception);
        return nullptr;
    }

    if (!WebCLInputChecker::isValidCommandQueueProperty(properties)) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_VALUE, exception);
        return nullptr;
    }

    RefPtr<WebCLDevice> webCLDevice;
    if (!device) {
        Vector<RefPtr<WebCLDevice> > webCLDevices = m_devices;
        // NOTE: This can be slow, depending the number of 'devices' available.
        size_t i = 0;
        for (i = 0; i < webCLDevices.size(); i++) {
            WebCLGetInfo info = webCLDevices[i]->getInfo(ComputeContext::DEVICE_QUEUE_PROPERTIES, exception);
            if (exception == WebCLException::SUCCESS
                && (info.getUnsignedInt() == static_cast<unsigned>(properties) || !properties)) {
                webCLDevice = webCLDevices[i];
                break;
            }
        }
        if (i == webCLDevices.size()) {
            setExceptionFromComputeErrorCode(ComputeContext::INVALID_DEVICE, exception);
            return nullptr;
        }
        //FIXME: Spec needs to say what we need to do here
        ASSERT(webCLDevice);
    } else {
        size_t i = 0;
        for (i = 0; i < m_devices.size(); i++) {
            if (m_devices[i]->platformObject() == device->platformObject()) {
                webCLDevice = device;
                break;
            }
        }
        if (i == m_devices.size()) {
            setExceptionFromComputeErrorCode(ComputeContext::INVALID_DEVICE, exception);
            return nullptr;
        }
    }

    RefPtr<WebCLCommandQueue> queue = WebCLCommandQueue::create(this, properties, webCLDevice.get(), exception);
    postCreateCommandQueue(queue.get(), exception);
    return queue.release();
}

RefPtr<WebCLProgram> WebCLContext::createProgram(const String& programSource, ExceptionObject& exception)
{
    if (isPlatformObjectNeutralized()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_CONTEXT, exception);
        return nullptr;
    }

    if (!programSource.length()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_VALUE, exception);
        return nullptr;
    }

    return WebCLProgram::create(this, programSource, exception);
}

void WebCLContext::postCreateCommandQueue(WebCLCommandQueue* queue, ExceptionObject& exception)
{
    if (!queue)
        return;

    m_memoryInitializer.commandQueueCreated(queue, exception);
}

void WebCLContext::postCreateBuffer(WebCLBuffer* buffer, ExceptionObject& exception)
{
    if (!buffer)
        return;

    m_memoryInitializer.bufferCreated(buffer, exception);
}

RefPtr<WebCLBuffer> WebCLContext::createBufferBase(CCenum memoryFlags, CCuint size, void* hostPtr, ExceptionObject& exception)
{
    if (isPlatformObjectNeutralized()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_CONTEXT, exception);
        return nullptr;
    }
    if (!WebCLInputChecker::isValidMemoryObjectFlag(memoryFlags)) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_VALUE, exception);
        return nullptr;
    }

    bool needsInitialization = !hostPtr;
    if (hostPtr)
        memoryFlags |= ComputeContext::MEM_COPY_HOST_PTR;

    RefPtr<WebCLBuffer> buffer = WebCLBuffer::create(this, memoryFlags, size, hostPtr, exception);
    if (needsInitialization && buffer) {
        ASSERT(exception == WebCLException::SUCCESS);
        postCreateBuffer(buffer.get(), exception);
    }

    return buffer.release();
}

RefPtr<WebCLBuffer> WebCLContext::createBuffer(CCenum memoryFlags, CCuint size, ArrayBufferView* hostPtr, ExceptionObject& exception)
{
    RefPtr<ArrayBuffer> buffer;
    if (hostPtr) {
        if (hostPtr->byteLength() < size) {
            setExceptionFromComputeErrorCode(ComputeContext::INVALID_HOST_PTR, exception);
            return nullptr;
        }

        if (!hostPtr->buffer()) {
            setExceptionFromComputeErrorCode(ComputeContext::INVALID_HOST_PTR, exception);
            return nullptr;
        }
        buffer = hostPtr->buffer();
    }

    return createBufferBase(memoryFlags, size, buffer ? buffer->data() : 0, exception);
}

RefPtr<WebCLBuffer> WebCLContext::createBuffer(CCenum memoryFlags, ImageData& srcPixelsRef, ExceptionObject& exception)
{
    ImageData* srcPixels = &srcPixelsRef;
    
    if (!isExtensionEnabled("WEBCL_html_image")) {
        setExtensionsNotEnabledException(exception);
        return nullptr;
    }

    void* hostPtr = 0;
    size_t pixelSize = 0;
    WebCLHTMLInterop::extractDataFromImageData(srcPixels, hostPtr, pixelSize, exception);
    if (willThrowException(exception))
        return nullptr;

    return createBufferBase(memoryFlags, pixelSize, hostPtr, exception);
}

RefPtr<WebCLBuffer> WebCLContext::createBuffer(CCenum memoryFlags, HTMLCanvasElement& srcCanvasRef, ExceptionObject& exception)
{
    HTMLCanvasElement* srcCanvas = &srcCanvasRef;
    
    if (!isExtensionEnabled("WEBCL_html_image")) {
        setExtensionsNotEnabledException(exception);
        return nullptr;
    }
    void* hostPtr = 0;
    size_t canvasSize = 0;
    WebCLHTMLInterop::extractDataFromCanvas(srcCanvas, hostPtr, canvasSize, exception);
    if (willThrowException(exception))
        return nullptr;

    return createBufferBase(memoryFlags, canvasSize, hostPtr, exception);
}

RefPtr<WebCLBuffer> WebCLContext::createBuffer(CCenum memoryFlags, HTMLImageElement& srcImageRef, ExceptionObject& exception)
{
    HTMLImageElement* srcImage = &srcImageRef;
    
    if (!isExtensionEnabled("WEBCL_html_image")) {
        setExtensionsNotEnabledException(exception);
        return nullptr;
    }
    void* hostPtr = 0;
    size_t bufferSize = 0;
    WebCLHTMLInterop::extractDataFromImage(srcImage, hostPtr, bufferSize, exception);
    if (willThrowException(exception))
        return nullptr;

    return createBufferBase(memoryFlags, bufferSize, hostPtr, exception);
}

RefPtr<WebCLImage> WebCLContext::createImage2DBase(CCenum flags, CCuint width, CCuint height, CCuint rowPitch, CCuint channelOrder,
    CCuint channelType, void* data, ExceptionObject& exception)
{
    if (isPlatformObjectNeutralized()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_CONTEXT, exception);
        return nullptr;
    }

    if (!width || !height) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_IMAGE_SIZE, exception);
        return nullptr;
    }

    if (!WebCLInputChecker::isValidMemoryObjectFlag(flags)) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_VALUE, exception);
        return nullptr;
    }

    if (!supportsWidthHeight(width, height)) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_IMAGE_SIZE, exception);
        return nullptr;
    }

    ASSERT(data);
    flags |= ComputeContext::MEM_COPY_HOST_PTR;

    RefPtr<WebCLImageDescriptor> imageDescriptor = WebCLImageDescriptor::create(width, height, rowPitch, channelOrder, channelType);
    return WebCLImage::create(this, flags, imageDescriptor.release(), data, exception);
}

RefPtr<WebCLImage> WebCLContext::createImage(CCenum flags, HTMLCanvasElement& srcCanvasRef, ExceptionObject& exception)
{
    HTMLCanvasElement* srcCanvas = &srcCanvasRef;
    
    if (!isExtensionEnabled("WEBCL_html_image")) {
        setExtensionsNotEnabledException(exception);
        return nullptr;
    }
    void* hostPtr = 0;
    size_t canvasSize = 0;
    WebCLHTMLInterop::extractDataFromCanvas(srcCanvas, hostPtr, canvasSize, exception);
    if (willThrowException(exception))
        return nullptr;

    CCuint width = srcCanvas->width();
    CCuint height = srcCanvas->height();
    return createImage2DBase(flags, width, height, 0 /* rowPitch */, ComputeContext::RGBA, ComputeContext::UNORM_INT8, hostPtr, exception);
}

RefPtr<WebCLImage> WebCLContext::createImage(CCenum flags, HTMLImageElement& srcImageRef, ExceptionObject& exception)
{
    HTMLImageElement* srcImage = &srcImageRef;
    
    if (!isExtensionEnabled("WEBCL_html_image")) {
        setExtensionsNotEnabledException(exception);
        return nullptr;
    }
    void* hostPtr = 0;
    size_t bufferSize = 0;
    WebCLHTMLInterop::extractDataFromImage(srcImage, hostPtr, bufferSize, exception);
    if (willThrowException(exception))
        return nullptr;

    CCuint width = srcImage->width();
    CCuint height = srcImage->height();

    return createImage2DBase(flags, width, height, 0 /* rowPitch */, ComputeContext::RGBA, ComputeContext::UNORM_INT8, hostPtr, exception);
}

RefPtr<WebCLImage> WebCLContext::createImage(CCenum flags, HTMLVideoElement& videoRef, ExceptionObject& exception)
{
    HTMLVideoElement* video = &videoRef;
    
    if (!isExtensionEnabled("WEBCL_html_video")) {
        setExtensionsNotEnabledException(exception);
        return nullptr;
    }

    if (ComputeContext::MEM_READ_ONLY != flags) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_VALUE, exception);
        return nullptr;
    }

    void* hostPtr = 0;
    size_t videoSize = 0;
    m_HTMLInteropObject->extractDataFromVideo(video, hostPtr, videoSize, exception);
    if (willThrowException(exception))
        return nullptr;

    CCuint width =  video->videoWidth();
    CCuint height = video->videoHeight();
    return createImage2DBase(flags, width, height, 0 /* rowPitch */, ComputeContext::RGBA, ComputeContext::UNORM_INT8, hostPtr, exception);
}

RefPtr<WebCLImage> WebCLContext::createImage(CCenum flags, ImageData& srcPixelsRef, ExceptionObject& exception)
{
    ImageData* srcPixels = &srcPixelsRef;
    
    if (!isExtensionEnabled("WEBCL_html_image")) {
        setExtensionsNotEnabledException(exception);
        return nullptr;
    }
    void* hostPtr = 0;
    size_t pixelSize = 0;
    WebCLHTMLInterop::extractDataFromImageData(srcPixels, hostPtr, pixelSize, exception);
    if (willThrowException(exception))
        return nullptr;

    CCuint width = srcPixels->width();
    CCuint height = srcPixels->height();
    return createImage2DBase(flags, width, height, 0 /* rowPitch */, ComputeContext::RGBA, ComputeContext::UNORM_INT8, hostPtr, exception);
}

unsigned WebCLContext::numberOfChannelsForChannelOrder(CCenum order)
{
    switch(order) {
    case ComputeContext::R:
    case ComputeContext::A:
    case ComputeContext::INTENSITY:
    case ComputeContext::LUMINANCE:
        return 1;
    case ComputeContext::RG:
    case ComputeContext::RA:
    case ComputeContext::Rx:
        return 2;
    case ComputeContext::RGB:
    case ComputeContext::RGx:
        return 3;
    case ComputeContext::RGBA:
    case ComputeContext::BGRA:
    case ComputeContext::ARGB:
    case ComputeContext::RGBx:
        return 4;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

unsigned WebCLContext::bytesPerChannelType(CCenum channelType)
{
    switch(channelType) {
    case ComputeContext::SNORM_INT8:
    case ComputeContext::UNORM_INT8:
    case ComputeContext::SIGNED_INT8:
    case ComputeContext::UNSIGNED_INT8:
        return 1;
    case ComputeContext::SNORM_INT16:
    case ComputeContext::UNORM_INT16:
    case ComputeContext::SIGNED_INT16:
    case ComputeContext::UNSIGNED_INT16:
    case ComputeContext::HALF_FLOAT:
        return 2;
    case ComputeContext::SIGNED_INT32:
    case ComputeContext::UNSIGNED_INT32:
    case ComputeContext::FLOAT:
        return 4;
    case ComputeContext::UNORM_SHORT_565:
    case ComputeContext::UNORM_SHORT_555:
    case ComputeContext::UNORM_INT_101010:
        break;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

RefPtr<WebCLImage> WebCLContext::createImage(CCenum flags, WebCLImageDescriptor* descriptor, ArrayBufferView* hostPtr, ExceptionObject& exception)
{
    if (!descriptor) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_IMAGE_FORMAT_DESCRIPTOR, exception);
        return nullptr;
    }

    CCuint width =  descriptor->width();
    CCuint height = descriptor->height();
    CCuint rowPitch = descriptor->rowPitch();
    CCenum channelOrder = descriptor->channelOrder();
    CCenum channelType = descriptor->channelType();

    if (!WebCLInputChecker::isValidChannelOrder(channelOrder) || !WebCLInputChecker::isValidChannelType(channelType)) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_IMAGE_FORMAT_DESCRIPTOR, exception);
        return nullptr;
    }
    unsigned numberOfChannels = numberOfChannelsForChannelOrder(channelOrder);
    unsigned bytesPerChannel = bytesPerChannelType(channelType);

    // If rowPitch is specified, must be hostPtr != 0 & rowPitch > width * bytesPerPixel.
    if (rowPitch && (!hostPtr || rowPitch < (width * numberOfChannels * bytesPerChannel))) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_IMAGE_SIZE, exception);
        return nullptr;
    }

    RefPtr<ArrayBuffer> buffer;
    if (hostPtr) {
        unsigned byteLength = hostPtr->byteLength();
        // Validate user sent rowPitch to avoid out of bound access of hostPtr.
        // When rowPitch = 0, rowPitch = width * bytes/pixel, which will be checked by 2nd expression.
        if ((rowPitch && byteLength < (rowPitch * height))
            || byteLength < (width * height * numberOfChannels * bytesPerChannel)) {
            setExceptionFromComputeErrorCode(ComputeContext::INVALID_HOST_PTR, exception);
            return nullptr;
        }
        buffer = hostPtr->buffer();
    } else {
        // FIXME :: This is a slow initialization method. Need to verify against kernel init method.
        buffer = ArrayBuffer::create(width * height * numberOfChannels * bytesPerChannel, 1);
    }

    return createImage2DBase(flags, width, height, rowPitch, channelOrder, channelType, buffer->data(), exception);
}

RefPtr<WebCLSampler> WebCLContext::createSampler(CCbool normCoords, CCenum addressingMode, CCenum filterMode, ExceptionObject& exception)
{
    if (isPlatformObjectNeutralized()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_CONTEXT, exception);
        return nullptr;
    }

    if (!WebCLInputChecker::isValidAddressingMode(addressingMode)) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_VALUE, exception);
        return nullptr;
    }

    if (!WebCLInputChecker::isValidFilterMode(filterMode)) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_VALUE, exception);
        return nullptr;
    }

    if (!normCoords && (addressingMode == ComputeContext::ADDRESS_REPEAT || addressingMode == ComputeContext::ADDRESS_MIRRORED_REPEAT)) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_VALUE, exception);
        return nullptr;
    }

    return WebCLSampler::create(this, normCoords, addressingMode, filterMode, exception);
}

RefPtr<WebCLUserEvent> WebCLContext::createUserEvent(ExceptionObject& exception)
{
    if (isPlatformObjectNeutralized()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_CONTEXT, exception);
        return nullptr;
    }

    return WebCLUserEvent::create(this, exception);
}

Vector<RefPtr<WebCLImageDescriptor> > WebCLContext::getSupportedImageFormats(ExceptionObject &exception)
{
    return getSupportedImageFormats(ComputeContext::MEM_READ_WRITE, exception);
}

Vector<RefPtr<WebCLImageDescriptor> > WebCLContext::getSupportedImageFormats(CCenum memoryFlags, ExceptionObject &exception)
{
    Vector<RefPtr<WebCLImageDescriptor> > imageDescriptors;
    if (isPlatformObjectNeutralized()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_CONTEXT, exception);
        return imageDescriptors;
    }

    if (!WebCLInputChecker::isValidMemoryObjectFlag(memoryFlags)) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_VALUE, exception);
        return imageDescriptors;
    }

    Vector<CCImageFormat> supportedImageFormats;
    CCerror error = platformObject()->supportedImageFormats(memoryFlags, ComputeContext::MEM_OBJECT_IMAGE2D, supportedImageFormats);

    if (error != ComputeContext::SUCCESS) {
        setExceptionFromComputeErrorCode(error, exception);
        return imageDescriptors;
    }

    CCuint numberOfSupportedImages = supportedImageFormats.size();
    for (size_t i = 0; i < numberOfSupportedImages; ++i)
        imageDescriptors.append(WebCLImageDescriptor::create(supportedImageFormats[i]));
    return imageDescriptors;
}

WebGLRenderingContext* WebCLContext::getGLContext(ExceptionObject& exception) const
{
    if (!isGLCapableContext()) {
        setExtensionsNotEnabledException(exception);
        return nullptr;
    }
    return m_glContext;
}

bool WebCLContext::isGLCapableContext() const
{
    if (!isExtensionEnabled("KHR_gl_sharing"))
        return false;

    return !!m_glContext;
}

void WebCLContext::trackReleaseableWebCLObject(WeakPtr<WebCLObject> object)
{
    m_descendantWebCLObjects.append(object);
}

void WebCLContext::releaseAll()
{
    if (isPlatformObjectNeutralized())
        return;

    for (size_t i = 0; i < m_descendantWebCLObjects.size(); ++i) {
        WebCLObject* object = m_descendantWebCLObjects.at(i).get();
        if (!object)
            continue;

        object->release();
    }

    release();
}

// Public method to be used in enqueue* API's.
bool WebCLContext::supportsWidthHeight(CCuint width, CCuint height, RefPtr<WebCLDevice> device) const
{
    return (width <= m_contextDeviceMaxValues.get(device.get()).first)
        && (height <= m_contextDeviceMaxValues.get(device.get()).second);
}

// Private method to check if {width, height} can be accomodated in any of the context devices.
bool WebCLContext::supportsWidthHeight(CCuint width, CCuint height) const
{
    for (MaximumImageWidthAndHeightForDeviceMap::const_iterator i = m_contextDeviceMaxValues.begin(); i != m_contextDeviceMaxValues.end(); ++i) {
        if (width <= i->value.first && height <= i->value.second)
            return true;
    }
    return false;
}

#if ENABLE(WEBGL)
RefPtr<WebCLBuffer> WebCLContext::createFromGLBuffer(CCenum flags, WebGLBuffer& webGLBufferRef, ExceptionObject& exception)
{
    WebGLBuffer* webGLBuffer = &webGLBufferRef;
    
    if (!isGLCapableContext()) {
        setExtensionsNotEnabledException(exception);
        return nullptr;
    }

    if (isPlatformObjectNeutralized()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_CONTEXT, exception);
        return nullptr;
    }

    if (!webGLBuffer || !webGLBuffer->object()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_GL_OBJECT, exception);
        return nullptr;
    }

    if (!WebCLInputChecker::isValidMemoryObjectFlag(flags)) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_VALUE, exception);
        return nullptr;
    }

    return WebCLBuffer::create(this, flags, webGLBuffer, exception);
}

RefPtr<WebCLImage> WebCLContext::createFromGLRenderbuffer(CCenum flags, WebGLRenderbuffer& renderbufferRef, ExceptionObject& exception)
{
    WebGLRenderbuffer* renderbuffer = &renderbufferRef;
    
    if (!isGLCapableContext()) {
        setExtensionsNotEnabledException(exception);
        return nullptr;
    }

    if (isPlatformObjectNeutralized()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_CONTEXT, exception);
        return nullptr;
    }

    if (!renderbuffer || !renderbuffer->getWidth() || !renderbuffer->getHeight()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_GL_OBJECT, exception);
        return nullptr;
    }

    if (!WebCLInputChecker::isValidMemoryObjectFlag(flags)) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_VALUE, exception);
        return nullptr;
    }

    return WebCLImage::create(this, flags, renderbuffer, exception);
}

RefPtr<WebCLImage> WebCLContext::createFromGLTexture(CCenum flags, CCenum textureTarget, GCGLint miplevel, WebGLTexture& textureRef, ExceptionObject& exception)
{
    WebGLTexture* texture = &textureRef;
    
    if (!isGLCapableContext()) {
        setExtensionsNotEnabledException(exception);
        return nullptr;
    }

    if (isPlatformObjectNeutralized()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_CONTEXT, exception);
        return nullptr;
    }

    if (!WebCLInputChecker::isValidGLTextureTarget(textureTarget)) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_VALUE, exception);
        return nullptr;
    }
    if (miplevel < 0) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_MIP_LEVEL, exception);
        return nullptr;
    }

    if (!texture || textureTarget != texture->getTarget()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_GL_OBJECT, exception);
        return nullptr;
    }

    if (!WebCLInputChecker::isValidMemoryObjectFlag(flags)) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_VALUE, exception);
        return nullptr;
    }
    return WebCLImage::create(this, flags, textureTarget, miplevel, texture, exception);
}
#endif

} // namespace WebCore

#endif // ENABLE(WEBCL)
