// Copyright (C) 2018 - 2023 Tony's Studio. All rights reserved.

#pragma once

#ifndef _TWIO_UNWRAPPER_H_
#define _TWIO_UNWRAPPER_H_

#include <twio/Common.h>
#include <twio/stream/IStream.h>
#include <memory>

TWIO_BEGIN


// A utility class to unwrap stream to raw buffer.
std::unique_ptr<char[]> UnwrapStream(const IOutputStreamPtr& stream);


TWIO_END

#endif // _TWIO_UNWRAPPER_H_
