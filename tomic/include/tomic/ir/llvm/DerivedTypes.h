/*******************************************************************************
 * Copyright (C) Tony's Studio 2018 - 2023. All rights reserved.
 *
 *   For BUAA 2023 Compiler Technology
 */

/*
 * Reference:
 *   https://llvm.org/doxygen/classllvm_1_1Type.html
 *   https://llvm.org/doxygen/DerivedTypes_8h_source.html
 */

#ifndef _TOMIC_LLVM_DERIVED_TYPE_H_
#define _TOMIC_LLVM_DERIVED_TYPE_H_

#include <tomic/ir/llvm/Llvm.h>
#include <tomic/ir/llvm/Type.h>

TOMIC_LLVM_BEGIN

/*
 * In LLVM implementation, types are shared
 */

class IntegerType : public Type
{
    friend class LLVMContext;
public:
    IntegerType(const IntegerType&) = delete;
    IntegerType& operator=(const IntegerType&) = delete;

    static IntegerType* Get(unsigned bitWidth);

    unsigned BitWidth() const { return _bitWidth; }

protected:
    IntegerType(unsigned bitWidth) : Type(IntegerTyID), _bitWidth(bitWidth) {}

private:
    unsigned _bitWidth;
};

using IntegerTypePtr = IntegerType*;

class FunctionType : public Type
{
    friend class LLVMContext;
public:
    FunctionType(const FunctionType&) = delete;
    FunctionType& operator=(const FunctionType&) = delete;

    static FunctionType* Get(Type* returnType, const std::vector<Type*>& paramTypes, bool isVarArg);
    static FunctionType* Get(Type* returnType, bool isVarArg);

    Type* ReturnType() const { return _returnType; }
    bool IsVarArg() const { return _isVarArg; }

    using param_iterator = std::vector<Type*>::iterator;
    using const_param_iterator = std::vector<Type*>::const_iterator;

    param_iterator ParamBegin() { return _containedTypes.begin() + 1; }
    param_iterator ParamEnd() { return _containedTypes.end(); }
    const_param_iterator ParamBegin() const { return _containedTypes.begin() + 1; }
    const_param_iterator ParamEnd() const { return _containedTypes.end(); }

    int ParamCount() const { return _containedTypes.size() - 1; }

protected:
    FunctionType(Type* returnType, std::vector<Type*> paramTypes, bool isVarArg);

private:
    Type* _returnType;
    bool _isVarArg;
};

using FunctionTypePtr = FunctionType*;

class ArrayType : public Type
{
    friend class LLVMContext;
public:
    ArrayType(const ArrayType&) = delete;
    ArrayType& operator=(const ArrayType&) = delete;

    static ArrayType* Get(Type* elementType, int elementCount);

    Type* ElementType() const { return _elementType; }
    int ElementCount() const { return _elementCount; }

private:
    Type* _elementType;
    int _elementCount;
};

using ArrayTypePtr = ArrayType*;

TOMIC_LLVM_END

#endif // _TOMIC_LLVM_TYPE_H_