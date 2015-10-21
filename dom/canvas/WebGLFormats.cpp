/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLFormats.h"

#include "GLDefs.h"
#include "mozilla/StaticMutex.h"

namespace mozilla {
namespace webgl {

//////////////////////////////////////////////////////////////////////////////////////////

std::map<EffectiveFormat, const CompressedFormatInfo> gCompressedFormatInfoMap;
std::map<EffectiveFormat, const FormatInfo> gFormatInfoMap;
std::map<UnpackTuple, const FormatInfo*> gUnpackTupleMap;
std::map<GLenum, const FormatInfo*> gSizedFormatMap;

static const CompressedFormatInfo*
GetCompressedFormatInfo(EffectiveFormat format)
{
    MOZ_ASSERT(!gCompressedFormatInfoMap.empty());
    auto itr = gCompressedFormatInfoMap.find(format);
    if (itr == gCompressedFormatInfoMap.end())
        return nullptr;

    return &(itr->second);
}

static const FormatInfo*
GetFormatInfo_NoLock(EffectiveFormat format)
{
    MOZ_ASSERT(!gFormatInfoMap.empty());
    auto itr = gFormatInfoMap.find(format);
    if (itr == gFormatInfoMap.end())
        return nullptr;

    return &(itr->second);
}

// Returns an iterator to the in-place pair.
template<typename K, typename V, typename K2, typename V2>
static auto
AlwaysInsert(std::map<K,V>& dest, const K2& key, const V2& val)
{
    auto res = dest.insert({ key, val });
    bool didInsert = res.second;
    MOZ_ALWAYS_TRUE(didInsert);

    return res.first;
}

//////////////////////////////////////////////////////////////////////////////////////////

static void
AddCompressedFormatInfo(EffectiveFormat format, uint16_t bitsPerBlock, uint8_t blockWidth,
                        uint8_t blockHeight, bool requirePOT,
                        SubImageUpdateBehavior subImageUpdateBehavior)
{
    MOZ_ASSERT(bitsPerBlock % 8 == 0);
    uint16_t bytesPerBlock = bitsPerBlock / 8; // The specs always state these in bits,
                                               // but it's only ever useful to us as
                                               // bytes.
    MOZ_ASSERT(bytesPerBlock <= 255);

    const CompressedFormatInfo info = { format, uint8_t(bytesPerBlock), blockWidth,
                                        blockHeight, requirePOT, subImageUpdateBehavior };
    AlwaysInsert(gCompressedFormatInfoMap, format, info);
}

static void
InitCompressedFormatInfo()
{
    // GLES 3.0.4, p147, table 3.19
    // GLES 3.0.4, p286+, $C.1 "ETC Compressed Texture Image Formats"
    AddCompressedFormatInfo(EffectiveFormat::COMPRESSED_RGB8_ETC2                     ,  64, 4, 4, false, SubImageUpdateBehavior::BlockAligned);
    AddCompressedFormatInfo(EffectiveFormat::COMPRESSED_SRGB8_ETC2                    ,  64, 4, 4, false, SubImageUpdateBehavior::BlockAligned);
    AddCompressedFormatInfo(EffectiveFormat::COMPRESSED_RGBA8_ETC2_EAC                , 128, 4, 4, false, SubImageUpdateBehavior::BlockAligned);
    AddCompressedFormatInfo(EffectiveFormat::COMPRESSED_SRGB8_ALPHA8_ETC2_EAC         , 128, 4, 4, false, SubImageUpdateBehavior::BlockAligned);
    AddCompressedFormatInfo(EffectiveFormat::COMPRESSED_R11_EAC                       ,  64, 4, 4, false, SubImageUpdateBehavior::BlockAligned);
    AddCompressedFormatInfo(EffectiveFormat::COMPRESSED_RG11_EAC                      , 128, 4, 4, false, SubImageUpdateBehavior::BlockAligned);
    AddCompressedFormatInfo(EffectiveFormat::COMPRESSED_SIGNED_R11_EAC                ,  64, 4, 4, false, SubImageUpdateBehavior::BlockAligned);
    AddCompressedFormatInfo(EffectiveFormat::COMPRESSED_SIGNED_RG11_EAC               , 128, 4, 4, false, SubImageUpdateBehavior::BlockAligned);
    AddCompressedFormatInfo(EffectiveFormat::COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2 ,  64, 4, 4, false, SubImageUpdateBehavior::BlockAligned);
    AddCompressedFormatInfo(EffectiveFormat::COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2,  64, 4, 4, false, SubImageUpdateBehavior::BlockAligned);

    // AMD_compressed_ATC_texture
    AddCompressedFormatInfo(EffectiveFormat::ATC_RGB_AMD                    ,  64, 4, 4, false, SubImageUpdateBehavior::Forbidden);
    AddCompressedFormatInfo(EffectiveFormat::ATC_RGBA_EXPLICIT_ALPHA_AMD    , 128, 4, 4, false, SubImageUpdateBehavior::Forbidden);
    AddCompressedFormatInfo(EffectiveFormat::ATC_RGBA_INTERPOLATED_ALPHA_AMD, 128, 4, 4, false, SubImageUpdateBehavior::Forbidden);

    // EXT_texture_compression_s3tc
    AddCompressedFormatInfo(EffectiveFormat::COMPRESSED_RGB_S3TC_DXT1 ,  64, 4, 4, false, SubImageUpdateBehavior::BlockAligned);
    AddCompressedFormatInfo(EffectiveFormat::COMPRESSED_RGBA_S3TC_DXT1,  64, 4, 4, false, SubImageUpdateBehavior::BlockAligned);
    AddCompressedFormatInfo(EffectiveFormat::COMPRESSED_RGBA_S3TC_DXT3, 128, 4, 4, false, SubImageUpdateBehavior::BlockAligned);
    AddCompressedFormatInfo(EffectiveFormat::COMPRESSED_RGBA_S3TC_DXT5, 128, 4, 4, false, SubImageUpdateBehavior::BlockAligned);

    // IMG_texture_compression_pvrtc
    AddCompressedFormatInfo(EffectiveFormat::COMPRESSED_RGB_PVRTC_4BPPV1 , 256,  8, 8, true, SubImageUpdateBehavior::FullOnly);
    AddCompressedFormatInfo(EffectiveFormat::COMPRESSED_RGBA_PVRTC_4BPPV1, 256,  8, 8, true, SubImageUpdateBehavior::FullOnly);
    AddCompressedFormatInfo(EffectiveFormat::COMPRESSED_RGB_PVRTC_2BPPV1 , 256, 16, 8, true, SubImageUpdateBehavior::FullOnly);
    AddCompressedFormatInfo(EffectiveFormat::COMPRESSED_RGBA_PVRTC_2BPPV1, 256, 16, 8, true, SubImageUpdateBehavior::FullOnly);

    // OES_compressed_ETC1_RGB8_texture
    AddCompressedFormatInfo(EffectiveFormat::ETC1_RGB8, 64, 4, 4, false, SubImageUpdateBehavior::Forbidden);
}

//////////////////////////////////////////////////////////////////////////////////////////

static void
AddFormatInfo(EffectiveFormat format, const char* name, uint8_t bytesPerPixel,
              UnsizedFormat unsizedFormat, bool isSRGB, ComponentType componentType)
{
    bool isColorFormat = false;
    bool hasAlpha = false;
    bool hasDepth = false;
    bool hasStencil = false;

    switch (unsizedFormat) {
    case UnsizedFormat::L:
    case UnsizedFormat::R:
    case UnsizedFormat::RG:
    case UnsizedFormat::RGB:
        isColorFormat = true;
        break;

    case UnsizedFormat::A: // Alpha is a 'color format' since it's 'color-attachable'.
    case UnsizedFormat::LA:
    case UnsizedFormat::RGBA:
        isColorFormat = true;
        hasAlpha = true;
        break;

    case UnsizedFormat::D:
        hasDepth = true;
        break;

    case UnsizedFormat::S:
        hasStencil = true;
        break;

    case UnsizedFormat::DS:
        hasDepth = true;
        hasStencil = true;
        break;

    default:
        MOZ_CRASH("Missing UnsizedFormat case.");
    }

    const CompressedFormatInfo* compressedFormatInfo = GetCompressedFormatInfo(format);
    MOZ_ASSERT(!bytesPerPixel == bool(compressedFormatInfo));

    const FormatInfo info = { format, name, unsizedFormat, componentType, bytesPerPixel,
                              isColorFormat, isSRGB, hasAlpha, hasDepth, hasStencil,
                              compressedFormatInfo };
    AlwaysInsert(gFormatInfoMap, format, info);
}

static void
InitFormatInfoMap()
{
#ifdef FOO
#error FOO is already defined!
#endif

#define FOO(x) EffectiveFormat::x, #x

    // GLES 3.0.4, p130-132, table 3.13
    AddFormatInfo(FOO(R8            ),  1, UnsizedFormat::R   , false, ComponentType::NormUInt);
    AddFormatInfo(FOO(R8_SNORM      ),  1, UnsizedFormat::R   , false, ComponentType::NormInt );
    AddFormatInfo(FOO(RG8           ),  2, UnsizedFormat::RG  , false, ComponentType::NormUInt);
    AddFormatInfo(FOO(RG8_SNORM     ),  2, UnsizedFormat::RG  , false, ComponentType::NormInt );
    AddFormatInfo(FOO(RGB8          ),  3, UnsizedFormat::RGB , false, ComponentType::NormUInt);
    AddFormatInfo(FOO(RGB8_SNORM    ),  3, UnsizedFormat::RGB , false, ComponentType::NormInt );
    AddFormatInfo(FOO(RGB565        ),  2, UnsizedFormat::RGB , false, ComponentType::NormUInt);
    AddFormatInfo(FOO(RGBA4         ),  2, UnsizedFormat::RGBA, false, ComponentType::NormUInt);
    AddFormatInfo(FOO(RGB5_A1       ),  2, UnsizedFormat::RGBA, false, ComponentType::NormUInt);
    AddFormatInfo(FOO(RGBA8         ),  4, UnsizedFormat::RGBA, false, ComponentType::NormUInt);
    AddFormatInfo(FOO(RGBA8_SNORM   ),  4, UnsizedFormat::RGBA, false, ComponentType::NormInt );
    AddFormatInfo(FOO(RGB10_A2      ),  4, UnsizedFormat::RGBA, false, ComponentType::NormUInt);
    AddFormatInfo(FOO(RGB10_A2UI    ),  4, UnsizedFormat::RGBA, false, ComponentType::UInt    );

    AddFormatInfo(FOO(SRGB8         ),  3, UnsizedFormat::RGB , true , ComponentType::NormUInt);
    AddFormatInfo(FOO(SRGB8_ALPHA8  ),  4, UnsizedFormat::RGBA, true , ComponentType::NormUInt);

    AddFormatInfo(FOO(R16F          ),  2, UnsizedFormat::R   , false, ComponentType::Float   );
    AddFormatInfo(FOO(RG16F         ),  4, UnsizedFormat::RG  , false, ComponentType::Float   );
    AddFormatInfo(FOO(RGB16F        ),  6, UnsizedFormat::RGB , false, ComponentType::Float   );
    AddFormatInfo(FOO(RGBA16F       ),  8, UnsizedFormat::RGBA, false, ComponentType::Float   );
    AddFormatInfo(FOO(R32F          ),  4, UnsizedFormat::R   , false, ComponentType::Float   );
    AddFormatInfo(FOO(RG32F         ),  8, UnsizedFormat::RG  , false, ComponentType::Float   );
    AddFormatInfo(FOO(RGB32F        ), 12, UnsizedFormat::RGB , false, ComponentType::Float   );
    AddFormatInfo(FOO(RGBA32F       ), 16, UnsizedFormat::RGBA, false, ComponentType::Float   );

    AddFormatInfo(FOO(R11F_G11F_B10F),  4, UnsizedFormat::RGB , false, ComponentType::Float   );
    AddFormatInfo(FOO(RGB9_E5       ),  4, UnsizedFormat::RGB , false, ComponentType::Float   );

    AddFormatInfo(FOO(R8I           ),  1, UnsizedFormat::R   , false, ComponentType::Int     );
    AddFormatInfo(FOO(R8UI          ),  1, UnsizedFormat::R   , false, ComponentType::UInt    );
    AddFormatInfo(FOO(R16I          ),  2, UnsizedFormat::R   , false, ComponentType::Int     );
    AddFormatInfo(FOO(R16UI         ),  2, UnsizedFormat::R   , false, ComponentType::UInt    );
    AddFormatInfo(FOO(R32I          ),  4, UnsizedFormat::R   , false, ComponentType::Int     );
    AddFormatInfo(FOO(R32UI         ),  4, UnsizedFormat::R   , false, ComponentType::UInt    );

    AddFormatInfo(FOO(RG8I          ),  2, UnsizedFormat::RG  , false, ComponentType::Int     );
    AddFormatInfo(FOO(RG8UI         ),  2, UnsizedFormat::RG  , false, ComponentType::UInt    );
    AddFormatInfo(FOO(RG16I         ),  4, UnsizedFormat::RG  , false, ComponentType::Int     );
    AddFormatInfo(FOO(RG16UI        ),  4, UnsizedFormat::RG  , false, ComponentType::UInt    );
    AddFormatInfo(FOO(RG32I         ),  8, UnsizedFormat::RG  , false, ComponentType::Int     );
    AddFormatInfo(FOO(RG32UI        ),  8, UnsizedFormat::RG  , false, ComponentType::UInt    );

    AddFormatInfo(FOO(RGB8I         ),  3, UnsizedFormat::RGB , false, ComponentType::Int     );
    AddFormatInfo(FOO(RGB8UI        ),  3, UnsizedFormat::RGB , false, ComponentType::UInt    );
    AddFormatInfo(FOO(RGB16I        ),  6, UnsizedFormat::RGB , false, ComponentType::Int     );
    AddFormatInfo(FOO(RGB16UI       ),  6, UnsizedFormat::RGB , false, ComponentType::UInt    );
    AddFormatInfo(FOO(RGB32I        ), 12, UnsizedFormat::RGB , false, ComponentType::Int     );
    AddFormatInfo(FOO(RGB32UI       ), 12, UnsizedFormat::RGB , false, ComponentType::UInt    );

    AddFormatInfo(FOO(RGBA8I        ),  4, UnsizedFormat::RGBA, false, ComponentType::Int     );
    AddFormatInfo(FOO(RGBA8UI       ),  4, UnsizedFormat::RGBA, false, ComponentType::UInt    );
    AddFormatInfo(FOO(RGBA16I       ),  8, UnsizedFormat::RGBA, false, ComponentType::Int     );
    AddFormatInfo(FOO(RGBA16UI      ),  8, UnsizedFormat::RGBA, false, ComponentType::UInt    );
    AddFormatInfo(FOO(RGBA32I       ), 16, UnsizedFormat::RGBA, false, ComponentType::Int     );
    AddFormatInfo(FOO(RGBA32UI      ), 16, UnsizedFormat::RGBA, false, ComponentType::UInt    );

    // GLES 3.0.4, p133, table 3.14
    AddFormatInfo(FOO(DEPTH_COMPONENT16 ), 2, UnsizedFormat::D , false, ComponentType::NormUInt);
    AddFormatInfo(FOO(DEPTH_COMPONENT24 ), 3, UnsizedFormat::D , false, ComponentType::NormUInt);
    AddFormatInfo(FOO(DEPTH_COMPONENT32F), 4, UnsizedFormat::D , false, ComponentType::Float);
    AddFormatInfo(FOO(DEPTH24_STENCIL8  ), 4, UnsizedFormat::DS, false, ComponentType::None);
    AddFormatInfo(FOO(DEPTH32F_STENCIL8 ), 5, UnsizedFormat::DS, false, ComponentType::None);

    // GLES 3.0.4, p205-206, "Required Renderbuffer Formats"
    AddFormatInfo(FOO(STENCIL_INDEX8), 1, UnsizedFormat::S, false, ComponentType::UInt);

    // GLES 3.0.4, p128, table 3.12.
    AddFormatInfo(FOO(Luminance8Alpha8), 2, UnsizedFormat::LA, false, ComponentType::NormUInt);
    AddFormatInfo(FOO(Luminance8      ), 1, UnsizedFormat::L , false, ComponentType::NormUInt);
    AddFormatInfo(FOO(Alpha8          ), 1, UnsizedFormat::A , false, ComponentType::NormUInt);

    // GLES 3.0.4, p147, table 3.19
    // GLES 3.0.4  p286+  $C.1 "ETC Compressed Texture Image Formats"
    AddFormatInfo(FOO(COMPRESSED_RGB8_ETC2                     ), 0, UnsizedFormat::RGB , false, ComponentType::NormUInt);
    AddFormatInfo(FOO(COMPRESSED_SRGB8_ETC2                    ), 0, UnsizedFormat::RGB , true , ComponentType::NormUInt);
    AddFormatInfo(FOO(COMPRESSED_RGBA8_ETC2_EAC                ), 0, UnsizedFormat::RGBA, false, ComponentType::NormUInt);
    AddFormatInfo(FOO(COMPRESSED_SRGB8_ALPHA8_ETC2_EAC         ), 0, UnsizedFormat::RGBA, true , ComponentType::NormUInt);
    AddFormatInfo(FOO(COMPRESSED_R11_EAC                       ), 0, UnsizedFormat::R   , false, ComponentType::NormUInt);
    AddFormatInfo(FOO(COMPRESSED_RG11_EAC                      ), 0, UnsizedFormat::RG  , false, ComponentType::NormUInt);
    AddFormatInfo(FOO(COMPRESSED_SIGNED_R11_EAC                ), 0, UnsizedFormat::R   , false, ComponentType::NormInt );
    AddFormatInfo(FOO(COMPRESSED_SIGNED_RG11_EAC               ), 0, UnsizedFormat::RG  , false, ComponentType::NormInt );
    AddFormatInfo(FOO(COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2 ), 0, UnsizedFormat::RGBA, false, ComponentType::NormUInt);
    AddFormatInfo(FOO(COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2), 0, UnsizedFormat::RGBA, true , ComponentType::NormUInt);

    // AMD_compressed_ATC_texture
    AddFormatInfo(FOO(ATC_RGB_AMD                    ), 0, UnsizedFormat::RGB , false, ComponentType::NormUInt);
    AddFormatInfo(FOO(ATC_RGBA_EXPLICIT_ALPHA_AMD    ), 0, UnsizedFormat::RGBA, false, ComponentType::NormUInt);
    AddFormatInfo(FOO(ATC_RGBA_INTERPOLATED_ALPHA_AMD), 0, UnsizedFormat::RGBA, false, ComponentType::NormUInt);

    // EXT_texture_compression_s3tc
    AddFormatInfo(FOO(COMPRESSED_RGB_S3TC_DXT1 ), 0, UnsizedFormat::RGB , false, ComponentType::NormUInt);
    AddFormatInfo(FOO(COMPRESSED_RGBA_S3TC_DXT1), 0, UnsizedFormat::RGBA, false, ComponentType::NormUInt);
    AddFormatInfo(FOO(COMPRESSED_RGBA_S3TC_DXT3), 0, UnsizedFormat::RGBA, false, ComponentType::NormUInt);
    AddFormatInfo(FOO(COMPRESSED_RGBA_S3TC_DXT5), 0, UnsizedFormat::RGBA, false, ComponentType::NormUInt);

    // IMG_texture_compression_pvrtc
    AddFormatInfo(FOO(COMPRESSED_RGB_PVRTC_4BPPV1 ), 0, UnsizedFormat::RGB , false, ComponentType::NormUInt);
    AddFormatInfo(FOO(COMPRESSED_RGBA_PVRTC_4BPPV1), 0, UnsizedFormat::RGBA, false, ComponentType::NormUInt);
    AddFormatInfo(FOO(COMPRESSED_RGB_PVRTC_2BPPV1 ), 0, UnsizedFormat::RGB , false, ComponentType::NormUInt);
    AddFormatInfo(FOO(COMPRESSED_RGBA_PVRTC_2BPPV1), 0, UnsizedFormat::RGBA, false, ComponentType::NormUInt);

    // OES_compressed_ETC1_RGB8_texture
    AddFormatInfo(FOO(ETC1_RGB8), 0, UnsizedFormat::RGB, false, ComponentType::NormUInt);

    // OES_texture_float
    AddFormatInfo(FOO(Luminance32FAlpha32F), 2, UnsizedFormat::LA, false, ComponentType::Float);
    AddFormatInfo(FOO(Luminance32F        ), 1, UnsizedFormat::L , false, ComponentType::Float);
    AddFormatInfo(FOO(Alpha32F            ), 1, UnsizedFormat::A , false, ComponentType::Float);

    // OES_texture_half_float
    AddFormatInfo(FOO(Luminance16FAlpha16F), 2, UnsizedFormat::LA, false, ComponentType::Float);
    AddFormatInfo(FOO(Luminance16F        ), 1, UnsizedFormat::L , false, ComponentType::Float);
    AddFormatInfo(FOO(Alpha16F            ), 1, UnsizedFormat::A , false, ComponentType::Float);

#undef FOO
}

//////////////////////////////////////////////////////////////////////////////////////////

static void
AddUnpackTuple(GLenum unpackFormat, GLenum unpackType, EffectiveFormat effectiveFormat)
{
    const UnpackTuple unpack = { unpackFormat, unpackType };
    const FormatInfo* info = GetFormatInfo_NoLock(effectiveFormat);
    MOZ_ASSERT(info);

    AlwaysInsert(gUnpackTupleMap, unpack, info);
}

static void
InitUnpackTupleMap()
{
    AddUnpackTuple(LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE         , EffectiveFormat::RGBA8  );
    AddUnpackTuple(LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_SHORT_4_4_4_4, EffectiveFormat::RGBA4  );
    AddUnpackTuple(LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_SHORT_5_5_5_1, EffectiveFormat::RGB5_A1);
    AddUnpackTuple(LOCAL_GL_RGB , LOCAL_GL_UNSIGNED_BYTE         , EffectiveFormat::RGB8   );
    AddUnpackTuple(LOCAL_GL_RGB , LOCAL_GL_UNSIGNED_SHORT_5_6_5  , EffectiveFormat::RGB565 );

    AddUnpackTuple(LOCAL_GL_LUMINANCE_ALPHA, LOCAL_GL_UNSIGNED_BYTE, EffectiveFormat::Luminance8Alpha8);
    AddUnpackTuple(LOCAL_GL_LUMINANCE      , LOCAL_GL_UNSIGNED_BYTE, EffectiveFormat::Luminance8      );
    AddUnpackTuple(LOCAL_GL_ALPHA          , LOCAL_GL_UNSIGNED_BYTE, EffectiveFormat::Alpha8          );

    AddUnpackTuple(LOCAL_GL_RGB            , LOCAL_GL_FLOAT, EffectiveFormat::RGB32F );
    AddUnpackTuple(LOCAL_GL_RGBA           , LOCAL_GL_FLOAT, EffectiveFormat::RGBA32F);
    AddUnpackTuple(LOCAL_GL_LUMINANCE_ALPHA, LOCAL_GL_FLOAT, EffectiveFormat::Luminance32FAlpha32F);
    AddUnpackTuple(LOCAL_GL_LUMINANCE      , LOCAL_GL_FLOAT, EffectiveFormat::Luminance32F);
    AddUnpackTuple(LOCAL_GL_ALPHA          , LOCAL_GL_FLOAT, EffectiveFormat::Alpha32F);

    AddUnpackTuple(LOCAL_GL_RGB            , LOCAL_GL_HALF_FLOAT, EffectiveFormat::RGB16F );
    AddUnpackTuple(LOCAL_GL_RGBA           , LOCAL_GL_HALF_FLOAT, EffectiveFormat::RGBA16F);
    AddUnpackTuple(LOCAL_GL_LUMINANCE_ALPHA, LOCAL_GL_HALF_FLOAT, EffectiveFormat::Luminance16FAlpha16F);
    AddUnpackTuple(LOCAL_GL_LUMINANCE      , LOCAL_GL_HALF_FLOAT, EffectiveFormat::Luminance16F);
    AddUnpackTuple(LOCAL_GL_ALPHA          , LOCAL_GL_HALF_FLOAT, EffectiveFormat::Alpha16F);

    // Everyone's favorite problem-child:
    AddUnpackTuple(LOCAL_GL_RGB            , LOCAL_GL_HALF_FLOAT_OES, EffectiveFormat::RGB16F );
    AddUnpackTuple(LOCAL_GL_RGBA           , LOCAL_GL_HALF_FLOAT_OES, EffectiveFormat::RGBA16F);
    AddUnpackTuple(LOCAL_GL_LUMINANCE_ALPHA, LOCAL_GL_HALF_FLOAT_OES, EffectiveFormat::Luminance16FAlpha16F);
    AddUnpackTuple(LOCAL_GL_LUMINANCE      , LOCAL_GL_HALF_FLOAT_OES, EffectiveFormat::Luminance16F);
    AddUnpackTuple(LOCAL_GL_ALPHA          , LOCAL_GL_HALF_FLOAT_OES, EffectiveFormat::Alpha16F);
}

//////////////////////////////////////////////////////////////////////////////////////////

static void
AddSizedFormat(GLenum sizedFormat, EffectiveFormat effectiveFormat)
{
    const FormatInfo* info = GetFormatInfo_NoLock(effectiveFormat);
    MOZ_ASSERT(info);

    AlwaysInsert(gSizedFormatMap, sizedFormat, info);
}

static void
InitSizedFormatMap()
{
    // GLES 3.0.4, p128-129 "Required Texture Formats"

    // "Texture and renderbuffer color formats"
#ifdef FOO
#error FOO is already defined!
#endif

#define FOO(x) AddSizedFormat(LOCAL_GL_ ## x, EffectiveFormat::x);

    FOO(RGBA32I)
    FOO(RGBA32UI)
    FOO(RGBA16I)
    FOO(RGBA16UI)
    FOO(RGBA8)
    FOO(RGBA8I)
    FOO(RGBA8UI)
    FOO(SRGB8_ALPHA8)
    FOO(RGB10_A2)
    FOO(RGB10_A2UI)
    FOO(RGBA4)
    FOO(RGB5_A1)

    FOO(RGB8)
    FOO(RGB565)

    FOO(RG32I)
    FOO(RG32UI)
    FOO(RG16I)
    FOO(RG16UI)
    FOO(RG8)
    FOO(RG8I)
    FOO(RG8UI)

    FOO(R32I)
    FOO(R32UI)
    FOO(R16I)
    FOO(R16UI)
    FOO(R8)
    FOO(R8I)
    FOO(R8UI)

    // "Texture-only color formats"
    FOO(RGBA32F)
    FOO(RGBA16F)
    FOO(RGBA8_SNORM)

    FOO(RGB32F)
    FOO(RGB32I)
    FOO(RGB32UI)

    FOO(RGB16F)
    FOO(RGB16I)
    FOO(RGB16UI)

    FOO(RGB8_SNORM)
    FOO(RGB8I)
    FOO(RGB8UI)
    FOO(SRGB8)

    FOO(R11F_G11F_B10F)
    FOO(RGB9_E5)

    FOO(RG32F)
    FOO(RG16F)
    FOO(RG8_SNORM)

    FOO(R32F)
    FOO(R16F)
    FOO(R8_SNORM)

    // "Depth formats"
    FOO(DEPTH_COMPONENT32F)
    FOO(DEPTH_COMPONENT24)
    FOO(DEPTH_COMPONENT16)

    // "Combined depth+stencil formats"
    FOO(DEPTH32F_STENCIL8)
    FOO(DEPTH24_STENCIL8)

    // GLES 3.0.4, p205-206, "Required Renderbuffer Formats"
    FOO(STENCIL_INDEX8)

#undef FOO
}

//////////////////////////////////////////////////////////////////////////////////////////

bool gAreFormatTablesInitialized = false;

static void
EnsureInitFormatTables()
{
    if (MOZ_LIKELY(gAreFormatTablesInitialized))
        return;

    gAreFormatTablesInitialized = true;

    InitCompressedFormatInfo();
    InitFormatInfoMap();
    InitUnpackTupleMap();
    InitSizedFormatMap();
}

//////////////////////////////////////////////////////////////////////////////////////////
// Public funcs

StaticMutex gFormatMapMutex;

const FormatInfo*
GetFormatInfo(EffectiveFormat format)
{
    StaticMutexAutoLock lock(gFormatMapMutex);
    EnsureInitFormatTables();

    return GetFormatInfo_NoLock(format);
}

const FormatInfo*
GetInfoByUnpackTuple(GLenum unpackFormat, GLenum unpackType)
{
    StaticMutexAutoLock lock(gFormatMapMutex);
    EnsureInitFormatTables();

    const UnpackTuple unpack = { unpackFormat, unpackType };

    MOZ_ASSERT(!gUnpackTupleMap.empty());
    auto itr = gUnpackTupleMap.find(unpack);
    if (itr == gUnpackTupleMap.end())
        return nullptr;

    return itr->second;
}

const FormatInfo*
GetInfoBySizedFormat(GLenum sizedFormat)
{
    StaticMutexAutoLock lock(gFormatMapMutex);
    EnsureInitFormatTables();

    MOZ_ASSERT(!gSizedFormatMap.empty());
    auto itr = gSizedFormatMap.find(sizedFormat);
    if (itr == gSizedFormatMap.end())
        return nullptr;

    return itr->second;
}


//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
// FormatUsageAuthority

void
FormatUsageInfo::AddUnpack(const PackingInfo& key, const DriverUnpackInfo& value)
{
    MOZ_RELEASE_ASSERT(asTexture);

    const auto itr = AlwaysInsert(validUnpack, key, value);

    if (!idealUnpack) {
        // First one!
        idealUnpack = &(itr->second);
    }
}

bool
FormatUsageInfo::IsUnpackValid(const PackingInfo& key,
                               const DriverUnpackInfo** const out_value) const
{
    auto itr = validUnpacks.find(key);
    if (itr == validUnpacks.end())
        return false;

    *out_value = &(itr->second);
    return true;
}

////////////////////////////////////////

static inline void
AddUsage(FormatUsageAuthority* fua, EffectiveFormat effFormat, bool asRenderbuffer,
         bool isRenderable, bool asTexture, bool isFilterable)
{
    MOZ_ASSERT_IF(asRenderbuffer, isRenderable);
    MOZ_ASSERT_IF(isFilterable, asTexture);

    MOZ_ASSERT(!fua->GetUsage(effFormat));

    auto usage = fua->EditUsage(effFormat);
    usage.asRenderbuffer = asRenderbuffer;
    usage.isRenderable = isRenderable;
    usage.asTexture = asTexture;
    usage.isFilterable = isFilterable;
}

static inline void
AddUnpack(FormatUsageAuthority* fua, EffectiveFormat effFormat, const PackingInfo& key,
          const DriverUnpackInfo& value)
{
    auto usage = fua->EditUsage(effFormat);
    usage->AddUnpack(key, value);
}

static inline void
AddUnsizedUnpack(FormatUsageAuthority* fua, EffectiveFormat format, GLenum unpackFormat,
                 GLenum unpackType)
{
    PackingInfo pi = {unpackFormat, unpackType};
    DriverUnpackInfo dui = {unpackFormat, unpackFormat, unpackType};
    AddUnpack(fua, format, pi, dui);
}

static void
AddLegacyUnpacks_LA8(FormatUsageAuthority* fua, gl::GLContext* gl)
{
    if (gl->IsCoreProfile()) {
        PackingInfo pi;
        DriverUnpackInfo dui;

        pi = {LOCAL_GL_LUMINANCE, LOCAL_GL_UNSIGNED_BYTE};
        dui = {LOCAL_GL_R8, LOCAL_GL_RED, LOCAL_GL_UNSIGNED_BYTE};
        AddUnpack(fua, EffectiveFormat::Luminance8, pi, dui);

        pi = {LOCAL_GL_ALPHA, LOCAL_GL_UNSIGNED_BYTE};
        dui = {LOCAL_GL_R8, LOCAL_GL_RED, LOCAL_GL_UNSIGNED_BYTE};
        AddUnpack(fua, EffectiveFormat::Alpha8, pi, dui);

        pi = {LOCAL_GL_LUMINANCE_ALPHA, LOCAL_GL_UNSIGNED_BYTE};
        dui = {LOCAL_GL_RG8, LOCAL_GL_RG, LOCAL_GL_UNSIGNED_BYTE};
        AddUnpack(fua, EffectiveFormat::Luminance8Alpha8, pi, dui);
    } else {
        AddUnsizedUnpack(fua, EffectiveFormat::Luminance8      , LOCAL_GL_LUMINANCE      , LOCAL_GL_UNSIGNED_BYTE);
        AddUnsizedUnpack(fua, EffectiveFormat::Alpha8          , LOCAL_GL_ALPHA          , LOCAL_GL_UNSIGNED_BYTE);
        AddUnsizedUnpack(fua, EffectiveFormat::Luminance8Alpha8, LOCAL_GL_LUMINANCE_ALPHA, LOCAL_GL_UNSIGNED_BYTE);
    }
}

UniquePtr<FormatUsageAuthority>
FormatUsageAuthority::CreateForWebGL1(gl::GLContext* gl)
{
    UniquePtr<FormatUsageAuthority> ret(new FormatUsageAuthority);

    ////////////////////////////////////////////////////////////////////////////

    // GLES 2.0.25, p117, Table 4.5
    // RGBA8 is made renderable in WebGL 1.0, "Framebuffer Object Attachments"

    //                                              render       filter
    //                                        RB    able   Tex   able
    AddUnpack(ret, EffectiveFormat::RGBA8  , false, true , true, true);
    AddUnpack(ret, EffectiveFormat::RGBA4  , true , true , true, true);
    AddUnpack(ret, EffectiveFormat::RGB5_A1, true , true , true, true);
    AddUnpack(ret, EffectiveFormat::RGB8   , false, false, true, true);
    AddUnpack(ret, EffectiveFormat::RGB565 , true , true , true, true);

    AddUnpack(ret, EffectiveFormat::Luminance8Alpha8, false, false, true, true);
    AddUnpack(ret, EffectiveFormat::Luminance8      , false, false, true, true);
    AddUnpack(ret, EffectiveFormat::Alpha8          , false, false, true, true);

    AddUnpack(ret, EffectiveFormat::DEPTH_COMPONENT16, true, true, false, false);
    AddUnpack(ret, EffectiveFormat::STENCIL_INDEX8   , true, true, false, false);

    // Added in WebGL 1.0 spec:
    AddUnpack(ret, EffectiveFormat::DEPTH24_STENCIL8, true, true, false, false);

    ////////////////////////////////////////////////////////////////////////////

    // GLES 2.0.25, p63, Table 3.4

    // SetUnpack and AddAuxUnpack

    AddUnsizedUnpack(ret, EffectiveFormat::RGBA8  , LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE         );
    AddUnsizedUnpack(ret, EffectiveFormat::RGBA4  , LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_SHORT_4_4_4_4);
    AddUnsizedUnpack(ret, EffectiveFormat::RGB5_A1, LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_SHORT_5_5_5_1);
    AddUnsizedUnpack(ret, EffectiveFormat::RGB8   , LOCAL_GL_RGB , LOCAL_GL_UNSIGNED_BYTE         );
    AddUnsizedUnpack(ret, EffectiveFormat::RGB565 , LOCAL_GL_RGB , LOCAL_GL_UNSIGNED_SHORT_5_6_5  );

    // L, A, LA
    AddLegacyUnpacks_LA8(ret, gl);

    return Move(ret);
}

static void
AddES3TexFormat(FormatUsageAuthority* fua, EffectiveFormat format, bool isRenderable,
                bool isFilterable)
{
    bool asRenderbuffer = isRenderable;
    bool asTexture = true;

    AddUsage(fua, format, asRenderbuffer, isRenderable, asTexture, isFilterable);
}

UniquePtr<FormatUsageAuthority>
FormatUsageAuthority::CreateForWebGL2()
{
    UniquePtr<FormatUsageAuthority> ret(new FormatUsageAuthority);
    FormatUsageAuthority* const ptr = ret.get();

    ////////////////////////////////////////////////////////////////////////////

    // For renderable, see GLES 3.0.4, p212 "Framebuffer Completeness"
    // For filterable, see GLES 3.0.4, p161 "...a texture is complete unless..."

    // GLES 3.0.4, p128-129 "Required Texture Formats"
    // GLES 3.0.4, p130-132, table 3.13
    //                                                render filter
    //                                                 able   able
    AddES3TexFormat(ptr, EffectiveFormat::R8         , true , true );
    AddES3TexFormat(ptr, EffectiveFormat::R8_SNORM   , false, true );
    AddES3TexFormat(ptr, EffectiveFormat::RG8        , true , true );
    AddES3TexFormat(ptr, EffectiveFormat::RG8_SNORM  , false, true );
    AddES3TexFormat(ptr, EffectiveFormat::RGB8       , true , true );
    AddES3TexFormat(ptr, EffectiveFormat::RGB8_SNORM , false, true );
    AddES3TexFormat(ptr, EffectiveFormat::RGB565     , true , true );
    AddES3TexFormat(ptr, EffectiveFormat::RGBA4      , true , true );
    AddES3TexFormat(ptr, EffectiveFormat::RGB5_A1    , true , true );
    AddES3TexFormat(ptr, EffectiveFormat::RGBA8      , true , true );
    AddES3TexFormat(ptr, EffectiveFormat::RGBA8_SNORM, false, true );
    AddES3TexFormat(ptr, EffectiveFormat::RGB10_A2   , true , true );
    AddES3TexFormat(ptr, EffectiveFormat::RGB10_A2UI , true , false);

    AddES3TexFormat(ptr, EffectiveFormat::SRGB8       , false, true);
    AddES3TexFormat(ptr, EffectiveFormat::SRGB8_ALPHA8, true , true);

    AddES3TexFormat(ptr, EffectiveFormat::R16F   , false, true);
    AddES3TexFormat(ptr, EffectiveFormat::RG16F  , false, true);
    AddES3TexFormat(ptr, EffectiveFormat::RGB16F , false, true);
    AddES3TexFormat(ptr, EffectiveFormat::RGBA16F, false, true);

    AddES3TexFormat(ptr, EffectiveFormat::R32F   , false, false);
    AddES3TexFormat(ptr, EffectiveFormat::RG32F  , false, false);
    AddES3TexFormat(ptr, EffectiveFormat::RGB32F , false, false);
    AddES3TexFormat(ptr, EffectiveFormat::RGBA32F, false, false);

    AddES3TexFormat(ptr, EffectiveFormat::R11F_G11F_B10F, false, true);
    AddES3TexFormat(ptr, EffectiveFormat::RGB9_E5       , false, true);

    AddES3TexFormat(ptr, EffectiveFormat::R8I  , true, false);
    AddES3TexFormat(ptr, EffectiveFormat::R8UI , true, false);
    AddES3TexFormat(ptr, EffectiveFormat::R16I , true, false);
    AddES3TexFormat(ptr, EffectiveFormat::R16UI, true, false);
    AddES3TexFormat(ptr, EffectiveFormat::R32I , true, false);
    AddES3TexFormat(ptr, EffectiveFormat::R32UI, true, false);

    AddES3TexFormat(ptr, EffectiveFormat::RG8I  , true, false);
    AddES3TexFormat(ptr, EffectiveFormat::RG8UI , true, false);
    AddES3TexFormat(ptr, EffectiveFormat::RG16I , true, false);
    AddES3TexFormat(ptr, EffectiveFormat::RG16UI, true, false);
    AddES3TexFormat(ptr, EffectiveFormat::RG32I , true, false);
    AddES3TexFormat(ptr, EffectiveFormat::RG32UI, true, false);

    AddES3TexFormat(ptr, EffectiveFormat::RGB8I  , false, false);
    AddES3TexFormat(ptr, EffectiveFormat::RGB8UI , false, false);
    AddES3TexFormat(ptr, EffectiveFormat::RGB16I , false, false);
    AddES3TexFormat(ptr, EffectiveFormat::RGB16UI, false, false);
    AddES3TexFormat(ptr, EffectiveFormat::RGB32I , false, false);
    AddES3TexFormat(ptr, EffectiveFormat::RGB32UI, false, false);

    AddES3TexFormat(ptr, EffectiveFormat::RGBA8I  , true, false);
    AddES3TexFormat(ptr, EffectiveFormat::RGBA8UI , true, false);
    AddES3TexFormat(ptr, EffectiveFormat::RGBA16I , true, false);
    AddES3TexFormat(ptr, EffectiveFormat::RGBA16UI, true, false);
    AddES3TexFormat(ptr, EffectiveFormat::RGBA32I , true, false);
    AddES3TexFormat(ptr, EffectiveFormat::RGBA32UI, true, false);

    // GLES 3.0.4, p133, table 3.14
    // GLES 3.0.4, p161 "...a texture is complete unless..."
    AddES3TexFormat(ptr, EffectiveFormat::DEPTH_COMPONENT16 , true, false);
    AddES3TexFormat(ptr, EffectiveFormat::DEPTH_COMPONENT24 , true, false);
    AddES3TexFormat(ptr, EffectiveFormat::DEPTH_COMPONENT32F, true, false);
    AddES3TexFormat(ptr, EffectiveFormat::DEPTH24_STENCIL8  , true, false);
    AddES3TexFormat(ptr, EffectiveFormat::DEPTH32F_STENCIL8 , true, false);

    // GLES 3.0.4, p205-206, "Required Renderbuffer Formats"
    AddES3TexFormat(ptr, EffectiveFormat::STENCIL_INDEX8, true, false);

    // GLES 3.0.4, p128, table 3.12.
    // Unsized RGBA/RGB formats are renderable, other unsized are not.
    AddES3TexFormat(ptr, EffectiveFormat::Luminance8Alpha8, false, true);
    AddES3TexFormat(ptr, EffectiveFormat::Luminance8      , false, true);
    AddES3TexFormat(ptr, EffectiveFormat::Alpha8          , false, true);

    // GLES 3.0.4, p147, table 3.19
    // GLES 3.0.4, p286+, $C.1 "ETC Compressed Texture Image Formats"
    // (jgilbert) I can't find where these are established as filterable.
    AddES3TexFormat(ptr, EffectiveFormat::COMPRESSED_RGB8_ETC2                     , false, true);
    AddES3TexFormat(ptr, EffectiveFormat::COMPRESSED_SRGB8_ETC2                    , false, true);
    AddES3TexFormat(ptr, EffectiveFormat::COMPRESSED_RGBA8_ETC2_EAC                , false, true);
    AddES3TexFormat(ptr, EffectiveFormat::COMPRESSED_SRGB8_ALPHA8_ETC2_EAC         , false, true);
    AddES3TexFormat(ptr, EffectiveFormat::COMPRESSED_R11_EAC                       , false, true);
    AddES3TexFormat(ptr, EffectiveFormat::COMPRESSED_RG11_EAC                      , false, true);
    AddES3TexFormat(ptr, EffectiveFormat::COMPRESSED_SIGNED_R11_EAC                , false, true);
    AddES3TexFormat(ptr, EffectiveFormat::COMPRESSED_SIGNED_RG11_EAC               , false, true);
    AddES3TexFormat(ptr, EffectiveFormat::COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2 , false, true);
    AddES3TexFormat(ptr, EffectiveFormat::COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2, false, true);

    ////////////////////////////////////////////////////////////////////////////
    // GLES 3.0.4 p111-113
    // RGBA
    AddUnsizedUnpack(ret, EffectiveFormat::RGBA8       , LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE              );
    AddUnsizedUnpack(ret, EffectiveFormat::RGBA4       , LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_SHORT_4_4_4_4     );
    AddUnsizedUnpack(ret, EffectiveFormat::RGBA4       , LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE              );
    AddUnsizedUnpack(ret, EffectiveFormat::RGB5_A1     , LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_SHORT_5_5_5_1     );
    AddUnsizedUnpack(ret, EffectiveFormat::RGB5_A1     , LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE              );
    AddUnsizedUnpack(ret, EffectiveFormat::RGB5_A1     , LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_INT_2_10_10_10_REV);
    AddUnsizedUnpack(ret, EffectiveFormat::SRGB8_ALPHA8, LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE              );
    AddUnsizedUnpack(ret, EffectiveFormat::RGBA8_SNORM , LOCAL_GL_RGBA, LOCAL_GL_BYTE                       );
    AddUnsizedUnpack(ret, EffectiveFormat::RGB10_A2    , LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_INT_2_10_10_10_REV);
    AddUnsizedUnpack(ret, EffectiveFormat::RGBA16F     , LOCAL_GL_RGBA, LOCAL_GL_HALF_FLOAT                 );
    AddUnsizedUnpack(ret, EffectiveFormat::RGBA16F     , LOCAL_GL_RGBA, LOCAL_GL_FLOAT                      );
    AddUnsizedUnpack(ret, EffectiveFormat::RGBA32F     , LOCAL_GL_RGBA, LOCAL_GL_FLOAT                      );

    // RGBA_INTEGER
    AddUnsizedUnpack(ret, EffectiveFormat::RGBA8UI   , LOCAL_GL_RGBA_INTEGER, LOCAL_GL_UNSIGNED_BYTE              );
    AddUnsizedUnpack(ret, EffectiveFormat::RGBA8I    , LOCAL_GL_RGBA_INTEGER, LOCAL_GL_BYTE                       );
    AddUnsizedUnpack(ret, EffectiveFormat::RGBA16UI  , LOCAL_GL_RGBA_INTEGER, LOCAL_GL_UNSIGNED_SHORT             );
    AddUnsizedUnpack(ret, EffectiveFormat::RGBA16I   , LOCAL_GL_RGBA_INTEGER, LOCAL_GL_SHORT                      );
    AddUnsizedUnpack(ret, EffectiveFormat::RGBA32UI  , LOCAL_GL_RGBA_INTEGER, LOCAL_GL_UNSIGNED_INT               );
    AddUnsizedUnpack(ret, EffectiveFormat::RGBA32I   , LOCAL_GL_RGBA_INTEGER, LOCAL_GL_INT                        );
    AddUnsizedUnpack(ret, EffectiveFormat::RGB10_A2UI, LOCAL_GL_RGBA_INTEGER, LOCAL_GL_UNSIGNED_INT_2_10_10_10_REV);

    // RGB
    AddUnsizedUnpack(ret, EffectiveFormat::RGB8          , LOCAL_GL_RGB, LOCAL_GL_UNSIGNED_BYTE               );
    AddUnsizedUnpack(ret, EffectiveFormat::SRGB8         , LOCAL_GL_RGB, LOCAL_GL_UNSIGNED_BYTE               );
    AddUnsizedUnpack(ret, EffectiveFormat::RGB565        , LOCAL_GL_RGB, LOCAL_GL_UNSIGNED_SHORT_5_6_5        );
    AddUnsizedUnpack(ret, EffectiveFormat::RGB565        , LOCAL_GL_RGB, LOCAL_GL_UNSIGNED_BYTE               );
    AddUnsizedUnpack(ret, EffectiveFormat::RGB8_SNORM    , LOCAL_GL_RGB, LOCAL_GL_BYTE                        );
    AddUnsizedUnpack(ret, EffectiveFormat::R11F_G11F_B10F, LOCAL_GL_RGB, LOCAL_GL_UNSIGNED_INT_10F_11F_11F_REV);
    AddUnsizedUnpack(ret, EffectiveFormat::R11F_G11F_B10F, LOCAL_GL_RGB, LOCAL_GL_HALF_FLOAT                  );
    AddUnsizedUnpack(ret, EffectiveFormat::R11F_G11F_B10F, LOCAL_GL_RGB, LOCAL_GL_FLOAT                       );
    AddUnsizedUnpack(ret, EffectiveFormat::RGB16F        , LOCAL_GL_RGB, LOCAL_GL_HALF_FLOAT                  );
    AddUnsizedUnpack(ret, EffectiveFormat::RGB16F        , LOCAL_GL_RGB, LOCAL_GL_FLOAT                       );
    AddUnsizedUnpack(ret, EffectiveFormat::RGB9_E5       , LOCAL_GL_RGB, LOCAL_GL_UNSIGNED_INT_5_9_9_9_REV    );
    AddUnsizedUnpack(ret, EffectiveFormat::RGB9_E5       , LOCAL_GL_RGB, LOCAL_GL_HALF_FLOAT                  );
    AddUnsizedUnpack(ret, EffectiveFormat::RGB9_E5       , LOCAL_GL_RGB, LOCAL_GL_FLOAT                       );
    AddUnsizedUnpack(ret, EffectiveFormat::RGB32F        , LOCAL_GL_RGB, LOCAL_GL_FLOAT                       );

    // RGB_INTEGER
    AddUnsizedUnpack(ret, EffectiveFormat::RGB8UI , LOCAL_GL_RGB_INTEGER, LOCAL_GL_UNSIGNED_BYTE );
    AddUnsizedUnpack(ret, EffectiveFormat::RGB8I  , LOCAL_GL_RGB_INTEGER, LOCAL_GL_BYTE          );
    AddUnsizedUnpack(ret, EffectiveFormat::RGB16UI, LOCAL_GL_RGB_INTEGER, LOCAL_GL_UNSIGNED_SHORT);
    AddUnsizedUnpack(ret, EffectiveFormat::RGB16I , LOCAL_GL_RGB_INTEGER, LOCAL_GL_SHORT         );
    AddUnsizedUnpack(ret, EffectiveFormat::RGB32UI, LOCAL_GL_RGB_INTEGER, LOCAL_GL_UNSIGNED_INT  );
    AddUnsizedUnpack(ret, EffectiveFormat::RGB32I , LOCAL_GL_RGB_INTEGER, LOCAL_GL_INT           );

    // RG
    AddUnsizedUnpack(ret, EffectiveFormat::RG8      , LOCAL_GL_RG, LOCAL_GL_UNSIGNED_BYTE);
    AddUnsizedUnpack(ret, EffectiveFormat::RG8_SNORM, LOCAL_GL_RG, LOCAL_GL_BYTE         );
    AddUnsizedUnpack(ret, EffectiveFormat::RG16F    , LOCAL_GL_RG, LOCAL_GL_HALF_FLOAT   );
    AddUnsizedUnpack(ret, EffectiveFormat::RG16F    , LOCAL_GL_RG, LOCAL_GL_FLOAT        );
    AddUnsizedUnpack(ret, EffectiveFormat::RG32F    , LOCAL_GL_RG, LOCAL_GL_FLOAT        );

    // RG_INTEGER
    AddUnsizedUnpack(ret, EffectiveFormat::RG8UI , LOCAL_GL_RG_INTEGER, LOCAL_GL_UNSIGNED_BYTE );
    AddUnsizedUnpack(ret, EffectiveFormat::RG8I  , LOCAL_GL_RG_INTEGER, LOCAL_GL_BYTE          );
    AddUnsizedUnpack(ret, EffectiveFormat::RG16UI, LOCAL_GL_RG_INTEGER, LOCAL_GL_UNSIGNED_SHORT);
    AddUnsizedUnpack(ret, EffectiveFormat::RG16I , LOCAL_GL_RG_INTEGER, LOCAL_GL_SHORT         );
    AddUnsizedUnpack(ret, EffectiveFormat::RG32UI, LOCAL_GL_RG_INTEGER, LOCAL_GL_UNSIGNED_INT  );
    AddUnsizedUnpack(ret, EffectiveFormat::RG32I , LOCAL_GL_RG_INTEGER, LOCAL_GL_INT           );

    // RED
    AddUnsizedUnpack(ret, EffectiveFormat::R8      , LOCAL_GL_RED, LOCAL_GL_UNSIGNED_BYTE);
    AddUnsizedUnpack(ret, EffectiveFormat::R8_SNORM, LOCAL_GL_RED, LOCAL_GL_BYTE         );
    AddUnsizedUnpack(ret, EffectiveFormat::R16F    , LOCAL_GL_RED, LOCAL_GL_HALF_FLOAT   );
    AddUnsizedUnpack(ret, EffectiveFormat::R16F    , LOCAL_GL_RED, LOCAL_GL_FLOAT        );
    AddUnsizedUnpack(ret, EffectiveFormat::R32F    , LOCAL_GL_RED, LOCAL_GL_FLOAT        );

    // RED_INTEGER
    AddUnsizedUnpack(ret, EffectiveFormat::R8UI , LOCAL_GL_RED_INTEGER, LOCAL_GL_UNSIGNED_BYTE );
    AddUnsizedUnpack(ret, EffectiveFormat::R8I  , LOCAL_GL_RED_INTEGER, LOCAL_GL_BYTE          );
    AddUnsizedUnpack(ret, EffectiveFormat::R16UI, LOCAL_GL_RED_INTEGER, LOCAL_GL_UNSIGNED_SHORT);
    AddUnsizedUnpack(ret, EffectiveFormat::R16I , LOCAL_GL_RED_INTEGER, LOCAL_GL_SHORT         );
    AddUnsizedUnpack(ret, EffectiveFormat::R32UI, LOCAL_GL_RED_INTEGER, LOCAL_GL_UNSIGNED_INT  );
    AddUnsizedUnpack(ret, EffectiveFormat::R32I , LOCAL_GL_RED_INTEGER, LOCAL_GL_INT           );

    // DEPTH_COMPONENT
    AddUnsizedUnpack(ret, EffectiveFormat::DEPTH_COMPONENT16 , LOCAL_GL_DEPTH_COMPONENT, LOCAL_GL_UNSIGNED_SHORT);
    AddUnsizedUnpack(ret, EffectiveFormat::DEPTH_COMPONENT16 , LOCAL_GL_DEPTH_COMPONENT, LOCAL_GL_UNSIGNED_INT  );
    AddUnsizedUnpack(ret, EffectiveFormat::DEPTH_COMPONENT24 , LOCAL_GL_DEPTH_COMPONENT, LOCAL_GL_UNSIGNED_INT  );
    AddUnsizedUnpack(ret, EffectiveFormat::DEPTH_COMPONENT32F, LOCAL_GL_DEPTH_COMPONENT, LOCAL_GL_FLOAT         );

    // DEPTH_STENCIL
    AddUnsizedUnpack(ret, EffectiveFormat::DEPTH24_STENCIL8 , LOCAL_GL_DEPTH_STENCIL, LOCAL_GL_UNSIGNED_INT_24_8             );
    AddUnsizedUnpack(ret, EffectiveFormat::DEPTH32F_STENCIL8, LOCAL_GL_DEPTH_STENCIL, LOCAL_GL_FLOAT_32_UNSIGNED_INT_24_8_REV);

    // L, A, LA
    AddLegacyUnpacks_LA8(ret, gl);

    return Move(ret);
}

//////////////////////////////////////////////////////////////////////////////////////////

FormatUsageInfo*
FormatUsageAuthority::EditUsage(EffectiveFormat format)
{
    auto itr = mInfoMap.find(format);

    if (itr == mInfoMap.end()) {
        const FormatInfo* formatInfo = GetFormatInfo(format);
        MOZ_RELEASE_ASSERT(formatInfo);

        FormatUsageInfo usage(formatInfo);
        itr = AlwaysInsert(mInfoMap, format, usage);
    }

    const auto& inPlaceValue = itr->second;
    return &inPlaceValue;
}

const FormatUsageInfo*
FormatUsageAuthority::GetUsage(EffectiveFormat format) const
{
    auto itr = mInfoMap.find(format);
    if (itr == mInfoMap.end())
        return nullptr;

    const auto& inPlaceValue = itr->second;
    return &inPlaceValue;
}

////////////////////////////////////////////////////////////////////////////////

struct ComponentSizes
{
    GLubyte redSize;
    GLubyte greenSize;
    GLubyte blueSize;
    GLubyte alphaSize;
    GLubyte depthSize;
    GLubyte stencilSize;
};

static ComponentSizes kComponentSizes[] = {
    // GLES 3.0.4, p128-129, "Required Texture Formats"
    // "Texture and renderbuffer color formats"
    { 32, 32, 32, 32,  0,  0 }, // RGBA32I,
    { 32, 32, 32, 32,  0,  0 }, // RGBA32UI,
    { 16, 16, 16, 16,  0,  0 }, // RGBA16I,
    { 16, 16, 16, 16,  0,  0 }, // RGBA16UI,
    {  8,  8,  8,  8,  0,  0 }, // RGBA8,
    {  8,  8,  8,  8,  0,  0 }, // RGBA8I,
    {  8,  8,  8,  8,  0,  0 }, // RGBA8UI,
    {  8,  8,  8,  8,  0,  0 }, // SRGB8_ALPHA8,
    { 10, 10, 10,  2,  0,  0 }, // RGB10_A2,
    { 10, 10, 10,  2,  0,  0 }, // RGB10_A2UI,
    {  4,  4,  4,  4,  0,  0 }, // RGBA4,
    {  5,  5,  5,  1,  0,  0 }, // RGB5_A1,

    {  8,  8,  8,  0,  0,  0 }, // RGB8,
    {  8,  8,  8,  0,  0,  0 }, // RGB565,

    { 32, 32,  0,  0,  0,  0 }, // RG32I,
    { 32, 32,  0,  0,  0,  0 }, // RG32UI,
    { 16, 16,  0,  0,  0,  0 }, // RG16I,
    { 16, 16,  0,  0,  0,  0 }, // RG16UI,
    {  8,  8,  0,  0,  0,  0 }, // RG8,
    {  8,  8,  0,  0,  0,  0 }, // RG8I,
    {  8,  8,  0,  0,  0,  0 }, // RG8UI,

    { 32,  0,  0,  0,  0,  0 }, // R32I,
    { 32,  0,  0,  0,  0,  0 }, // R32UI,
    { 16,  0,  0,  0,  0,  0 }, // R16I,
    { 16,  0,  0,  0,  0,  0 }, // R16UI,
    {  8,  0,  0,  0,  0,  0 }, // R8,
    {  8,  0,  0,  0,  0,  0 }, // R8I,
    {  8,  0,  0,  0,  0,  0 }, // R8UI,

    // "Texture-only color formats"
    { 32, 32, 32, 32,  0,  0 }, // RGBA32F,
    { 16, 16, 16, 16,  0,  0 }, // RGBA16F,
    {  8,  8,  8,  8,  0,  0 }, // RGBA8_SNORM,

    { 32, 32, 32,  0,  0,  0 }, // RGB32F,
    { 32, 32, 32,  0,  0,  0 }, // RGB32I,
    { 32, 32, 32,  0,  0,  0 }, // RGB32UI,

    { 16, 16, 16,  0,  0,  0 }, // RGB16F,
    { 16, 16, 16,  0,  0,  0 }, // RGB16I,
    { 16, 16, 16,  0,  0,  0 }, // RGB16UI,

    {  8,  8,  8,  0,  0,  0 }, // RGB8_SNORM,
    {  8,  8,  8,  0,  0,  0 }, // RGB8I,
    {  8,  8,  8,  0,  0,  0 }, // RGB8UI,
    {  8,  8,  8,  0,  0,  0 }, // SRGB8,

    { 11, 11, 11,  0,  0,  0 }, // R11F_G11F_B10F,
    {  9,  9,  9,  0,  0,  0 }, // RGB9_E5,

    { 32, 32,  0,  0,  0,  0 }, // RG32F,
    { 16, 16,  0,  0,  0,  0 }, // RG16F,
    {  8,  8,  0,  0,  0,  0 }, // RG8_SNORM,

    { 32,  0,  0,  0,  0,  0 }, // R32F,
    { 16,  0,  0,  0,  0,  0 }, // R16F,
    {  8,  0,  0,  0,  0,  0 }, // R8_SNORM,

    // "Depth formats"
    {  0,  0,  0,  0, 32,  0 }, // DEPTH_COMPONENT32F,
    {  0,  0,  0,  0, 24,  0 }, // DEPTH_COMPONENT24,
    {  0,  0,  0,  0, 16,  0 }, // DEPTH_COMPONENT16,

    // "Combined depth+stencil formats"
    {  0,  0,  0,  0, 32,  8 }, // DEPTH32F_STENCIL0,
    {  0,  0,  0,  0, 24,  8 }, // DEPTH24_STENCIL8,

    // GLES 3.0.4, p205-206, "Required Renderbuffer Formats"
    {  0,  0,  0,  0,  0,  8 }, // STENCIL_INDEX8,

    // GLES 3.0.4, p128, table 3.12.
    {  8,  8,  8,  8,  0,  0 }, // Luminance8Alpha8,
    {  8,  8,  8,  0,  0,  0 }, // Luminance8,
    {  0,  0,  0,  8,  0,  0 }, // Alpha8,

    // GLES 3.0.4, p147, table 3.19
    // GLES 3.0.4, p286+, $C.1 "ETC Compressed Texture Image Formats"
    {  8,  8,  8,  8,  0,  0 }, // COMPRESSED_R11_EAC,
    {  8,  8,  8,  8,  0,  0 }, // COMPRESSED_SIGNED_R11_EAC,
    {  8,  8,  8,  8,  0,  0 }, // COMPRESSED_RG11_EAC,
    {  8,  8,  8,  8,  0,  0 }, // COMPRESSED_SIGNED_RG11_EAC,
    {  8,  8,  8,  8,  0,  0 }, // COMPRESSED_RGB8_ETC2,
    {  8,  8,  8,  8,  0,  0 }, // COMPRESSED_SRGB8_ETC2,
    {  8,  8,  8,  8,  0,  0 }, // COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,
    {  8,  8,  8,  8,  0,  0 }, // COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2,
    {  8,  8,  8,  8,  0,  0 }, // COMPRESSED_RGBA8_ETC2_EAC,
    {  8,  8,  8,  8,  0,  0 }, // COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,

    // AMD_compressed_ATC_texture
    {  8,  8,  8,  0,  0,  0 }, // ATC_RGB_AMD,
    {  8,  8,  8,  8,  0,  0 }, // ATC_RGBA_EXPLICIT_ALPHA_AMD,
    {  8,  8,  8,  8,  0,  0 }, // ATC_RGBA_INTERPOLATED_ALPHA_AMD,

    // EXT_texture_compression_s3tc
    {  8,  8,  8,  0,  0,  0 }, // COMPRESSED_RGB_S3TC_DXT1,
    {  8,  8,  8,  8,  0,  0 }, // COMPRESSED_RGBA_S3TC_DXT1,
    {  8,  8,  8,  8,  0,  0 }, // COMPRESSED_RGBA_S3TC_DXT3,
    {  8,  8,  8,  8,  0,  0 }, // COMPRESSED_RGBA_S3TC_DXT5,

    // IMG_texture_compression_pvrtc
    {  8,  8,  8,  0,  0,  0 }, // COMPRESSED_RGB_PVRTC_4BPPV1,
    {  8,  8,  8,  8,  0,  0 }, // COMPRESSED_RGBA_PVRTC_4BPPV1,
    {  8,  8,  8,  0,  0,  0 }, // COMPRESSED_RGB_PVRTC_2BPPV1,
    {  8,  8,  8,  8,  0,  0 }, // COMPRESSED_RGBA_PVRTC_2BPPV1,

    // OES_compressed_ETC1_RGB8_texture
    {  8,  8,  8,  0,  0,  0 }, // ETC1_RGB8,

    // OES_texture_float
    { 32, 32, 32, 32,  0,  0 }, // Luminance32FAlpha32F,
    { 32, 32, 32,  0,  0,  0 }, // Luminance32F,
    {  0,  0,  0, 32,  0,  0 }, // Alpha32F,

    // OES_texture_half_float
    { 16, 16, 16, 16, 0, 0 }, // Luminance16FAlpha16F,
    { 16, 16, 16,  0, 0, 0 }, // Luminance16F,
    {  0,  0,  0, 16, 0, 0 }, // Alpha16F,

    {  0, } // MAX
};

GLint
GetComponentSize(EffectiveFormat format, GLenum component)
{
    ComponentSizes compSize = kComponentSizes[(int) format];
    switch (component) {
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE:
    case LOCAL_GL_RENDERBUFFER_RED_SIZE:
    case LOCAL_GL_RED_BITS:
        return compSize.redSize;
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE:
    case LOCAL_GL_RENDERBUFFER_GREEN_SIZE:
    case LOCAL_GL_GREEN_BITS:
        return compSize.greenSize;
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE:
    case LOCAL_GL_RENDERBUFFER_BLUE_SIZE:
    case LOCAL_GL_BLUE_BITS:
        return compSize.blueSize;
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE:
    case LOCAL_GL_RENDERBUFFER_ALPHA_SIZE:
    case LOCAL_GL_ALPHA_BITS:
        return compSize.alphaSize;
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE:
    case LOCAL_GL_RENDERBUFFER_DEPTH_SIZE:
    case LOCAL_GL_DEPTH_BITS:
        return compSize.depthSize;
    case LOCAL_GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE:
    case LOCAL_GL_RENDERBUFFER_STENCIL_SIZE:
    case LOCAL_GL_STENCIL_BITS:
        return compSize.stencilSize;
    }

    return 0;
}

static GLenum kComponentTypes[] = {
    // GLES 3.0.4, p128-129, "Required Texture Formats"
    // "Texture and renderbuffer color formats"
    LOCAL_GL_INT,                       // RGBA32I,
    LOCAL_GL_UNSIGNED_INT,              // RGBA32UI,
    LOCAL_GL_INT,                       // RGBA16I,
    LOCAL_GL_UNSIGNED_INT,              // RGBA16UI,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // RGBA8,
    LOCAL_GL_INT,                       // RGBA8I,
    LOCAL_GL_UNSIGNED_INT,              // RGBA8UI,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // SRGB8_ALPHA8,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // RGB10_A2,
    LOCAL_GL_UNSIGNED_INT,              // RGB10_A2UI,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // RGBA4,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // RGB5_A1,

    LOCAL_GL_UNSIGNED_NORMALIZED,       // RGB8,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // RGB565,

    LOCAL_GL_INT,                       // RG32I,
    LOCAL_GL_UNSIGNED_INT,              // RG32UI,
    LOCAL_GL_INT,                       // RG16I,
    LOCAL_GL_UNSIGNED_INT,              // RG16UI,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // RG8,
    LOCAL_GL_INT,                       // RG8I,
    LOCAL_GL_UNSIGNED_INT,              // RG8UI,

    LOCAL_GL_INT,                       // R32I,
    LOCAL_GL_UNSIGNED_INT,              // R32UI,
    LOCAL_GL_INT,                       // R16I,
    LOCAL_GL_UNSIGNED_INT,              // R16UI,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // R8,
    LOCAL_GL_INT,                       // R8I,
    LOCAL_GL_UNSIGNED_INT,              // R8UI,

    // "Texture-only color formats"
    LOCAL_GL_FLOAT,                     // RGBA32F,
    LOCAL_GL_FLOAT,                     // RGBA16F,
    LOCAL_GL_SIGNED_NORMALIZED,         // RGBA8_SNORM,

    LOCAL_GL_FLOAT,                     // RGB32F,
    LOCAL_GL_INT,                       // RGB32I,
    LOCAL_GL_UNSIGNED_INT,              // RGB32UI,

    LOCAL_GL_FLOAT,                     // RGB16F,
    LOCAL_GL_INT,                       // RGB16I,
    LOCAL_GL_UNSIGNED_INT,              // RGB16UI,

    LOCAL_GL_SIGNED_NORMALIZED,         // RGB8_SNORM,
    LOCAL_GL_INT,                       // RGB8I,
    LOCAL_GL_UNSIGNED_INT,              // RGB8UI,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // SRGB8,

    LOCAL_GL_FLOAT,                     // R11F_G11F_B10F,
    LOCAL_GL_FLOAT,                     // RGB9_E5,

    LOCAL_GL_FLOAT,                     // RG32F,
    LOCAL_GL_FLOAT,                     // RG16F,
    LOCAL_GL_SIGNED_NORMALIZED,         // RG8_SNORM,

    LOCAL_GL_FLOAT,                     // R32F,
    LOCAL_GL_FLOAT,                     // R16F,
    LOCAL_GL_SIGNED_NORMALIZED,         // R8_SNORM,

    // "Depth formats"
    LOCAL_GL_FLOAT,                     // DEPTH_COMPONENT32F,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // DEPTH_COMPONENT24,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // DEPTH_COMPONENT16,

    // "Combined depth+stencil formats"
    LOCAL_GL_FLOAT,                     // DEPTH32F_STENCIL8,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // DEPTH24_STENCIL8,

    // GLES 3.0.4, p205-206, "Required Renderbuffer Formats"
    LOCAL_GL_UNSIGNED_NORMALIZED,       // STENCIL_INDEX8,

    // GLES 3.0.4, p128, table 3.12.
    LOCAL_GL_UNSIGNED_NORMALIZED,       // Luminance8Alpha8,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // Luminance8,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // Alpha8,

    // GLES 3.0.4, p147, table 3.19
    // GLES 3.0.4, p286+, $C.1 "ETC Compressed Texture Image Formats"
    LOCAL_GL_UNSIGNED_NORMALIZED,       // COMPRESSED_R11_EAC,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // COMPRESSED_SIGNED_R11_EAC,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // COMPRESSED_RG11_EAC,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // COMPRESSED_SIGNED_RG11_EAC,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // COMPRESSED_RGB8_ETC2,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // COMPRESSED_SRGB8_ETC2,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // COMPRESSED_RGBA8_ETC2_EAC,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // COMPRESSED_SRGB8_ALPHA8_ETC2_EAC,

    // AMD_compressed_ATC_texture
    LOCAL_GL_UNSIGNED_NORMALIZED,       // ATC_RGB_AMD,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // ATC_RGBA_EXPLICIT_ALPHA_AMD,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // ATC_RGBA_INTERPOLATED_ALPHA_AMD,

    // EXT_texture_compression_s3tc
    LOCAL_GL_UNSIGNED_NORMALIZED,       // COMPRESSED_RGB_S3TC_DXT1,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // COMPRESSED_RGBA_S3TC_DXT1,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // COMPRESSED_RGBA_S3TC_DXT3,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // COMPRESSED_RGBA_S3TC_DXT5,

    // IMG_texture_compression_pvrtc
    LOCAL_GL_UNSIGNED_NORMALIZED,       // COMPRESSED_RGB_PVRTC_4BPPV1,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // COMPRESSED_RGBA_PVRTC_4BPPV1,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // COMPRESSED_RGB_PVRTC_2BPPV1,
    LOCAL_GL_UNSIGNED_NORMALIZED,       // COMPRESSED_RGBA_PVRTC_2BPPV1,

    // OES_compressed_ETC1_RGB8_texture
    LOCAL_GL_UNSIGNED_NORMALIZED,       // ETC1_RGB8,

    // OES_texture_float
    LOCAL_GL_FLOAT,                     // Luminance32FAlpha32F,
    LOCAL_GL_FLOAT,                     // Luminance32F,
    LOCAL_GL_FLOAT,                     // Alpha32F,

    // OES_texture_half_float
    LOCAL_GL_FLOAT,                     // Luminance16FAlpha16F,
    LOCAL_GL_FLOAT,                     // Luminance16F,
    LOCAL_GL_FLOAT,                     // Alpha16F,

    LOCAL_GL_NONE // MAX
};

GLenum
GetComponentType(EffectiveFormat format)
{
    return kComponentTypes[(int) format];
}

GLenum
GetColorEncoding(EffectiveFormat format)
{
    const bool isSRGB = (GetFormatInfo(format)->colorComponentType ==
                         ComponentType::NormUIntSRGB);
    return (isSRGB) ? LOCAL_GL_SRGB : LOCAL_GL_LINEAR;
}

} // namespace webgl
} // namespace mozilla
