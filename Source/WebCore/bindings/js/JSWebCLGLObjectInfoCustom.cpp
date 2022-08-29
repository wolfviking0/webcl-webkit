/*
 * Copyright (C) 2013 Samsung Electronics Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEBCL) && ENABLE(WEBGL)

#include "JSWebCLGLObjectInfo.h"

#include "ComputeContext.h"
#include "JSWebGLBuffer.h"
#include "JSWebGLRenderbuffer.h"
#include "JSWebGLTexture.h"
#include "JSDOMBinding.h"

namespace WebCore {

class WebGLBuffer;
class WebGLRenderbuffer;
class WebGLTexture;

JSC::JSValue JSWebCLGLObjectInfo::glObject(JSC::ExecState* exec) const
{
    WebCLGLObjectInfo& info = impl();

    switch(info.type()) {
    case ComputeContext::GL_OBJECT_BUFFER:
        return toJS(exec, globalObject(), static_cast<WebGLBuffer*>(info.glObject()));
    case ComputeContext::GL_OBJECT_TEXTURE2D:
        return toJS(exec, globalObject(), static_cast<WebGLTexture*>(info.glObject()));
    case ComputeContext::GL_OBJECT_RENDERBUFFER:
        return toJS(exec, globalObject(), static_cast<WebGLRenderbuffer*>(info.glObject()));
    }

    ASSERT_NOT_REACHED();
    return JSC::jsNull();
}

JSC::JSValue JSWebCLGLObjectInfo::textureTarget(JSC::ExecState*) const
{
    WebCLGLObjectInfo& info = impl();

    if (info.type() == ComputeContext::GL_OBJECT_TEXTURE2D)
        return JSC::jsNumber(info.textureTarget());

    return JSC::jsUndefined();
}

JSC::JSValue JSWebCLGLObjectInfo::mipmapLevel(JSC::ExecState*) const
{
    WebCLGLObjectInfo& info = impl();

    if (info.type() == ComputeContext::GL_OBJECT_TEXTURE2D)
        return JSC::jsNumber(info.mipmapLevel());

    return JSC::jsUndefined();
}

} // namespace WebCore

#endif // ENABLE(WEBCL)
