//===------------------------- ItaniumDemangle.cpp ------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

// FIXME: (possibly) incomplete list of features that clang mangles that this
// file does not yet support:
//   - C++ modules TS

#include <demangler/Demangle.h>
#include <demangler/ItaniumDemangle.h>

#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <utility>

using namespace llvm;
using namespace llvm::itanium_demangle;

// <discriminator> := _ <non-negative number>      # when number < 10
//                 := __ <non-negative number> _   # when number >= 10
//  extension      := decimal-digit+               # at the end of string
const char *itanium_demangle::parse_discriminator(const char *first,
    const char *last)
{
    // parse but ignore discriminator
    if (first != last)
    {
        if (*first == '_')
        {
            const char *t1 = first + 1;
            if (t1 != last)
            {
                if (std::isdigit(*t1))
                    first = t1 + 1;
                else if (*t1 == '_')
                {
                    for (++t1; t1 != last && std::isdigit(*t1); ++t1)
                        ;
                    if (t1 != last && *t1 == '_')
                        first = t1 + 1;
                }
            }
        }
        else if (std::isdigit(*first))
        {
            const char *t1 = first + 1;
            for (; t1 != last && std::isdigit(*t1); ++t1)
                ;
            if (t1 == last)
                first = last;
        }
    }
    return first;
}

namespace
{
    class BumpPointerAllocator
    {
        struct BlockMeta
        {
            BlockMeta *Next;
            size_t Current;
        };

        static constexpr size_t AllocSize = 4096;
        static constexpr size_t UsableAllocSize = AllocSize - sizeof(BlockMeta);

        alignas(long double) char InitialBuffer[AllocSize];
        BlockMeta *BlockList = nullptr;

        void grow()
        {
            char *NewMeta = static_cast<char *>(std::malloc(AllocSize));
            if (NewMeta == nullptr)
                std::terminate();
            BlockList = new (NewMeta) BlockMeta{ BlockList, 0 };
        }

        void *allocateMassive(size_t NBytes)
        {
            NBytes += sizeof(BlockMeta);
            BlockMeta *NewMeta = reinterpret_cast<BlockMeta *>(std::malloc(NBytes));
            if (NewMeta == nullptr)
                std::terminate();
            BlockList->Next = new (NewMeta) BlockMeta{ BlockList->Next, 0 };
            return static_cast<void *>(NewMeta + 1);
        }

    public:
        BumpPointerAllocator() :
            BlockList(new(InitialBuffer) BlockMeta{ nullptr, 0 }) { }

        void *allocate(size_t N)
        {
            N = (N + 15u) & ~15u;
            if (N + BlockList->Current >= UsableAllocSize)
            {
                if (N > UsableAllocSize)
                    return allocateMassive(N);
                grow();
            }
            BlockList->Current += N;
            return static_cast<void *>(reinterpret_cast<char *>(BlockList + 1) + BlockList->Current - N);
        }

        void reset()
        {
            while (BlockList)
            {
                BlockMeta *Tmp = BlockList;
                BlockList = BlockList->Next;
                if (reinterpret_cast<char *>(Tmp) != InitialBuffer)
                    std::free(Tmp);
            }
            BlockList = new (InitialBuffer) BlockMeta{ nullptr, 0 };
        }

        ~BumpPointerAllocator()
        {
            reset();
        }
    };

    class DefaultAllocator
    {
        BumpPointerAllocator Alloc;

    public:
        void reset()
        {
            Alloc.reset();
        }

        template<typename T, typename... Args>
        T *makeNode(Args &&...args)
        {
            return new (Alloc.allocate(sizeof(T)))
                T(std::forward<Args>(args)...);
        }

        void *allocateNodeArray(size_t sz)
        {
            return Alloc.allocate(sizeof(Node *) * sz);
        }
    };
} // unnamed namespace

//===----------------------------------------------------------------------===//
// Code beyond this point should not be synchronized with libc++abi.
//===----------------------------------------------------------------------===//

using Demangler = itanium_demangle::ManglingParser<DefaultAllocator>;

char *llvm::itaniumDemangle(const char *MangledName, char *Buf,
    size_t *N, int *Status)
{
    if (MangledName == nullptr || (Buf != nullptr && N == nullptr))
    {
        if (Status)
            *Status = demangle_invalid_args;
        return nullptr;
    }

    int InternalStatus = demangle_success;
    Demangler Parser(MangledName, MangledName + std::strlen(MangledName));
    Node *AST = Parser.parse();

    if (AST == nullptr)
        InternalStatus = demangle_invalid_mangled_name;
    else
    {
        OutputBuffer OB(Buf, N);
        assert(Parser.ForwardTemplateRefs.empty());
        AST->print(OB);
        OB += '\0';
        if (N != nullptr)
            *N = OB.getCurrentPosition();
        Buf = OB.getBuffer();
    }

    if (Status)
        *Status = InternalStatus;
    return InternalStatus == demangle_success ? Buf : nullptr;
}

ItaniumPartialDemangler::ItaniumPartialDemangler() :
    RootNode(nullptr), Context(new Demangler{ nullptr, nullptr }) { }

ItaniumPartialDemangler::~ItaniumPartialDemangler()
{
    delete static_cast<Demangler *>(Context);
}

ItaniumPartialDemangler::ItaniumPartialDemangler(
    ItaniumPartialDemangler &&Other) :
    RootNode(Other.RootNode),
    Context(Other.Context)
{
    Other.Context = Other.RootNode = nullptr;
}

ItaniumPartialDemangler &ItaniumPartialDemangler::
operator=(ItaniumPartialDemangler &&Other)
{
    std::swap(RootNode, Other.RootNode);
    std::swap(Context, Other.Context);
    return *this;
}

// Demangle MangledName into an AST, storing it into this->RootNode.
bool ItaniumPartialDemangler::partialDemangle(const char *MangledName)
{
    Demangler *Parser = static_cast<Demangler *>(Context);
    size_t Len = std::strlen(MangledName);
    Parser->reset(MangledName, MangledName + Len);
    RootNode = Parser->parse();
    return RootNode == nullptr;
}

static char *printNode(const Node *RootNode, char *Buf, size_t *N)
{
    OutputBuffer OB(Buf, N);
    RootNode->print(OB);
    OB += '\0';
    if (N != nullptr)
        *N = OB.getCurrentPosition();
    return OB.getBuffer();
}

char *ItaniumPartialDemangler::getFunctionBaseName(char *Buf, size_t *N) const
{
    if (!isFunction())
        return nullptr;

    const Node *Name = static_cast<const FunctionEncoding *>(RootNode)->getName();

    while (true)
    {
        switch (Name->getKind())
        {
            case Node::KAbiTagAttr:
                Name = static_cast<const AbiTagAttr *>(Name)->Base;
                continue;
            case Node::KModuleEntity:
                Name = static_cast<const ModuleEntity *>(Name)->Name;
                continue;
            case Node::KNestedName:
                Name = static_cast<const NestedName *>(Name)->Name;
                continue;
            case Node::KLocalName:
                Name = static_cast<const LocalName *>(Name)->Entity;
                continue;
            case Node::KNameWithTemplateArgs:
                Name = static_cast<const NameWithTemplateArgs *>(Name)->Name;
                continue;
            default:
                return printNode(Name, Buf, N);
        }
    }
}

char *ItaniumPartialDemangler::getFunctionDeclContextName(char *Buf,
    size_t *N) const
{
    if (!isFunction())
        return nullptr;
    const Node *Name = static_cast<const FunctionEncoding *>(RootNode)->getName();

    OutputBuffer OB(Buf, N);

KeepGoingLocalFunction:
    while (true)
    {
        if (Name->getKind() == Node::KAbiTagAttr)
        {
            Name = static_cast<const AbiTagAttr *>(Name)->Base;
            continue;
        }
        if (Name->getKind() == Node::KNameWithTemplateArgs)
        {
            Name = static_cast<const NameWithTemplateArgs *>(Name)->Name;
            continue;
        }
        break;
    }

    if (Name->getKind() == Node::KModuleEntity)
        Name = static_cast<const ModuleEntity *>(Name)->Name;

    switch (Name->getKind())
    {
        case Node::KNestedName:
            static_cast<const NestedName *>(Name)->Qual->print(OB);
            break;
        case Node::KLocalName:
        {
            auto *LN = static_cast<const LocalName *>(Name);
            LN->Encoding->print(OB);
            OB += "::";
            Name = LN->Entity;
            goto KeepGoingLocalFunction;
        }
        default:
            break;
    }
    OB += '\0';
    if (N != nullptr)
        *N = OB.getCurrentPosition();
    return OB.getBuffer();
}

char *ItaniumPartialDemangler::getFunctionName(char *Buf, size_t *N) const
{
    if (!isFunction())
        return nullptr;
    auto *Name = static_cast<FunctionEncoding *>(RootNode)->getName();
    return printNode(Name, Buf, N);
}

char *ItaniumPartialDemangler::getFunctionParameters(char *Buf,
    size_t *N) const
{
    if (!isFunction())
        return nullptr;
    NodeArray Params = static_cast<FunctionEncoding *>(RootNode)->getParams();

    OutputBuffer OB(Buf, N);

    OB += '(';
    Params.printWithComma(OB);
    OB += ')';
    OB += '\0';
    if (N != nullptr)
        *N = OB.getCurrentPosition();
    return OB.getBuffer();
}

char *ItaniumPartialDemangler::getFunctionReturnType(
    char *Buf, size_t *N) const
{
    if (!isFunction())
        return nullptr;

    OutputBuffer OB(Buf, N);

    if (const Node *Ret =
            static_cast<const FunctionEncoding *>(RootNode)->getReturnType())
        Ret->print(OB);

    OB += '\0';
    if (N != nullptr)
        *N = OB.getCurrentPosition();
    return OB.getBuffer();
}

char *ItaniumPartialDemangler::finishDemangle(char *Buf, size_t *N) const
{
    assert(RootNode != nullptr && "must call partialDemangle()");
    return printNode(static_cast<Node *>(RootNode), Buf, N);
}

bool ItaniumPartialDemangler::hasFunctionQualifiers() const
{
    assert(RootNode != nullptr && "must call partialDemangle()");
    if (!isFunction())
        return false;
    auto *E = static_cast<const FunctionEncoding *>(RootNode);
    return E->getCVQuals() != QualNone || E->getRefQual() != FrefQualNone;
}

bool ItaniumPartialDemangler::isCtorOrDtor() const
{
    const Node *N = static_cast<const Node *>(RootNode);
    while (N)
    {
        switch (N->getKind())
        {
            default:
                return false;
            case Node::KCtorDtorName:
                return true;

            case Node::KAbiTagAttr:
                N = static_cast<const AbiTagAttr *>(N)->Base;
                break;
            case Node::KFunctionEncoding:
                N = static_cast<const FunctionEncoding *>(N)->getName();
                break;
            case Node::KLocalName:
                N = static_cast<const LocalName *>(N)->Entity;
                break;
            case Node::KNameWithTemplateArgs:
                N = static_cast<const NameWithTemplateArgs *>(N)->Name;
                break;
            case Node::KNestedName:
                N = static_cast<const NestedName *>(N)->Name;
                break;
            case Node::KModuleEntity:
                N = static_cast<const ModuleEntity *>(N)->Name;
                break;
        }
    }
    return false;
}

bool ItaniumPartialDemangler::isFunction() const
{
    assert(RootNode != nullptr && "must call partialDemangle()");
    return static_cast<const Node *>(RootNode)->getKind() == Node::KFunctionEncoding;
}

bool ItaniumPartialDemangler::isSpecialName() const
{
    assert(RootNode != nullptr && "must call partialDemangle()");
    auto K = static_cast<const Node *>(RootNode)->getKind();
    return K == Node::KSpecialName || K == Node::KCtorVtableSpecialName;
}

bool ItaniumPartialDemangler::isData() const
{
    return !isFunction() && !isSpecialName();
}
