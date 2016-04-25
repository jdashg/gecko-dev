/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLCONTEXT_TYPES_H_
#define GLCONTEXT_TYPES_H_

#include "GLTypes.h"
#include "mozilla/TypedEnumBits.h"

namespace mozilla {
namespace gl {

class GLContext;

enum class GLContextType {
    Unknown,
    WGL,
    CGL,
    GLX,
    EGL,
    EAGL
};

enum class OriginPos : uint8_t {
  TopLeft,
  BottomLeft
};

struct GLFormats
{
    // Constructs a zeroed object:
    GLFormats();

    GLenum color_texInternalFormat;
    GLenum color_texFormat;
    GLenum color_texType;
    GLenum color_rbFormat;

    GLenum depthStencil;
    GLenum depth;
    GLenum stencil;

    GLsizei samples;
};

enum class CreateContextFlags : int8_t {
    NONE = 0,
    REQUIRE_COMPAT_PROFILE = 1 << 0,
    // Force the use of hardware backed GL, don't allow software implementations.
    FORCE_ENABLE_HARDWARE = 1 << 1,
    /* Don't force discrete GPU to be used (if applicable) */
    ALLOW_OFFLINE_RENDERER =  1 << 2,
    // Ask for ES3 if possible
    PREFER_ES3 = 1 << 3,
};
MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(CreateContextFlags)

////////////////////////////////////////

void SplitByChar(const nsACString& str, const char delim,
                 std::vector<nsCString>* const out);

template<typename EnumT>
class SupportedSet
{
    std::bitset<EnumT::Max> mBits;
    const char* const* const mNames;

public:
    template<size_t N>
    SupportedSet(const char* (&names)[N])
        : mNames(names)
    {
        MOZ_STATIC_ASSERT(N == EnumT::Max);
    }

    ////////////////

    void MarkSupported(EnumT id) {
        mBits[id] = true;
    }

    void MarkUnsupported(EnumT id) {
        mBits[id] = false;
    }

    bool IsSupported(EnumT id) const {
        return mBits[id];
    }

    ////////////////

    const char* Name(EnumT id) const {
        return mNames[id];
    }

    void MarkByName(const nsACString& str) {
        for (size_t i = 0; i < EnumT::Max; i++) {
            if (str.Equals(mNames[i])) {
                mBits[i] = true;
                return;
            }
        }
    }
};

} /* namespace gl */
} /* namespace mozilla */

#endif /* GLCONTEXT_TYPES_H_ */
