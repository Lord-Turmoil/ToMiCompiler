/*******************************************************************************
 * Copyright (C) Tony's Studio 2018 - 2023. All rights reserved.
 *
 *   For BUAA 2023 Compiler Technology
 */

/*
 * Reference:
 *   https://llvm.org/doxygen/classllvm_1_1Value.html
 *   https://llvm.org/doxygen/Value_8h_source.html
 */

#ifndef _TOMIC_LLVM_VALUE_H_
#define _TOMIC_LLVM_VALUE_H_

#include <memory>
#include <string>
#include <tomic/llvm/asm/IAsmWriter.h>
#include <tomic/llvm/ir/IrForward.h>
#include <tomic/llvm/ir/value/Use.h>
#include <tomic/llvm/ir/value/ValueType.h>
#include <tomic/llvm/Llvm.h>
#include <vector>

TOMIC_LLVM_BEGIN

class Value
{
public:
    virtual ~Value() = default;


    static bool classof(ValueType type) { return true; }


    /*
     * I suddenly realized that, except dependency injection, I hardly
     * ever used Object-Oriented method in the whole project. Perhaps
     * it is time for us to utilize it. :(
     *
     * All implementations will be placed under llvm/asm directory.
     */

    /*
     * Print the complete asm code of this value. This is used to print
     * the value itself on its occurrence.
     */
    virtual void PrintAsm(IAsmWriterPtr writer);

    /*
     * Print the use of this value. Usually, the type and name.
     */
    virtual void PrintUse(IAsmWriterPtr writer);

    /*
     * Print only the name of this value. For example, only %1.
     */
    virtual void PrintName(IAsmWriterPtr writer);

    ValueType GetValueType() const { return _valueType; }

    // Get the type of this value.
    TypePtr GetType() const { return _type; }

    // Get the context.
    LlvmContextPtr Context() const;


    // Check if this value is of a specific type.
    template<typename _Ty>
    bool Is() const { return _Ty::classof(_valueType); }


    // Cast this type to a specific type.
    // You should use this function only when you are sure that this type is
    // actually the type you want to cast to.
    template<typename _Ty>
    _Ty* As()
    {
        TOMIC_ASSERT(Is<_Ty>());
        return static_cast<_Ty*>(this);
    }


    // Get and set name for this Value.
    // If set, it will no longer use number as its name.
    const char* GetName() const { return _name.c_str(); }
    void SetName(const std::string& name) { _name = name; }

public:
    /*
     * Some properties of this value.
     * Should be overridden by subclasses respectively.
     */
    virtual bool IsInstruction() const { return false; }
    virtual bool IsUser() const { return false; }
    virtual bool IsFunction() const { return false; }
    virtual bool IsArgument() const { return false; }
    virtual bool IsGlobalValue() const { return false; }

protected:
    Value(ValueType valueType, TypePtr type) : _type(type), _valueType(valueType)
    {
    }


    Value(ValueType valueType, TypePtr type, UseListPtr useList)
        : _type(type), _valueType(valueType)
    {
        if (useList)
        {
            _useList = *useList;
        }
    }

protected:
    using _use_iterator_raw = UseList::iterator;


    class use_base_iterator_base
    {
    public:
        use_base_iterator_base(_use_iterator_raw iter) : _iter(iter)
        {
        }


        bool operator==(const use_base_iterator_base& rhs) const
        {
            return _iter == rhs._iter;
        }


        bool operator!=(const use_base_iterator_base& rhs) const
        {
            return _iter != rhs._iter;
        }


        // Prefix increment
        use_base_iterator_base& operator++()
        {
            ++_iter;
            return *this;
        }


        // Postfix increment
        use_base_iterator_base operator++(int)
        {
            use_base_iterator_base tmp = *this;
            ++_iter;
            return tmp;
        }

    protected:
        _use_iterator_raw _iter;
    };


    class use_iterator_impl : use_base_iterator_base
    {
        friend class Value;

    public:
        use_iterator_impl(_use_iterator_raw iter) : use_base_iterator_base(iter)
        {
        }


        UsePtr& operator*() const
        {
            return *_iter;
        }


        UsePtr* operator->() const
        {
            return &*_iter;
        }
    };


    class user_iterator_impl : use_base_iterator_base
    {
        friend class Value;

    public:
        user_iterator_impl(_use_iterator_raw iter) : use_base_iterator_base(iter)
        {
        }


        UserPtr operator*() const
        {
            return (*_iter)->GetUser();
        }


        UserPtr operator->() const
        {
            return operator*();
        }
    };

public:
    // Just... no const friend. :(
    using use_iterator = use_iterator_impl;
    using user_iterator = user_iterator_impl;

    use_iterator UseBegin() { return use_iterator(_useList.begin()); }
    use_iterator UseEnd() { return use_iterator(_useList.end()); }
    use_iterator RemoveUse(use_iterator iter) { return { _useList.erase(iter._iter) }; }

    user_iterator UserBegin() { return user_iterator(_userList.begin()); }
    user_iterator UserEnd() { return user_iterator(_userList.end()); }
    user_iterator RemoveUser(user_iterator iter) { return { _userList.erase(iter._iter) }; }

    UsePtr UseAt(int index) const { return _useList[index]; }
    UserPtr UserAt(int index) const { return _userList[index]->GetUser(); }

    void AddUser(UsePtr user);
    void AddUse(UsePtr use);

protected:
    TypePtr _type;

    UseList _useList;
    UseList _userList;

    std::string _name;

private:
    ValueType _valueType;
};


TOMIC_LLVM_END

#endif // _TOMIC_LLVM_VALUE_H_
