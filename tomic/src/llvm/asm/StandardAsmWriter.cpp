/*******************************************************************************
 * Copyright (C) Tony's Studio 2018 - 2023. All rights reserved.
 *
 *   For BUAA 2023 Compiler Technology
 */

#include <tomic/llvm/asm/impl/StandardAsmWriter.h>
#include <cstdarg>

TOMIC_LLVM_BEGIN

StandardAsmWriter::StandardAsmWriter(twio::IWriterPtr writer)
    : _writer(writer)
{
    TOMIC_ASSERT(writer);
}


StandardAsmWriterPtr StandardAsmWriter::New(twio::IWriterPtr writer)
{
    return std::make_shared<StandardAsmWriter>(writer);
}


void StandardAsmWriter::Push(char ch)
{
    _writer->Write(ch);
}


void StandardAsmWriter::Push(const char* format, ...)
{
    va_list args;

    va_start(args, format);
    _writer->WriteVFormat(format, args);
    va_end(args);
}


void StandardAsmWriter::PushNext(char ch)
{
    PushSpace();
    Push(ch);
}


void StandardAsmWriter::PushNext(const char* format, ...)
{
    va_list args;

    va_start(args, format);
    PushSpace();
    _writer->WriteVFormat(format, args);
    va_end(args);
}


void StandardAsmWriter::PushSpace()
{
    _writer->Write(' ');
}


void StandardAsmWriter::PushSpaces(int repeat)
{
    for (int i = 0; i < repeat; i++)
    {
        _writer->Write(' ');
    }
}


void StandardAsmWriter::PushNewLine()
{
    _writer->Write('\n');
}


void StandardAsmWriter::PushNewLines(int repeat)
{
    for (int i = 0; i < repeat; i++)
    {
        _writer->Write(' ');
    }
}


void StandardAsmWriter::PushComment(const char* format, ...)
{
    va_list args;

    CommentBegin();

    va_start(args, format);
    _writer->WriteVFormat(format, args);
    va_end(args);

    CommentEnd();
}


void StandardAsmWriter::CommentBegin()
{
    _writer->Write(';');
    PushSpace();
}


void StandardAsmWriter::CommentEnd()
{
    PushNewLine();
}


TOMIC_LLVM_END
