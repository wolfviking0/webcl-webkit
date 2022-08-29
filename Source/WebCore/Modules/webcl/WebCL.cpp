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
#include "WebCL.h"

#include "ComputeEvent.h"
#include "WebCLCallback.h"
#include "WebCLContext.h"
#include "WebCLDevice.h"
#include "WebCLEvent.h"
#include <wtf/MainThread.h>

using namespace JSC;

namespace WebCore {

PassRefPtr<WebCL> WebCL::create()
{
    return adoptRef(new WebCL);
}

WebCL::WebCL()
    : WebCLExtensionsAccessor(0)
{
}

Vector<RefPtr<WebCLPlatform> > WebCL::getPlatforms(ExceptionObject& exception)
{
    if (m_platforms.size())
        return m_platforms;

    CCerror error = WebCore::getPlatforms(m_platforms);
    if (error != ComputeContext::SUCCESS) {
        setExceptionFromComputeErrorCode(error, exception);
        m_platforms.clear();
        return m_platforms;
    }
    return m_platforms;
}

static inline void validateWebCLEventList(const Vector<RefPtr<WebCLEvent> >& events, ExceptionObject& exception, bool isSyncCall)
{
    if (!events.size()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_VALUE, exception);
        return;
    }

    // So if the event being waited on has not been initialized (1) or is an user
    // event and call is synchronous (2), we would hang the browser.
    // Also as per [http://www.khronos.org/bugzilla/show_bug.cgi?id=1127], if a event is already being waited for(isWaitedOn())
    // throw INVALID_EVENT_WAIT_LIST.
    if (events[0]->isPlatformObjectNeutralized()
        || !events[0]->holdsValidCLObject()
        || (events[0]->isUserEvent() && isSyncCall)
        || events[0]->isWaitedOn()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_EVENT_WAIT_LIST, exception);
        return;
    }
    CCint status = 0;
    if (events[0]->platformObject()->getEventInfo(ComputeContext::EVENT_COMMAND_EXECUTION_STATUS, &status) != ComputeContext::SUCCESS || status < 0) {
        setExceptionFromComputeErrorCode(ComputeContext::EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST, exception);
        return;
    }

    ASSERT(events[0]->context());
    WebCLContext* referenceContext = events[0]->context();

    for (size_t i = 1; i < events.size(); ++i) {
        if (events[i]->isPlatformObjectNeutralized()
            || !events[i]->holdsValidCLObject()
            || (events[i]->isUserEvent() && isSyncCall)
            || events[i]->isWaitedOn()) {
            setExceptionFromComputeErrorCode(ComputeContext::INVALID_EVENT_WAIT_LIST, exception);
            return;
        }
        ASSERT(events[i]->context());
        if (!WebCLInputChecker::compareContext(events[i]->context(), referenceContext)) {
            setExceptionFromComputeErrorCode(ComputeContext::INVALID_CONTEXT, exception);
            return;
        }
        if (events[i]->platformObject()->getEventInfo(ComputeContext::EVENT_COMMAND_EXECUTION_STATUS, &status) != ComputeContext::SUCCESS || status < 0) {
            setExceptionFromComputeErrorCode(ComputeContext::EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST, exception);
            return;
        }
    }
    // Blocking the events at this point till the OpenCL clWaitForEvents() returns on new Thread.
    // Need to block in main thread function to avoid delay caused in spawning a new Thread.
    for (size_t i = 0; i < events.size(); ++i)
        events[i]->setIsBeingWaitedOn(true);
}

void WebCL::callbackProxyOnMainThread(void* userData)
{
    WebCLCallback* callback = static_cast<WebCLCallback*>(userData);
    ASSERT(callback);
    callback->handleEvent();
    callbackRegisterQueue().remove(callback);
}

void WebCL::threadStarterWebCL(void* data)
{
    WebCLCallback* callback = static_cast<WebCLCallback*>(data);
    ASSERT(callback);
    // On the new Thread, wait for waitForEventsImpl() AKA OpenCL clWaitForEvents to complete.
    // And then call the callback method on the Main Thread.

    Vector<RefPtr<WebCLEvent> > webCLEvents = callbackRegisterQueue().get(callback);
    ExceptionObject exception;
    waitForEventsImpl(webCLEvents, exception);
    // FIXME :: Exception from OpenCL is lost. [http://www.khronos.org/bugzilla/show_bug.cgi?id=1164]
    if (willThrowException(exception)) {
        callbackRegisterQueue().remove(callback);
        return;
    }

    if (!isMainThread()) {
        callOnMainThread(callbackProxyOnMainThread, data);
        return;
    }
    callbackProxyOnMainThread(data);
}

void WebCL::waitForEventsImpl(const Vector<RefPtr<WebCLEvent> >& webCLEvents, ExceptionObject& exception)
{
    Vector<ComputeEvent*> computeEvents;
    for (size_t i = 0; i < webCLEvents.size(); ++i)
        computeEvents.append(webCLEvents[i]->platformObject());

    CCerror error = ComputeContext::waitForEvents(computeEvents);
    setExceptionFromComputeErrorCode(error, exception);
    for (size_t i = 0; i < webCLEvents.size(); i++)
        webCLEvents[i]->setIsBeingWaitedOn(false);
}

void WebCL::waitForEvents(const Vector<RefPtr<WebCLEvent> >& events, PassRefPtr<WebCLCallback> callback, ExceptionObject& exception)
{
    validateWebCLEventList(events, exception, !callback);
    if (willThrowException(exception))
        return;

    if (callback) {
        // Store the callback, eventList to HashTable and call threadStarterWebCL.
        RefPtr<WebCLCallback> cb = callback;
        callbackRegisterQueue().set(cb, events);
        WTF::ThreadIdentifier threadID = createThread(WebCL::threadStarterWebCL, cb.get(), "webCLWaitForEvents");
        detachThread(threadID);
    } else
        waitForEventsImpl(events, exception);
}

PassRefPtr<WebCLContext> WebCL::createContext(ExceptionObject& exception)
{
    return createContext(ComputeContext::DEVICE_TYPE_DEFAULT, exception);
}

PassRefPtr<WebCLContext> WebCL::createContext(CCenum deviceType, ExceptionObject& exception)
{
    getPlatforms(exception);
    if (willThrowException(exception))
        return nullptr;

    return createContext(m_platforms[0].get(), deviceType, exception);
}

PassRefPtr<WebCLContext> WebCL::createContext(WebCLPlatform* platform, ExceptionObject& exception)
{
    return createContext(platform, ComputeContext::DEVICE_TYPE_DEFAULT, exception);
}

PassRefPtr<WebCLContext> WebCL::createContext(WebCLPlatform* platform, CCenum deviceType, ExceptionObject& exception)
{
    if (!platform) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_PLATFORM, exception);
        return nullptr;
    }

    if (!WebCLInputChecker::isValidDeviceType(deviceType)) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_DEVICE_TYPE, exception);
        return nullptr;
    }

    Vector<RefPtr<WebCLDevice> > devices = platform->getDevices(deviceType, exception);
    if (willThrowException(exception))
        return nullptr;

    return WebCLContext::create(this, 0 /*glContext*/, platform, devices, exception);
}

PassRefPtr<WebCLContext> WebCL::createContext(const Vector<RefPtr<WebCLDevice> >& devices, ExceptionObject& exception)
{
    if (!devices.size()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_VALUE, exception);
        return nullptr;
    }
    return WebCLContext::create(this, 0 /*glContext*/, devices[0]->platform(), devices, exception);
}

PassRefPtr<WebCLContext> WebCL::createContext(WebCLDevice* device, ExceptionObject& exception)
{
    Vector<RefPtr<WebCLDevice>, 1> devices;
    devices.uncheckedAppend(device);

    return createContext(devices, exception);
}

PassRefPtr<WebCLContext> WebCL::createContext(WebGLRenderingContext* glContext, ExceptionObject& exception)
{
    return createContext(glContext, ComputeContext::DEVICE_TYPE_DEFAULT, exception);
}

PassRefPtr<WebCLContext> WebCL::createContext(WebGLRenderingContext* glContext, CCenum deviceType, ExceptionObject& exception)
{
    if (!WebCLInputChecker::isValidDeviceType(deviceType)) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_DEVICE_TYPE, exception);
        return nullptr;
    }

    getPlatforms(exception);
    if (willThrowException(exception))
        return nullptr;
    // FIXME: Pick a platform that best suites to cl-gl instead of any.
    return createContext(glContext, m_platforms[0].get(), deviceType, exception);
}

PassRefPtr<WebCLContext> WebCL::createContext(WebGLRenderingContext* glContext, WebCLPlatform* platform, ExceptionObject& exception)
{
    // FIXME: Pick a device that best suits to cl-gl instead of the default one.
    return createContext(glContext, platform, ComputeContext::DEVICE_TYPE_DEFAULT, exception);
}

PassRefPtr<WebCLContext> WebCL::createContext(WebGLRenderingContext* glContext, WebCLPlatform* platform, CCenum deviceType, ExceptionObject& exception)
{
    if (!platform) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_PLATFORM, exception);
        return nullptr;
    }

    if (!WebCLInputChecker::isValidDeviceType(deviceType)) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_DEVICE_TYPE, exception);
        return nullptr;
    }

    // FIXME: Pick devices that best suits to cl-gl instead of all.
    Vector<RefPtr<WebCLDevice> > devices = platform->getDevices(deviceType, exception);
    if (willThrowException(exception))
        return nullptr;

    return WebCLContext::create(this, glContext, platform, devices, exception);
}

PassRefPtr<WebCLContext> WebCL::createContext(WebGLRenderingContext* glContext, const Vector<RefPtr<WebCLDevice> >& devices, ExceptionObject& exception)
{
    if (!devices.size()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_DEVICE, exception);
        return nullptr;
    }
    return WebCLContext::create(this, glContext, devices[0]->platform(), devices, exception);
}

PassRefPtr<WebCLContext> WebCL::createContext(WebGLRenderingContext* glContext, WebCLDevice* device, ExceptionObject& exception)
{
    Vector<RefPtr<WebCLDevice>, 1> devices;
    devices.uncheckedAppend(device);

    return createContext(glContext, devices, exception);
}

void WebCL::trackReleaseableWebCLObject(WeakPtr<WebCLObject> object)
{
    m_descendantWebCLObjects.append(object);
}

void WebCL::releaseAll()
{
    for (size_t i = 0; i < m_descendantWebCLObjects.size(); ++i) {
        WebCLObject* object = m_descendantWebCLObjects.at(i).get();
        if (!object)
            continue;

        WebCLContext* context = static_cast<WebCLContext*>(object);
        context->releaseAll();
    }
}

} // namespace WebCore

#endif // ENABLE(WEBCL)
