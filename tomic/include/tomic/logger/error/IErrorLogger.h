/*******************************************************************************
 * Copyright (C) Tony's Studio 2018 - 2023. All rights reserved.
 *
 *   For BUAA 2023 Compiler Technology
 */

#ifndef _TOMIC_ERROR_LOGGER_H_
#define _TOMIC_ERROR_LOGGER_H_

#include "../../Common.h"
#include "ErrorType.h"
#include <memory>

TOMIC_BEGIN

class IErrorLogger
{
public:
    virtual ~IErrorLogger() {}

    virtual void Log(int line, int column, ErrorType type, const char* msg) = 0;

    virtual void Dumps(twio::IWriterPtr writer) = 0;
};

using IErrorLoggerPtr = std::shared_ptr<IErrorLogger>;

TOMIC_END

#endif // _TOMIC_ERROR_LOGGER_H_