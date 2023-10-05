/*******************************************************************************
 * Copyright (C) Tony's Studio 2018 - 2023. All rights reserved.
 *
 *   For BUAA 2023 Compiler Technology
 */

#ifndef _TOMIC_CONFIG_H_
#define _TOMIC_CONFIG_H_

#include "../Common.h"
#include <memory>
#include <string>

TOMIC_BEGIN

class IConfig
{
public:
    virtual ~IConfig() = default;

    virtual bool EnableCompleteAst() const = 0;
    virtual const char* OutputExt() const = 0;
};

using IConfigPtr = std::shared_ptr<IConfig>;

class Config : public IConfig
{
public:
    Config() : _enableCompleteAst(false), _outputExt(".ast") {}
    ~Config() override = default;

    static std::shared_ptr<Config> New() { return std::make_shared<Config>(); }

    bool EnableCompleteAst() const override { return _enableCompleteAst; }
    const char* OutputExt() const override { return _outputExt.c_str(); }

public:
    Config* SetEnableCompleteAst(bool enable)
    {
        _enableCompleteAst = enable;
        return this;
    }
    Config* SetOutputExt(std::string ext)
    {
        _outputExt = ext;
        return this;
    }

private:
    bool _enableCompleteAst;
    std::string _outputExt;
};

using ConfigPtr = std::shared_ptr<Config>;

TOMIC_END

#endif // _TOMIC_CONFIG_H_