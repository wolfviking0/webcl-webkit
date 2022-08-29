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

#include "JSWebCLCustom.h"

#include "JSWebCL.h"
#include "JSWebCLCommandQueue.h"
#include "JSWebCLDevice.h"
#include "JSWebCLImageDescriptor.h"
#include "JSWebCLPlatform.h"
#include "JSWebCLProgram.h"
#include "JSWebGLRenderingContext.h"
#include "WebCLGetInfo.h"
#include "WebCLContext.h"

using namespace JSC;

namespace WebCore {

class WebCLGetInfo;

JSValue toJS(ExecState* exec, JSDOMGlobalObject* globalObject, const WebCLGetInfo& info)
{
    switch (info.getType()) {
    case WebCLGetInfo::kTypeBool:
        return jsBoolean(info.getBool());
    case WebCLGetInfo::kTypeInt:
        return jsNumber(info.getInt());
    case WebCLGetInfo::kTypeNull:
        return jsNull();
    case WebCLGetInfo::kTypeString:
        return jsString(exec, info.getString());
    case WebCLGetInfo::kTypeUnsignedInt:
        return jsNumber(info.getUnsignedInt());
    case WebCLGetInfo::kTypeUnsignedLong:
        return jsNumber(info.getUnsignedLong());
    case WebCLGetInfo::kTypeWebCLIntArray: {
        MarkedArgumentBuffer list;
        Vector<CCuint> value = info.getWebCLUintArray();
        for (size_t i = 0; i < value.size(); ++i)
            list.append(jsNumber(value[i]));
        return constructArray(exec, 0, globalObject, list);
    }
    case WebCLGetInfo::kTypeWebCLImageDescriptor:
        return toJS(exec, globalObject, info.getWebCLImageDescriptor());
    case WebCLGetInfo::kTypeWebCLProgram:
        return toJS(exec, globalObject, info.getWebCLProgram());
    case WebCLGetInfo::kTypeWebCLContext:
        return toJS(exec, globalObject, info.getWebCLContext());
    case WebCLGetInfo::kTypeWebCLCommandQueue:
        return toJS(exec, globalObject, info.getWebCLCommandQueue());
    case WebCLGetInfo::kTypeWebCLDevice:
        return toJS(exec, globalObject, info.getWebCLDevice());
    case WebCLGetInfo::kTypeWebCLDevices:
        return jsArray(exec, globalObject, info.getWebCLDevices());
    case WebCLGetInfo::kTypeWebCLMemoryObject:
        return toJS(exec, globalObject, info.getWebCLMemoryObject());
    case WebCLGetInfo::kTypeWebCLPlatform:
        return toJS(exec, globalObject, info.getWebCLPlatform());
    default:
        return jsUndefined();
    }
}

} // namespace WebCore

#endif // ENABLE(WEBCL)
