/*******************************************************************************
 * Copyright (C) Tony's Studio 2018 - 2023. All rights reserved.
 *
 *   For BUAA 2023 Compiler Technology
 */

#include "../../../include/tomic/logger/error/impl/DefaultErrorLogger.h"
#include <string>
#include <algorithm>

TOMIC_BEGIN

class CompareDefaultErrorEntry
{
public:
    bool operator()(const DefaultErrorEntry& lhs, const DefaultErrorEntry& rhs)
    {
        if (lhs._line != rhs._line)
        {
            return lhs._line < rhs._line;
        }
        else if (lhs._column != rhs._column)
        {
            return lhs._column < rhs._column;
        }
        else
        {
            return lhs._type < rhs._type;
        }
    }
};

DefaultErrorLogger::DefaultErrorLogger(IErrorMapperPtr mapper)
        : _mapper(mapper)
{
}

void DefaultErrorLogger::Log(int line, int column, ErrorType type, const char* msg)
{
    _entries.emplace_back(line, column, type, msg);
}

void DefaultErrorLogger::LogFormat(int line, int column, ErrorType type, const char* format, ...)
{
    static char buffer[1024];

    if (format)
    {
        va_list args;
        va_start(args, format);
        vsprintf(buffer, format, args);
        va_end(args);
    }
    else
    {
        buffer[0] = '\0';
    }

    _entries.emplace_back(DefaultErrorEntry(line, column, type, buffer));
}

void DefaultErrorLogger::Dumps(twio::IWriterPtr writer)
{
    // It may be sorted many times, but it doesn't matter. :)
    std::sort(_entries.begin(), _entries.end(), CompareDefaultErrorEntry());

    for (auto& entry: _entries)
    {
        writer->WriteFormat("Line %d, Column %d: %s\n",
                            entry._line,
                            entry._column,
                            _mapper->Description(entry._type));
        writer->WriteFormat("    %s\n", entry._msg.c_str());
    }
}

int DefaultErrorLogger::Count()
{
    return _entries.size();
}

TOMIC_END
