// Copyright (C) 2018 - 2023 Tony's Studio. All rights reserved.

#include "../../include/twio/utils/Printer.h"
#include <cstdio>   // EOF

TWIO_BEGIN

Printer::Printer(IReaderPtr reader, IWriterPtr writer)
    : _reader(std::move(reader)), _writer(std::move(writer))
{
    TWIO_ASSERT(_reader != nullptr);
    TWIO_ASSERT(_writer != nullptr);
}

Printer::Printer(Printer&& other) noexcept
{
    _reader = std::move(other._reader);
    _writer = std::move(other._writer);
}

Printer& Printer::operator=(Printer&& other) noexcept
{
    if (this != &other)
    {
        _reader = std::move(other._reader);
        _writer = std::move(other._writer);
    }

    return *this;
}

std::shared_ptr<Printer> Printer::New(IReaderPtr reader, IWriterPtr writer)
{
    return std::make_shared<Printer>(std::move(reader), std::move(writer));
}

void Printer::Print()
{
    TWIO_ASSERT(IsReady());

    int ch;
    while ((ch = _reader->Read()) != EOF)
    {
        _writer->Write(ch);
    }
}

bool Printer::IsReady() const
{
    return _reader && _writer;
}

TWIO_END
