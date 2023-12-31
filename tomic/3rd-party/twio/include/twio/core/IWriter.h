// Copyright (C) 2018 - 2023 Tony's Studio. All rights reserved.

#pragma once

#ifndef _TWIO_IWRITER_H_
#define _TWIO_IWRITER_H_

#include <memory>
#include <twio/Common.h>
#include <twio/stream/IStream.h>
#include <cstdarg>

TWIO_BEGIN


class IWriter
{
public:
    virtual ~IWriter() = 0;

    // Write a buffer to output stream.
    virtual size_t Write(const char* buffer, size_t size) = 0;

    // Write a null-terminated string to output stream.
    virtual size_t Write(const char* buffer) = 0;

    // Write a character to output stream.
    virtual size_t Write(char ch) = 0;

    // Write formatted string to output stream.
    virtual size_t WriteFormat(const char* format, ...) = 0;
    virtual size_t WriteVFormat(const char* format, va_list args) = 0;

    // Dangerous method, use with caution.
    virtual IOutputStreamPtr Stream() const = 0;

    virtual void Close() = 0;
};


inline IWriter::~IWriter() = default;

using IWriterPtr = std::shared_ptr<IWriter>;


TWIO_END

#endif // _TWIO_IWRITER_H_
