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
#include "WebCLKernel.h"

#include "ComputeDevice.h"
#include "WebCLBuffer.h"
#include "WebCLCommandQueue.h"
#include "WebCLContext.h"
#include "WebCLException.h"
#include "WebCLGetInfo.h"
#include "WebCLImage.h"
#include "WebCLImageDescriptor.h"
#include "WebCLProgram.h"
#include "WebCLSampler.h"

#include <runtime/ArrayBufferView.h>
#include <runtime/Float32Array.h>
#include <runtime/Float64Array.h>
#include <runtime/Int16Array.h>
#include <runtime/Int32Array.h>
#include <runtime/Int8Array.h>
#include <runtime/Uint16Array.h>
#include <runtime/Uint32Array.h>
#include <runtime/Uint8Array.h>

namespace WebCore {

#if CPU(BIG_ENDIAN)
inline void swapElementsForBigEndian(size_t& arrayLength, ArrayBufferView* bufferView, Vector<CCulong>& uLongBuffer)
{
    for (size_t i = 0; i < arrayLength * 2; i += 2) {
        CCuint low, high;
        low = static_cast<Uint32Array*>(bufferView)->item(i);
        high = static_cast<Uint32Array*>(bufferView)->item(i+1);
        uLongBuffer[i / 2] = ((CCulong)low << 32) | high;
    }
}
#endif

WebCLKernel::~WebCLKernel()
{
    releasePlatformObject();
}

PassRefPtr<WebCLKernel> WebCLKernel::create(WebCLContext* context, WebCLProgram* program, const String& kernelName, ExceptionObject& exception)
{
    CCerror error = ComputeContext::SUCCESS;
    RefPtr<ComputeKernel> computeKernel = program->computeProgram()->createKernel(kernelName, error);
    if (error != ComputeContext::SUCCESS) {
        setExceptionFromComputeErrorCode(error, exception);
        return nullptr;
    }
    return adoptRef(new WebCLKernel(context, program, computeKernel, kernelName));
}

// FIXME: Move to WebCLProgram class.
Vector<RefPtr<WebCLKernel> > WebCLKernel::createKernelsInProgram(WebCLContext* context, WebCLProgram* program, ExceptionObject& exception)
{
    CCerror error = ComputeContext::SUCCESS;
    Vector<RefPtr<ComputeKernel>> computeKernels = program->computeProgram()->createKernelsInProgram(error);
    Vector<RefPtr<WebCLKernel>> kernels;
    if (error != ComputeContext::SUCCESS) {
        setExceptionFromComputeErrorCode(error, exception);
        return kernels;
    }

    Vector<char> functionName;
    for (size_t i = 0 ; i < computeKernels.size(); i++) {
        error = computeKernels[i]->getKernelInfo(ComputeContext::KERNEL_FUNCTION_NAME, &functionName);
        if (error != ComputeContext::SUCCESS) {
            setExceptionFromComputeErrorCode(error, exception);
            kernels.clear();
            return kernels;
        }

        kernels.append(adoptRef(new WebCLKernel(context, program, computeKernels[i].release(), String(functionName.data()))));
    }

    return kernels;
}

WebCLKernel::WebCLKernel(WebCLContext* context, WebCLProgram* program, PassRefPtr<ComputeKernel> kernel, const String& kernelName)
    : WebCLObjectImpl(kernel)
    , m_context(context)
    , m_program(program)
    , m_kernelName(kernelName)
    , m_argumentInfoProvider(this)
{
    context->trackReleaseableWebCLObject(createWeakPtr());
}

WebCLGetInfo WebCLKernel::getInfo(CCenum kernelInfo, ExceptionObject& exception)
{
    if (isPlatformObjectNeutralized()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_KERNEL, exception);
        return WebCLGetInfo();
    }

    CCerror err = ComputeContext::SUCCESS;
    switch (kernelInfo) {
    case ComputeContext::KERNEL_FUNCTION_NAME:
        return WebCLGetInfo(m_kernelName);
    case ComputeContext::KERNEL_NUM_ARGS: {
        CCuint numberOfArgs = 0;
        err = platformObject()->getKernelInfo(kernelInfo, &numberOfArgs);
        if (err == ComputeContext::SUCCESS)
            return WebCLGetInfo(static_cast<CCuint>(numberOfArgs));
        break;
    }
    case ComputeContext::KERNEL_PROGRAM:
        return WebCLGetInfo(m_program.get());
    case ComputeContext::KERNEL_CONTEXT:
        return WebCLGetInfo(m_context.get());
    default:
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_VALUE, exception);
        return WebCLGetInfo();
    }

    ASSERT(err != ComputeContext::SUCCESS);
    setExceptionFromComputeErrorCode(err, exception);
    return WebCLGetInfo();
}

WebCLGetInfo WebCLKernel::getWorkGroupInfo(WebCLDevice* device, CCenum paramName, ExceptionObject& exception)
{
    if (isPlatformObjectNeutralized()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_KERNEL, exception);
        return WebCLGetInfo();
    }

    if (!device && m_context->devices().size() != 1) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_DEVICE, exception);
        return WebCLGetInfo();
    }

    ComputeDevice* ccDevice = device ? device->platformObject() : nullptr;

    CCerror err = 0;
    switch (paramName) {
    case ComputeContext::KERNEL_WORK_GROUP_SIZE:
    case ComputeContext::KERNEL_PRIVATE_MEM_SIZE:
    case ComputeContext::KERNEL_LOCAL_MEM_SIZE:
    case ComputeContext::KERNEL_PREFERRED_WORK_GROUP_SIZE_MULTIPLE: {
        size_t kernelInfo = 0;
        err = platformObject()->getWorkGroupInfo(ccDevice, paramName, &kernelInfo);
        if (err == ComputeContext::SUCCESS)
            return WebCLGetInfo(static_cast<CCuint>(kernelInfo));
        break;
    }
    case ComputeContext::KERNEL_COMPILE_WORK_GROUP_SIZE: {
        Vector<size_t> workGroupSize;
        err = platformObject()->getWorkGroupInfo(ccDevice, paramName, &workGroupSize);
        if (err == ComputeContext::SUCCESS) {
            Vector<CCuint, 3> values;
            for (size_t i = 0; i < workGroupSize.size(); i++)
                values.uncheckedAppend(workGroupSize[i]);
            return WebCLGetInfo(values);
        }
        break;
    }
    default:
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_VALUE, exception);
        return WebCLGetInfo();
    }

    ASSERT(err != ComputeContext::SUCCESS);
    setExceptionFromComputeErrorCode(err, exception);
    return WebCLGetInfo();
}

typedef WebCLKernelArgInfo::WebCLKernelTypes kernelTypeEnum;
void WebCLKernel::setArg(CCuint index, WebCLBuffer* buffer, ExceptionObject& exception)
{
    if (isPlatformObjectNeutralized()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_KERNEL, exception);
        return;
    }

    if (!WebCLInputChecker::isValidKernelArgIndex(this, index)) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_ARG_INDEX, exception);
        return;
    }

    if (!WebCLInputChecker::validateWebCLObject(buffer)) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_MEM_OBJECT, exception);
        return;
    }

    WebCLKernelArgInfo* argInfo = getArgInfo(index, exception);
    // OpenCL mandates addressQualifier & "*" for cl_mem AKA buffer inputs.
    if (!argInfo || argInfo->addressQualifier().isEmpty() || argInfo->type() != kernelTypeEnum::BUFFER) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_ARG_VALUE, exception);
        return;
    }

    CCerror err = platformObject()->setKernelArg(index, buffer->platformObject());
    setExceptionFromComputeErrorCode(err, exception);
}

void WebCLKernel::setArg(CCuint index, WebCLImage* image, ExceptionObject& exception)
{
    if (isPlatformObjectNeutralized()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_KERNEL, exception);
        return;
    }

    if (!WebCLInputChecker::validateWebCLObject(image)) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_MEM_OBJECT, exception);
        return;
    }

    if (!WebCLInputChecker::isValidKernelArgIndex(this, index)) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_ARG_INDEX, exception);
        return;
    }

    WebCLKernelArgInfo* argInfo = getArgInfo(index, exception);
    if (!argInfo || argInfo->type() != kernelTypeEnum::IMAGE) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_ARG_VALUE, exception);
        return;
    }

    CCerror err = platformObject()->setKernelArg(index, image->platformObject());
    setExceptionFromComputeErrorCode(err, exception);
}
void WebCLKernel::setArg(CCuint index, WebCLSampler* sampler, ExceptionObject& exception)
{
    if (isPlatformObjectNeutralized()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_KERNEL, exception);
        return;
    }

    if (!WebCLInputChecker::validateWebCLObject(sampler)) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_SAMPLER, exception);
        return;
    }

    if (!WebCLInputChecker::isValidKernelArgIndex(this, index)) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_ARG_INDEX, exception);
        return;
    }

    WebCLKernelArgInfo* argInfo = getArgInfo(index, exception);
    if (!argInfo || argInfo->type() != kernelTypeEnum::SAMPLER) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_ARG_VALUE, exception);
        return;
    }

    ComputeSampler* computeSampler = sampler->platformObject();
    CCerror err = platformObject()->setKernelArg(index, computeSampler);
    setExceptionFromComputeErrorCode(err, exception);
}

void WebCLKernel::setArg(CCuint index, ArrayBufferView* bufferView, ExceptionObject& exception)
{
    if (isPlatformObjectNeutralized()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_KERNEL, exception);
        return;
    }

    if (!bufferView) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_ARG_VALUE, exception);
        return;
    }

    if (!WebCLInputChecker::isValidKernelArgIndex(this, index)) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_ARG_INDEX, exception);
        return;
    }

    WebCLKernelArgInfo* argInfo = getArgInfo(index, exception);
    if (argInfo->hasLocalAddressQualifier()) {
        if (bufferView->getType() != JSC::TypeUint32) {
            setExceptionFromComputeErrorCode(ComputeContext::INVALID_ARG_VALUE, exception);
            return;
        }

        Uint32Array* typedArray = static_cast<Uint32Array*>(bufferView);
        if (typedArray->length() != 1) {
            setExceptionFromComputeErrorCode(ComputeContext::INVALID_ARG_VALUE, exception);
            return;
        }

        unsigned* value = static_cast<Uint32Array*>(bufferView)->data();
        CCerror err = platformObject()->setKernelArg(index, static_cast<size_t>(value[0]), 0 /* __local required null'ed data */);
        setExceptionFromComputeErrorCode(err, exception);
        return;
    }

    void* bufferData = 0;
    size_t arrayLength = 0;
    Vector<CCulong> uLongBuffer;
    kernelTypeEnum type = argInfo->type();

    // FIXME: Add support for LONG, ULONG, HALF and DOUBLE types.
    // These need Int/Uint64Array, as well as Float16Array.
    switch (bufferView->getType()) {
    case JSC::TypeFloat64: // double
        if (type != kernelTypeEnum::DOUBLE) {
            setExceptionFromComputeErrorCode(ComputeContext::INVALID_ARG_VALUE, exception);
            return;
        }
        bufferData = static_cast<Float64Array*>(bufferView)->data();
        arrayLength = bufferView->byteLength() / 8;
        break;

    case JSC::TypeFloat32: // FLOAT
        if (type != kernelTypeEnum::FLOAT) {
            setExceptionFromComputeErrorCode(ComputeContext::INVALID_ARG_VALUE, exception);
            return;
        }
        bufferData = static_cast<Float32Array*>(bufferView)->data();
        arrayLength = bufferView->byteLength() / 4;
        break;
    case JSC::TypeUint32: // UINT
        if (!(type == kernelTypeEnum::ULONG || type == kernelTypeEnum::UINT)) {
            setExceptionFromComputeErrorCode(ComputeContext::INVALID_ARG_VALUE, exception);
            return;
        }
        bufferData = static_cast<Uint32Array*>(bufferView)->data();
        arrayLength = bufferView->byteLength() / 4;
        // For Long data type, input

        /* WebCL spec says 64-bit integers must be represented as pairs of 32-bit
           unsigned integers. The low-order 32 bits are stored in the first element of
           each pair, and the high-order 32 bits in the second element. As little endian
           architecture follows the same format, it is automatically taken care of. For
           Big endian architecture, order for 32 bit uint elements need to be swapped
           before passing to OpenCL. */

        if (type == kernelTypeEnum::ULONG) {
            arrayLength = arrayLength / 2;
#if CPU(BIG_ENDIAN)
            uLongBuffer.resize(arrayLength);
            swapElementsForBigEndian(arrayLength, bufferView, uLongBuffer);
            bufferData = uLongBuffer.data();
#endif
        }
        break;
    case JSC::TypeInt32: // INT
        if (!(type == kernelTypeEnum::LONG || type == kernelTypeEnum::INT)) {
            setExceptionFromComputeErrorCode(ComputeContext::INVALID_ARG_VALUE, exception);
            return;
        }
        bufferData = static_cast<Int32Array*>(bufferView)->data();
        arrayLength = bufferView->byteLength() / 4;
        if (type == kernelTypeEnum::LONG)
            arrayLength = arrayLength / 2;
        break;
    case JSC::TypeUint16: // USHORT
        if (type != kernelTypeEnum::USHORT) {
            setExceptionFromComputeErrorCode(ComputeContext::INVALID_ARG_VALUE, exception);
            return;
        }
        bufferData = static_cast<Uint16Array*>(bufferView)->data();
        arrayLength = bufferView->byteLength() / 2;
        break;
    case JSC::TypeInt16: // SHORT
        if (type != kernelTypeEnum::SHORT) {
            setExceptionFromComputeErrorCode(ComputeContext::INVALID_ARG_VALUE, exception);
            return;
        }
        bufferData = static_cast<Int16Array*>(bufferView)->data();
        arrayLength = bufferView->byteLength() / 2;
        break;
    case JSC::TypeUint8: // UCHAR
        if (type != kernelTypeEnum::UCHAR) {
            setExceptionFromComputeErrorCode(ComputeContext::INVALID_ARG_VALUE, exception);
            return;
        }
        bufferData = static_cast<Uint8Array*>(bufferView)->data();
        arrayLength = bufferView->byteLength() / 1;
        break;
    case JSC::TypeInt8: // CHAR
        if (type != kernelTypeEnum::CHAR) {
            setExceptionFromComputeErrorCode(ComputeContext::INVALID_ARG_VALUE, exception);
            return;
        }
        bufferData = static_cast<Int8Array*>(bufferView)->data();
        arrayLength = bufferView->byteLength() / 1;
        break;
    default:
        ASSERT_NOT_REACHED();
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_ARG_VALUE, exception);
        return;
    }

    if (!isValidVectorLength(argInfo->typeName(), arrayLength)) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_ARG_VALUE, exception);
        return;
    }
    size_t bufferDataSize = bufferView->byteLength();
    CCerror err = platformObject()->setKernelArg(index, bufferDataSize, bufferData);
    setExceptionFromComputeErrorCode(err, exception);
}

WebCLKernelArgInfo* WebCLKernel::getArgInfo(CCuint index, ExceptionObject& exception)
{
    if (isPlatformObjectNeutralized()) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_KERNEL, exception);
        return nullptr;
    }

    if (!WebCLInputChecker::isValidKernelArgIndex(this, index)) {
        setExceptionFromComputeErrorCode(ComputeContext::INVALID_ARG_INDEX, exception);
        return nullptr; 
    }

    return m_argumentInfoProvider.argumentsInfo().at(index).get();
}

WebCLProgram* WebCLKernel::program() const
{
    return m_program.get();
}

String WebCLKernel::kernelName() const
{
    return m_kernelName;
}

unsigned WebCLKernel::numberOfArguments()
{
    return m_argumentInfoProvider.argumentsInfo().size();
}

bool WebCLKernel::isValidVectorLength(String typeName, size_t arrayLength)
{
    // FIXME :: Any better way to parse vector length from TypeName ?
    if (arrayLength > 1 && !typeName.contains(String::number(arrayLength)))
        return false;

    switch (arrayLength) {
    case 1:
    case 2:
    case 3:
    case 4:
    case 8:
    case 16:
    case 32:
        return true;
    }
    return false;
}

} // namespace WebCore

#endif // ENABLE(WEBCL)
