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

#include "WebCLKernelArgInfoProvider.h"

#include "WebCLKernel.h"
#include "WebCLProgram.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static bool isASCIILineBreakCharacter(UChar c)
{
    return c == '\r' || c == '\n';
}

static bool isStarCharacter(UChar c)
{
    return c == '*';
}

inline bool isEmptySpace(UChar character)
{
    return character <= ' ' && (character == ' ' || character == '\n' || character == '\t' || character == '\r' || character == '\f');
}

inline bool isPrecededByUnderscores(const String& string, size_t index)
{
    if (index < 2)
        return false;
    return string[index - 1] == '_' && string[index - 2] == '_';
}

WebCLKernelArgInfoProvider::WebCLKernelArgInfoProvider(WebCLKernel* kernel)
    : m_kernel(kernel)
{
    ASSERT(kernel);
}

const Vector<RefPtr<WebCLKernelArgInfo> >& WebCLKernelArgInfoProvider::argumentsInfo()
{
    ensureInfo();
    return m_argumentInfoVector;
}

void WebCLKernelArgInfoProvider::ensureInfo()
{
    if (m_argumentInfoVector.size())
        return;

    const String& source = m_kernel->program()->sourceWithCommentsStripped();

    // 0) find "kernel" string.
    // 1) Check if it is a valid kernel declaration.
    // 2) find the first open braces past "kernel".
    // 3) reverseFind the given kernel name string.
    // 4) if not found go back to (1)
    // 5) if found, parse its argument list.

    size_t kernelNameIndex = 0;
    size_t kernelDeclarationIndex = 0;
    for (size_t startIndex = 0; ; startIndex = kernelDeclarationIndex + 6) {

        kernelDeclarationIndex = source.find("kernel", startIndex);
        if (kernelDeclarationIndex == WTF::notFound)
            CRASH();

        // Check if "kernel" is not a substring of a valid token,
        // e.g. "akernel" or "__kernel_":
        // 1) After "kernel" there has to be an empty space.
        // 2) Before "kernel" there has to be either:
        // 2.1) two underscore characters or
        // 2.2) none, i.e. "kernel" is the first string in the program source or
        // 2.3) an empty space.
        if (!isEmptySpace(source[kernelDeclarationIndex + 6]))
            continue;

        // If the kernel declaration is not at the beginning of the program.
        bool hasTwoUnderscores = isPrecededByUnderscores(source, kernelDeclarationIndex);
        bool isKernelDeclarationAtBeginning = hasTwoUnderscores ? (kernelDeclarationIndex == 2): (kernelDeclarationIndex == 0);

        if (!isKernelDeclarationAtBeginning) {
             size_t firstPrecedingIndex = kernelDeclarationIndex - (hasTwoUnderscores ? 3 : 1);
             if (!isEmptySpace(source[firstPrecedingIndex]))
                 continue;
        }

        size_t openBrace = source.find("{", kernelDeclarationIndex + 6);
        kernelNameIndex = source.reverseFind(m_kernel->kernelName(), openBrace);

        if (kernelNameIndex < kernelDeclarationIndex)
            continue;

        if (kernelNameIndex != WTF::notFound)
            break;
    }

    ASSERT(kernelNameIndex);
    size_t openBraket = source.find("(", kernelNameIndex);
    size_t closeBraket = source.find(")", openBraket);
    String argumentListStr = source.substring(openBraket + 1, closeBraket - openBraket - 1);

    Vector<String> argumentStrVector;
    argumentListStr.split(",", argumentStrVector);
    for (size_t i = 0; i < argumentStrVector.size(); ++i) {
        argumentStrVector[i] = argumentStrVector[i].removeCharacters(isASCIILineBreakCharacter);
        argumentStrVector[i] = argumentStrVector[i].stripWhiteSpace();
        parseAndAppendDeclaration(argumentStrVector[i]);
    }
}

static void prependUnsignedIfNeeded(Vector<String>& declarationStrVector, String& type)
{
    for (size_t i = 0; i < declarationStrVector.size(); ++i) {
        static NeverDestroyed<AtomicString> Unsigned("unsigned");
        if (declarationStrVector[i] == Unsigned.get()) {
            type = "u" + type;
            declarationStrVector.remove(i);
            return;
        }
    }
}

void WebCLKernelArgInfoProvider::parseAndAppendDeclaration(const String& argumentDeclaration)
{
    // "*" is used to indicate pointer data type, setting isPointerType flag if "*" is present in argumentDeclaration.
    // Since we parse only valid & buildable OpenCL kernels, * in argumentDeclaration must be associated with type only.
    bool isPointerType = false;
    if (argumentDeclaration.contains("*"))
        isPointerType = true;

    Vector<String> declarationStrVector;
    // As per the specification, remove the "*" from pointer data-types and then spilt the argumentDeclaration.
    argumentDeclaration.removeCharacters(isStarCharacter).split(" ", declarationStrVector);
    String name = extractName(declarationStrVector);
    String type = extractType(declarationStrVector);
    String addressQualifier = extractAddressQualifier(declarationStrVector);

    static NeverDestroyed<AtomicString> image2d_t("image2d_t");
    String accessQualifier = (type == image2d_t.get()) ? extractAccessQualifier(declarationStrVector) : "none";
    prependUnsignedIfNeeded(declarationStrVector, type);

    m_argumentInfoVector.append(WebCLKernelArgInfo::create(addressQualifier, accessQualifier, type, name, isPointerType));
}

String WebCLKernelArgInfoProvider::extractAddressQualifier(Vector<String>& declarationStrVector)
{
    static NeverDestroyed<AtomicString> __Private("__private");
    static NeverDestroyed<AtomicString> Private("private");

    static NeverDestroyed<AtomicString> __Global("__global");
    static NeverDestroyed<AtomicString> Global("global");

    static NeverDestroyed<AtomicString> __Constant("__constant");
    static NeverDestroyed<AtomicString> Constant("constant");

    static NeverDestroyed<AtomicString> __Local("__local");
    static NeverDestroyed<AtomicString> Local("local");

    String address = Private.get();
    size_t i = 0;
    for ( ; i < declarationStrVector.size(); ++i) {
        String candidate = declarationStrVector[i];
        if (candidate == __Private.get() || candidate == Private.get())
            break;
        if (candidate == __Global.get() || candidate == Global.get()) {
            address = Global.get();
            break;
        }
        if (candidate == __Constant.get() || candidate == Constant.get()) {
            address = Constant.get();
            break;
        }
        if (candidate == __Local.get() || candidate == Local.get()) {
            address = Local.get();
            break;
        }
    }

    if (i < declarationStrVector.size())
        declarationStrVector.remove(i);

    return address;
}

String WebCLKernelArgInfoProvider::extractAccessQualifier(Vector<String>& declarationStrVector)
{
    static NeverDestroyed<AtomicString> __read_only("__read_only");
    static NeverDestroyed<AtomicString> read_only("read_only");

    static NeverDestroyed<AtomicString> __write_only("__write_only");
    static NeverDestroyed<AtomicString> write_only("write_only");

    static NeverDestroyed<AtomicString> __read_write("__read_write");
    static NeverDestroyed<AtomicString> read_write("read_write");

    String access = read_only.get();
    size_t i = 0;
    for ( ; i < declarationStrVector.size(); ++i) {
        String candidate = declarationStrVector[i];
        if (candidate == __read_only.get()  || candidate == read_only.get())
            break;
        if (candidate == __write_only.get() || candidate == write_only.get()) {
            access = write_only.get();
            break;
        }
        if (candidate == __read_write.get() || candidate == read_write.get()) {
            access = read_write.get();
            break;
        }
    }

    if (i < declarationStrVector.size())
        declarationStrVector.remove(i);

    return access;
}

String WebCLKernelArgInfoProvider::extractName(Vector<String>& declarationStrVector)
{
    String last = declarationStrVector.last();
    declarationStrVector.removeLast();
    return last;
}

String WebCLKernelArgInfoProvider::extractType(Vector<String>& declarationStrVector)
{
    String type = declarationStrVector.last();
    declarationStrVector.removeLast();
    return type;
}

} // namespace WebCore

#endif // ENABLE(WEBCL)
