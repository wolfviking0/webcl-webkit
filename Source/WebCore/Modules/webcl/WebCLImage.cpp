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

#include "WebCLImage.h"

#include "ComputeMemoryObject.h"
#include "WebCLContext.h"
#include "WebCLImageDescriptor.h"
#include "WebGLRenderbuffer.h"
#include "WebGLTexture.h"

namespace WebCore {

WebCLImage::~WebCLImage()
{
}

PassRefPtr<WebCLImage> WebCLImage::create(WebCLContext* context, CCenum flags, PassRefPtr<WebCLImageDescriptor> imageDescriptor, void* data, ExceptionObject& exception)
{
    CCerror error;
    PassRefPtr<ComputeMemoryObject> memoryObject = context->computeContext()->createImage2D(flags, imageDescriptor->width(),
        imageDescriptor->height(), imageDescriptor->rowPitch(), imageDescriptor->imageFormat(), data, error);
    if (error != ComputeContext::SUCCESS) {
        setExceptionFromComputeErrorCode(error, exception);
        return nullptr;
    }

    return adoptRef(new WebCLImage(context, memoryObject, imageDescriptor));
}

#if ENABLE(WEBGL)
PassRefPtr<WebCLImage> WebCLImage::create(WebCLContext* context, CCenum flags, WebGLRenderbuffer* webGLRenderbuffer, ExceptionObject& exception)
{
    ASSERT(webGLRenderbuffer);

    GLuint renderbufferID = webGLRenderbuffer->object();
    if (!renderbufferID) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_GL_OBJECT, exception);
        return nullptr;
    }

    GC3Dsizei width = webGLRenderbuffer->getWidth();
    GC3Dsizei height = webGLRenderbuffer->getHeight();

    CCerror error;
    PassRefPtr<ComputeMemoryObject> memoryObject = context->computeContext()->createFromGLRenderbuffer(flags, renderbufferID, error);
    if (error != ComputeContext::SUCCESS) {
        setExceptionFromComputeErrorCode(error, exception);
        return nullptr;
    }

    // FIXME: Format is wrong here. It should have been gotten from WebGLRenderbuffer as well.

    RefPtr<WebCLImageDescriptor> imageDescriptor = WebCLImageDescriptor::create(width, height);
    RefPtr<WebCLImage> imageObject = adoptRef(new WebCLImage(context, memoryObject, imageDescriptor.release()));
    imageObject->cacheGLObjectInfo(ComputeContext::GL_OBJECT_RENDERBUFFER, 0 /*textureTarget*/, 0 /*miplevel*/, webGLRenderbuffer);
    return imageObject.release();
}

PassRefPtr<WebCLImage> WebCLImage::create(WebCLContext* context, CCenum flags, CCenum textureTarget, CCenum miplevel,
                                           WebGLTexture* webGLTexture, ExceptionObject& exception)
{
    ASSERT(webGLTexture);
    GLuint textureID = webGLTexture->object();
    if (!textureID) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_GL_OBJECT, exception);
        return nullptr;
    }

    GC3Dsizei width = webGLTexture->getWidth(textureTarget, miplevel);
    GC3Dsizei height = webGLTexture->getHeight(textureTarget, miplevel);

    CCerror error;
    PassRefPtr<ComputeMemoryObject> memoryObject = context->computeContext()->createFromGLTexture2D(flags, textureTarget, miplevel, textureID, error);
    if (error != ComputeContext::SUCCESS) {
        setExceptionFromComputeErrorCode(error, exception);
        return nullptr;
    }

    // FIXME: Format is wrong here. It should have been gotten from WebGLTexture as well.
    RefPtr<WebCLImageDescriptor> imageDescriptor = WebCLImageDescriptor::create(width, height);
    RefPtr<WebCLImage> imageObject = adoptRef(new WebCLImage(context, memoryObject, imageDescriptor));
    imageObject->cacheGLObjectInfo(ComputeContext::GL_OBJECT_TEXTURE2D, textureTarget, miplevel, webGLTexture);
    return imageObject.release();
}

int WebCLImage::getGLTextureInfo(CCenum textureInfoType, ExceptionObject& exception)
{
    if (isPlatformObjectNeutralized()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_MEM_OBJECT, exception);
        return 0;
    }

    if (!WebCLInputChecker::isValidGLTextureInfo(textureInfoType)) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_VALUE, exception);
        return 0;
    }

    // FIXME: Is a WebCLImage was created from a WebGLRenderbuffer instead of WebGLTexture,
    // what should happen here?
    // http://www.khronos.org/bugzilla/show_bug.cgi?id=940

    CCint err = 0;
    switch (textureInfoType) {
    case ComputeContext::GL_TEXTURE_TARGET:
    case ComputeContext::GL_MIPMAP_LEVEL: {
        CCint glTextureInfo = 0;
        err = platformObject()->getGLTextureInfo(textureInfoType, &glTextureInfo);
        if (err == ComputeContext::SUCCESS)
            return glTextureInfo;
        break;
    }
    }

    ASSERT(err != ComputeContext::SUCCESS);
    setExceptionFromComputeErrorCode(err, exception);
    return 0;
}

void WebCLImage::cacheGLObjectInfo(CCenum type, int textureTarget, int mipmapLevel, WebGLObject* glObject)
{
    m_objectInfo = WebCLGLObjectInfo::create(type, textureTarget, mipmapLevel, glObject);
}
#endif

WebCLImage::WebCLImage(WebCLContext* context, PassRefPtr<ComputeMemoryObject> image, PassRefPtr<WebCLImageDescriptor> imageDescriptor)
    : WebCLMemoryObject(context, image, 0 /* sizeInBytes */)
    , m_imageDescriptor(imageDescriptor)
{
    size_t memorySizeValue = 0;
    CCint err = platformObject()->getMemoryObjectInfo(ComputeContext::MEM_SIZE, &memorySizeValue);
    if (err == ComputeContext::SUCCESS)
        m_sizeInBytes = memorySizeValue;

    size_t actualRowPitch = 0;
    err = platformObject()->getImageInfo(ComputeContext::IMAGE_ROW_PITCH, &actualRowPitch);
    if (err == ComputeContext::SUCCESS)
        m_imageDescriptor->setRowPitch(actualRowPitch);

}

WebCLImageDescriptor* WebCLImage::getInfo(ExceptionObject& exception)
{
    if (isPlatformObjectNeutralized()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_MEM_OBJECT, exception);
        return nullptr;
    }

    return m_imageDescriptor.get();
}

const CCImageFormat& WebCLImage::imageFormat() const
{
    return m_imageDescriptor->imageFormat();
}

} // namespace WebCore

#endif // ENABLE(WEBCL)
