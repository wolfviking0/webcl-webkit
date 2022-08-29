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

#ifndef WebCLKernelArgInfo_h
#define WebCLKernelArgInfo_h

#if ENABLE(WEBCL)

#include <wtf/NeverDestroyed.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class WebCLKernelArgInfo : public RefCounted<WebCLKernelArgInfo> {
public:
    enum WebCLKernelTypes {
        CHAR,
        UCHAR,
        SHORT,
        USHORT,
        INT,
        UINT,
        LONG,
        ULONG,
        FLOAT,
        DOUBLE,
        HALF,
        BUFFER,
        SAMPLER,
        IMAGE,
        UNKNOWN
    };

    static PassRefPtr<WebCLKernelArgInfo> create(const String& addressQualifier, const String& accessQualifier, const String& type, const String& name, const bool isPointerType = false) { return adoptRef(new WebCLKernelArgInfo(addressQualifier, accessQualifier, type, name, isPointerType)); }

    String name() const { return m_name; }
    String typeName() const { return m_type; }
    String addressQualifier() const { return m_addressQualifier; }
    String accessQualifier() const { return m_accessQualifier; }
    WebCLKernelTypes type() const { return m_types; }
    bool hasLocalAddressQualifier() const { return m_hasLocalAddressQualifier; }

private:
    WebCLKernelArgInfo(const String& addressQualifier, const String& accessQualifier, const String& type, const String& name, const bool isPointerType)
        : m_addressQualifier(addressQualifier)
        , m_accessQualifier(accessQualifier)
        , m_type(type)
        , m_name(name)
    {
        m_types = extractTypeEnum(m_type, isPointerType);
        m_hasLocalAddressQualifier = (m_addressQualifier == "local");
    }
    String m_addressQualifier;
    String m_accessQualifier;
    String m_type;
    String m_name;
    WebCLKernelTypes m_types;
    bool m_hasLocalAddressQualifier;

    static inline WebCLKernelTypes extractTypeEnum(const String& typeName, bool isPointerType)
    {
        if (isPointerType)
            return WebCLKernelTypes::BUFFER;

        static NeverDestroyed<AtomicString> image2d_t("image2d_t");
        if (typeName == image2d_t)
            return WebCLKernelTypes::IMAGE;

        static NeverDestroyed<AtomicString> sampler_t("sampler_t");
        if (typeName == sampler_t)
            return WebCLKernelTypes::SAMPLER;

        static NeverDestroyed<AtomicString> ucharLiteral("uchar");
        if (typeName.contains(ucharLiteral.get()))
            return WebCLKernelTypes::UCHAR;
        static NeverDestroyed<AtomicString> charLiteral("char");
        if (typeName.contains(charLiteral.get()))
            return WebCLKernelTypes::CHAR;

        static NeverDestroyed<AtomicString> ushortLiteral("ushort");
        if (typeName.contains(ushortLiteral.get()))
            return WebCLKernelTypes::USHORT;
        static NeverDestroyed<AtomicString> shortLiteral("short");
        if (typeName.contains(shortLiteral.get()))
            return WebCLKernelTypes::SHORT;

        static NeverDestroyed<AtomicString> uintLiteral("uint");
        if (typeName.contains(uintLiteral.get()))
            return WebCLKernelTypes::UINT;
        static NeverDestroyed<AtomicString> intLiteral("int");
        if (typeName.contains(intLiteral.get()))
            return WebCLKernelTypes::INT;

        static NeverDestroyed<AtomicString> ulongLiteral("ulong");
        if (typeName.contains(ulongLiteral.get()))
            return WebCLKernelTypes::ULONG;
        static NeverDestroyed<AtomicString> longLiteral("long");
        if (typeName.contains(longLiteral.get()))
            return WebCLKernelTypes::LONG;

        static NeverDestroyed<AtomicString> floatLiteral("float");
        if (typeName.contains(floatLiteral.get()))
            return WebCLKernelTypes::FLOAT;

        static NeverDestroyed<AtomicString> doubleLiteral("double");
        if (typeName.contains(doubleLiteral.get()))
            return WebCLKernelTypes::DOUBLE;

        static NeverDestroyed<AtomicString> halfLiteral("half");
        if (typeName.contains(halfLiteral.get()))
            return WebCLKernelTypes::HALF;

        return WebCLKernelTypes::UNKNOWN;
    }

};
} // namespace WebCore

#endif
#endif // WebCLKernelArgInfo_h
