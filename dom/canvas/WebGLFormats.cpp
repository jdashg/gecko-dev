/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLFormats.h"

#include "GLDefs.h"
#include "mozilla/StaticMutex.h"

#ifdef FOO
#error FOO is already defined! We use FOO() macros to keep things succinct in this file.
#endif

namespace mozilla {
namespace webgl {

// Returns an iterator to the in-place pair.
template<typename K, typename V, typename K2, typename V2>
static inline auto
AlwaysInsert(std::map<K,V>& dest, const K2& key, const V2& val)
{
    auto res = dest.insert({ key, val });
    DebugOnly<bool> didInsert = res.second;
    MOZ_ASSERT(didInsert);

    return res.first;
}

template<typename K, typename V, typename K2>
static inline V*
FindOrNull(const std::map<K,V*>& dest, const K2& key)
{
    auto itr = dest.find(key);
    if (itr == dest.end())
        return nullptr;

    return itr->second;
}

// Returns a pointer to the in-place value for `key`.
template<typename K, typename V, typename K2>
static inline V*
FindPtrOrNull(const std::map<K,V>& dest, const K2& key)
{
    auto itr = dest.find(key);
    if (itr == dest.end())
        return nullptr;

    return &(itr->second);
}

//////////////////////////////////////////////////////////////////////////////////////////

std::map<EffectiveFormat, const CompressedFormatInfo> gCompressedFormatInfoMap;
std::map<EffectiveFormat, const FormatInfo> gFormatInfoMap;

static inline const CompressedFormatInfo*
GetCompressedFormatInfo(EffectiveFormat format)
{
    MOZ_ASSERT(!gCompressedFormatInfoMap.empty());
    return FindPtrOrNull(gCompressedFormatInfoMap, format);
}

static inline const FormatInfo*
GetFormatInfo_NoLock(EffectiveFormat format)
{
    MOZ_ASSERT(!gFormatInfoMap.empty());
    return FindPtrOrNull(gFormatInfoMap, format);
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
    AddCompressedFormatInfo(EffectiveFormat::COMPRESSED_RGB_S3TC_DXT1_EXT ,  64, 4, 4, false, SubImageUpdateBehavior::BlockAligned);
    AddCompressedFormatInfo(EffectiveFormat::COMPRESSED_RGBA_S3TC_DXT1_EXT,  64, 4, 4, false, SubImageUpdateBehavior::BlockAligned);
    AddCompressedFormatInfo(EffectiveFormat::COMPRESSED_RGBA_S3TC_DXT3_EXT, 128, 4, 4, false, SubImageUpdateBehavior::BlockAligned);
    AddCompressedFormatInfo(EffectiveFormat::COMPRESSED_RGBA_S3TC_DXT5_EXT, 128, 4, 4, false, SubImageUpdateBehavior::BlockAligned);

    // IMG_texture_compression_pvrtc
    AddCompressedFormatInfo(EffectiveFormat::COMPRESSED_RGB_PVRTC_4BPPV1 , 256,  8, 8, true, SubImageUpdateBehavior::FullOnly);
    AddCompressedFormatInfo(EffectiveFormat::COMPRESSED_RGBA_PVRTC_4BPPV1, 256,  8, 8, true, SubImageUpdateBehavior::FullOnly);
    AddCompressedFormatInfo(EffectiveFormat::COMPRESSED_RGB_PVRTC_2BPPV1 , 256, 16, 8, true, SubImageUpdateBehavior::FullOnly);
    AddCompressedFormatInfo(EffectiveFormat::COMPRESSED_RGBA_PVRTC_2BPPV1, 256, 16, 8, true, SubImageUpdateBehavior::FullOnly);

    // OES_compressed_ETC1_RGB8_texture
    AddCompressedFormatInfo(EffectiveFormat::ETC1_RGB8_OES, 64, 4, 4, false, SubImageUpdateBehavior::Forbidden);
}

//////////////////////////////////////////////////////////////////////////////////////////

static void
AddFormatInfo(EffectiveFormat format, const char* name, GLenum sizedFormat,
              uint8_t bytesPerPixel, UnsizedFormat unsizedFormat, bool isSRGB,
              ComponentType componentType)
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

    const FormatInfo info = { format, name, sizedFormat, unsizedFormat, componentType,
                              bytesPerPixel, isColorFormat, isSRGB, hasAlpha, hasDepth,
                              hasStencil, compressedFormatInfo };
    const auto itr = AlwaysInsert(gFormatInfoMap, format, info);
}

static void
InitFormatInfo()
{
#define FOO(x) EffectiveFormat::x, #x, LOCAL_GL_ ## x

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
    AddFormatInfo(FOO(COMPRESSED_RGB_S3TC_DXT1_EXT ), 0, UnsizedFormat::RGB , false, ComponentType::NormUInt);
    AddFormatInfo(FOO(COMPRESSED_RGBA_S3TC_DXT1_EXT), 0, UnsizedFormat::RGBA, false, ComponentType::NormUInt);
    AddFormatInfo(FOO(COMPRESSED_RGBA_S3TC_DXT3_EXT), 0, UnsizedFormat::RGBA, false, ComponentType::NormUInt);
    AddFormatInfo(FOO(COMPRESSED_RGBA_S3TC_DXT5_EXT), 0, UnsizedFormat::RGBA, false, ComponentType::NormUInt);

    // IMG_texture_compression_pvrtc
    AddFormatInfo(FOO(COMPRESSED_RGB_PVRTC_4BPPV1 ), 0, UnsizedFormat::RGB , false, ComponentType::NormUInt);
    AddFormatInfo(FOO(COMPRESSED_RGBA_PVRTC_4BPPV1), 0, UnsizedFormat::RGBA, false, ComponentType::NormUInt);
    AddFormatInfo(FOO(COMPRESSED_RGB_PVRTC_2BPPV1 ), 0, UnsizedFormat::RGB , false, ComponentType::NormUInt);
    AddFormatInfo(FOO(COMPRESSED_RGBA_PVRTC_2BPPV1), 0, UnsizedFormat::RGBA, false, ComponentType::NormUInt);

    // OES_compressed_ETC1_RGB8_texture
    AddFormatInfo(FOO(ETC1_RGB8_OES), 0, UnsizedFormat::RGB, false, ComponentType::NormUInt);

#undef FOO

    // 'Virtual' effective formats have no sizedFormat.
#define FOO(x) EffectiveFormat::x, #x, 0

    // GLES 3.0.4, p128, table 3.12.
    AddFormatInfo(FOO(Luminance8Alpha8), 2, UnsizedFormat::LA, false, ComponentType::NormUInt);
    AddFormatInfo(FOO(Luminance8      ), 1, UnsizedFormat::L , false, ComponentType::NormUInt);
    AddFormatInfo(FOO(Alpha8          ), 1, UnsizedFormat::A , false, ComponentType::NormUInt);

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

bool gAreFormatTablesInitialized = false;

static void
EnsureInitFormatTables(const StaticMutexAutoLock&) // Prove that you locked it!
{
    if (MOZ_LIKELY(gAreFormatTablesInitialized))
        return;

    gAreFormatTablesInitialized = true;

    InitCompressedFormatInfo();
    InitFormatInfo();
}

//////////////////////////////////////////////////////////////////////////////////////////
// Public funcs

StaticMutex gFormatMapMutex;

const FormatInfo*
GetFormat(EffectiveFormat format)
{
    StaticMutexAutoLock lock(gFormatMapMutex);
    EnsureInitFormatTables(lock);

    return GetFormatInfo_NoLock(format);
}

//////////////////////////////////////////////////////////////////////////////////////////

uint8_t
BytesPerPixel(const PackingInfo& packing)
{
    uint8_t bytesPerChannel;
    switch (packing.type) {
    case LOCAL_GL_UNSIGNED_SHORT_4_4_4_4:
    case LOCAL_GL_UNSIGNED_SHORT_5_5_5_1:
    case LOCAL_GL_UNSIGNED_SHORT_5_6_5:
        return 2;

    case LOCAL_GL_UNSIGNED_INT_10F_11F_11F_REV:
    case LOCAL_GL_UNSIGNED_INT_2_10_10_10_REV:
    case LOCAL_GL_UNSIGNED_INT_24_8:
    case LOCAL_GL_UNSIGNED_INT_5_9_9_9_REV:
        return 4;

    case LOCAL_GL_FLOAT_32_UNSIGNED_INT_24_8_REV:
        return 8;

    // Alright, that's all the fixed-size unpackTypes.

    case LOCAL_GL_BYTE:
    case LOCAL_GL_UNSIGNED_BYTE:
        bytesPerChannel = 1;
        break;

    case LOCAL_GL_SHORT:
    case LOCAL_GL_UNSIGNED_SHORT:
    case LOCAL_GL_HALF_FLOAT:
    case LOCAL_GL_HALF_FLOAT_OES:
        bytesPerChannel = 2;
        break;

    case LOCAL_GL_INT:
    case LOCAL_GL_UNSIGNED_INT:
    case LOCAL_GL_FLOAT:
        bytesPerChannel = 4;
        break;

    default:
        MOZ_CRASH("invalid PackingInfo");
    }

    uint8_t channels;
    switch (packing.format) {
    case LOCAL_GL_RG:
    case LOCAL_GL_RG_INTEGER:
    case LOCAL_GL_LUMINANCE_ALPHA:
        channels = 2;
        break;

    case LOCAL_GL_RGB:
    case LOCAL_GL_RGB_INTEGER:
        channels = 3;
        break;

    case LOCAL_GL_RGBA:
    case LOCAL_GL_RGBA_INTEGER:
        channels = 4;
        break;

    default:
        channels = 1;
        break;
    }

    return bytesPerChannel * channels;
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
    auto itr = AlwaysInsert(validUnpacks, key, value);

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
SetUsage(FormatUsageAuthority* fua, EffectiveFormat effFormat, bool isRenderable,
         bool isFilterable)
{
    MOZ_ASSERT(!fua->GetUsage(effFormat));

    auto usage = fua->EditUsage(effFormat);
    usage->isRenderable = isRenderable;
    usage->isFilterable = isFilterable;
}

static void
AddLegacyFormats_LA8(FormatUsageAuthority* fua, gl::GLContext* gl)
{
    PackingInfo pi;
    DriverUnpackInfo dui;

    const auto fnAdd = [fua, &pi, &dui](EffectiveFormat effFormat) {
        auto usage = fua->EditUsage(effFormat);
        fua->AddUnsizedTexFormat(pi, usage);
        usage->AddUnpack(pi, dui);
    };

    const bool isCore = gl->IsCoreProfile();

    pi = {LOCAL_GL_LUMINANCE, LOCAL_GL_UNSIGNED_BYTE};
    if (isCore) dui = {LOCAL_GL_R8, LOCAL_GL_RED, LOCAL_GL_UNSIGNED_BYTE};
    else        dui = {LOCAL_GL_LUMINANCE, LOCAL_GL_LUMINANCE, LOCAL_GL_UNSIGNED_BYTE};
    fnAdd(EffectiveFormat::Luminance8);

    pi = {LOCAL_GL_ALPHA, LOCAL_GL_UNSIGNED_BYTE};
    if (isCore) dui = {LOCAL_GL_R8, LOCAL_GL_RED, LOCAL_GL_UNSIGNED_BYTE};
    else        dui = {LOCAL_GL_ALPHA, LOCAL_GL_ALPHA, LOCAL_GL_UNSIGNED_BYTE};
    fnAdd(EffectiveFormat::Alpha8);

    pi = {LOCAL_GL_LUMINANCE_ALPHA, LOCAL_GL_UNSIGNED_BYTE};
    if (isCore) dui = {LOCAL_GL_RG8, LOCAL_GL_RG, LOCAL_GL_UNSIGNED_BYTE};
    else        dui = {LOCAL_GL_LUMINANCE_ALPHA, LOCAL_GL_LUMINANCE_ALPHA, LOCAL_GL_UNSIGNED_BYTE};
    fnAdd(EffectiveFormat::Luminance8Alpha8);
}

static void
AddBasicUnsizedFormats(FormatUsageAuthority* fua, gl::GLContext* gl)
{
    const auto fnAddSimpleUnsized = [fua](GLenum unpackFormat, GLenum unpackType,
                                          EffectiveFormat effFormat)
    {
        auto usage = fua->EditUsage(effFormat);

        const PackingInfo pi = {unpackFormat, unpackType};
        fua->AddUnsizedTexFormat(pi, usage);

        const DriverUnpackInfo dui = {unpackFormat, unpackFormat, unpackType};
        usage->AddUnpack(pi, dui);
    };

    // GLES 2.0.25, p63, Table 3.4

    fnAddSimpleUnsized(LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE         , EffectiveFormat::RGBA8  );
    fnAddSimpleUnsized(LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_SHORT_4_4_4_4, EffectiveFormat::RGBA4  );
    fnAddSimpleUnsized(LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_SHORT_5_5_5_1, EffectiveFormat::RGB5_A1);
    fnAddSimpleUnsized(LOCAL_GL_RGB , LOCAL_GL_UNSIGNED_BYTE         , EffectiveFormat::RGB8   );
    fnAddSimpleUnsized(LOCAL_GL_RGB , LOCAL_GL_UNSIGNED_SHORT_5_6_5  , EffectiveFormat::RGB565 );

    // L, A, LA
    AddLegacyFormats_LA8(fua, gl);
}

UniquePtr<FormatUsageAuthority>
FormatUsageAuthority::CreateForWebGL1(gl::GLContext* gl)
{
    UniquePtr<FormatUsageAuthority> ret(new FormatUsageAuthority);
    const auto ptr = ret.get();

    ////////////////////////////////////////////////////////////////////////////
    // Usages

    // GLES 2.0.25, p117, Table 4.5
    // RGBA8 is made renderable in WebGL 1.0, "Framebuffer Object Attachments"

    //                                      render filter
    //                                      able   able
    SetUsage(ptr, EffectiveFormat::RGBA8  , true , true);
    SetUsage(ptr, EffectiveFormat::RGBA4  , true , true);
    SetUsage(ptr, EffectiveFormat::RGB5_A1, true , true);
    SetUsage(ptr, EffectiveFormat::RGB8   , false, true);
    SetUsage(ptr, EffectiveFormat::RGB565 , true , true);

    SetUsage(ptr, EffectiveFormat::Luminance8Alpha8, false, true);
    SetUsage(ptr, EffectiveFormat::Luminance8      , false, true);
    SetUsage(ptr, EffectiveFormat::Alpha8          , false, true);

    SetUsage(ptr, EffectiveFormat::DEPTH_COMPONENT16, true, false);
    SetUsage(ptr, EffectiveFormat::STENCIL_INDEX8   , true, false);

    // Added in WebGL 1.0 spec:
    SetUsage(ptr, EffectiveFormat::DEPTH24_STENCIL8, true, false);

    ////////////////////////////////////
    // RB formats

#define FOO(x) ptr->AddRBFormat(LOCAL_GL_ ## x, ptr->GetUsage(EffectiveFormat::x))

    FOO(RGBA4            );
    FOO(RGB5_A1          );
    FOO(RGB565           );
    FOO(DEPTH_COMPONENT16);
    FOO(STENCIL_INDEX8   );
    FOO(DEPTH24_STENCIL8 );

#undef FOO

    ////////////////////////////////////////////////////////////////////////////

    AddBasicUnsizedFormats(ptr, gl);

    return Move(ret);
}

UniquePtr<FormatUsageAuthority>
FormatUsageAuthority::CreateForWebGL2(gl::GLContext* gl)
{
    UniquePtr<FormatUsageAuthority> ret(new FormatUsageAuthority);
    FormatUsageAuthority* const ptr = ret.get();

    const auto fnAddES3TexFormat = [ptr](GLenum sizedFormat, EffectiveFormat effFormat,
                                         bool isRenderable, bool isFilterable)
    {
        SetUsage(ptr, effFormat, isRenderable, isFilterable);
        auto usage = ptr->GetUsage(effFormat);
        ptr->AddSizedTexFormat(sizedFormat, usage);

        if (isRenderable) {
            ptr->AddRBFormat(sizedFormat, usage);
        }
    };

    ////////////////////////////////////////////////////////////////////////////

    // For renderable, see GLES 3.0.4, p212 "Framebuffer Completeness"
    // For filterable, see GLES 3.0.4, p161 "...a texture is complete unless..."

    // GLES 3.0.4, p128-129 "Required Texture Formats"
    // GLES 3.0.4, p130-132, table 3.13
    //                                             render filter
    //                                              able   able

#define FOO(x) LOCAL_GL_ ## x, EffectiveFormat::x

    fnAddES3TexFormat(FOO(R8         ), true , true );
    fnAddES3TexFormat(FOO(R8_SNORM   ), false, true );
    fnAddES3TexFormat(FOO(RG8        ), true , true );
    fnAddES3TexFormat(FOO(RG8_SNORM  ), false, true );
    fnAddES3TexFormat(FOO(RGB8       ), true , true );
    fnAddES3TexFormat(FOO(RGB8_SNORM ), false, true );
    fnAddES3TexFormat(FOO(RGB565     ), true , true );
    fnAddES3TexFormat(FOO(RGBA4      ), true , true );
    fnAddES3TexFormat(FOO(RGB5_A1    ), true , true );
    fnAddES3TexFormat(FOO(RGBA8      ), true , true );
    fnAddES3TexFormat(FOO(RGBA8_SNORM), false, true );
    fnAddES3TexFormat(FOO(RGB10_A2   ), true , true );
    fnAddES3TexFormat(FOO(RGB10_A2UI ), true , false);

    fnAddES3TexFormat(FOO(SRGB8       ), false, true);
    fnAddES3TexFormat(FOO(SRGB8_ALPHA8), true , true);

    fnAddES3TexFormat(FOO(R16F   ), false, true);
    fnAddES3TexFormat(FOO(RG16F  ), false, true);
    fnAddES3TexFormat(FOO(RGB16F ), false, true);
    fnAddES3TexFormat(FOO(RGBA16F), false, true);

    fnAddES3TexFormat(FOO(R32F   ), false, false);
    fnAddES3TexFormat(FOO(RG32F  ), false, false);
    fnAddES3TexFormat(FOO(RGB32F ), false, false);
    fnAddES3TexFormat(FOO(RGBA32F), false, false);

    fnAddES3TexFormat(FOO(R11F_G11F_B10F), false, true);
    fnAddES3TexFormat(FOO(RGB9_E5       ), false, true);

    fnAddES3TexFormat(FOO(R8I  ), true, false);
    fnAddES3TexFormat(FOO(R8UI ), true, false);
    fnAddES3TexFormat(FOO(R16I ), true, false);
    fnAddES3TexFormat(FOO(R16UI), true, false);
    fnAddES3TexFormat(FOO(R32I ), true, false);
    fnAddES3TexFormat(FOO(R32UI), true, false);

    fnAddES3TexFormat(FOO(RG8I  ), true, false);
    fnAddES3TexFormat(FOO(RG8UI ), true, false);
    fnAddES3TexFormat(FOO(RG16I ), true, false);
    fnAddES3TexFormat(FOO(RG16UI), true, false);
    fnAddES3TexFormat(FOO(RG32I ), true, false);
    fnAddES3TexFormat(FOO(RG32UI), true, false);

    fnAddES3TexFormat(FOO(RGB8I  ), false, false);
    fnAddES3TexFormat(FOO(RGB8UI ), false, false);
    fnAddES3TexFormat(FOO(RGB16I ), false, false);
    fnAddES3TexFormat(FOO(RGB16UI), false, false);
    fnAddES3TexFormat(FOO(RGB32I ), false, false);
    fnAddES3TexFormat(FOO(RGB32UI), false, false);

    fnAddES3TexFormat(FOO(RGBA8I  ), true, false);
    fnAddES3TexFormat(FOO(RGBA8UI ), true, false);
    fnAddES3TexFormat(FOO(RGBA16I ), true, false);
    fnAddES3TexFormat(FOO(RGBA16UI), true, false);
    fnAddES3TexFormat(FOO(RGBA32I ), true, false);
    fnAddES3TexFormat(FOO(RGBA32UI), true, false);

    // GLES 3.0.4, p133, table 3.14
    // GLES 3.0.4, p161 "...a texture is complete unless..."
    fnAddES3TexFormat(FOO(DEPTH_COMPONENT16 ), true, false);
    fnAddES3TexFormat(FOO(DEPTH_COMPONENT24 ), true, false);
    fnAddES3TexFormat(FOO(DEPTH_COMPONENT32F), true, false);
    fnAddES3TexFormat(FOO(DEPTH24_STENCIL8  ), true, false);
    fnAddES3TexFormat(FOO(DEPTH32F_STENCIL8 ), true, false);

    // GLES 3.0.4, p205-206, "Required Renderbuffer Formats"
    fnAddES3TexFormat(FOO(STENCIL_INDEX8), true, false);
/*
    // GLES 3.0.4, p128, table 3.12.
    // Unsized RGBA/RGB formats are renderable, other unsized are not.
    fnAddES3TexFormat(FOO(Luminance8Alpha8, false, true);
    fnAddES3TexFormat(FOO(Luminance8      , false, true);
    fnAddES3TexFormat(FOO(Alpha8          , false, true);
*/
    // GLES 3.0.4, p147, table 3.19
    // GLES 3.0.4, p286+, $C.1 "ETC Compressed Texture Image Formats"
    // (jgilbert) I can't find where these are established as filterable.
    fnAddES3TexFormat(FOO(COMPRESSED_RGB8_ETC2                     ), false, true);
    fnAddES3TexFormat(FOO(COMPRESSED_SRGB8_ETC2                    ), false, true);
    fnAddES3TexFormat(FOO(COMPRESSED_RGBA8_ETC2_EAC                ), false, true);
    fnAddES3TexFormat(FOO(COMPRESSED_SRGB8_ALPHA8_ETC2_EAC         ), false, true);
    fnAddES3TexFormat(FOO(COMPRESSED_R11_EAC                       ), false, true);
    fnAddES3TexFormat(FOO(COMPRESSED_RG11_EAC                      ), false, true);
    fnAddES3TexFormat(FOO(COMPRESSED_SIGNED_R11_EAC                ), false, true);
    fnAddES3TexFormat(FOO(COMPRESSED_SIGNED_RG11_EAC               ), false, true);
    fnAddES3TexFormat(FOO(COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2 ), false, true);
    fnAddES3TexFormat(FOO(COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2), false, true);

#undef FOO

    const auto fnAddSizedUnpack = [ptr](EffectiveFormat effFormat, GLenum internalFormat,
                                        GLenum unpackFormat, GLenum unpackType)
    {
        auto usage = ptr->EditUsage(effFormat);

        const PackingInfo pi = {unpackFormat, unpackType};
        const DriverUnpackInfo dui = {internalFormat, unpackFormat, unpackType};
        usage->AddUnpack(pi, dui);
    };

#define FOO(x) EffectiveFormat::x, LOCAL_GL_ ## x

    ////////////////////////////////////////////////////////////////////////////
    // GLES 3.0.4 p111-113
    // RGBA
    fnAddSizedUnpack(FOO(RGBA8       ), LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE              );
    fnAddSizedUnpack(FOO(RGBA4       ), LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_SHORT_4_4_4_4     );
    fnAddSizedUnpack(FOO(RGBA4       ), LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE              );
    fnAddSizedUnpack(FOO(RGB5_A1     ), LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_SHORT_5_5_5_1     );
    fnAddSizedUnpack(FOO(RGB5_A1     ), LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE              );
    fnAddSizedUnpack(FOO(RGB5_A1     ), LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_INT_2_10_10_10_REV);
    fnAddSizedUnpack(FOO(SRGB8_ALPHA8), LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE              );
    fnAddSizedUnpack(FOO(RGBA8_SNORM ), LOCAL_GL_RGBA, LOCAL_GL_BYTE                       );
    fnAddSizedUnpack(FOO(RGB10_A2    ), LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_INT_2_10_10_10_REV);
    fnAddSizedUnpack(FOO(RGBA16F     ), LOCAL_GL_RGBA, LOCAL_GL_HALF_FLOAT                 );
    fnAddSizedUnpack(FOO(RGBA16F     ), LOCAL_GL_RGBA, LOCAL_GL_FLOAT                      );
    fnAddSizedUnpack(FOO(RGBA32F     ), LOCAL_GL_RGBA, LOCAL_GL_FLOAT                      );

    // RGBA_INTEGER
    fnAddSizedUnpack(FOO(RGBA8UI   ), LOCAL_GL_RGBA_INTEGER, LOCAL_GL_UNSIGNED_BYTE              );
    fnAddSizedUnpack(FOO(RGBA8I    ), LOCAL_GL_RGBA_INTEGER, LOCAL_GL_BYTE                       );
    fnAddSizedUnpack(FOO(RGBA16UI  ), LOCAL_GL_RGBA_INTEGER, LOCAL_GL_UNSIGNED_SHORT             );
    fnAddSizedUnpack(FOO(RGBA16I   ), LOCAL_GL_RGBA_INTEGER, LOCAL_GL_SHORT                      );
    fnAddSizedUnpack(FOO(RGBA32UI  ), LOCAL_GL_RGBA_INTEGER, LOCAL_GL_UNSIGNED_INT               );
    fnAddSizedUnpack(FOO(RGBA32I   ), LOCAL_GL_RGBA_INTEGER, LOCAL_GL_INT                        );
    fnAddSizedUnpack(FOO(RGB10_A2UI), LOCAL_GL_RGBA_INTEGER, LOCAL_GL_UNSIGNED_INT_2_10_10_10_REV);

    // RGB
    fnAddSizedUnpack(FOO(RGB8          ), LOCAL_GL_RGB, LOCAL_GL_UNSIGNED_BYTE               );
    fnAddSizedUnpack(FOO(SRGB8         ), LOCAL_GL_RGB, LOCAL_GL_UNSIGNED_BYTE               );
    fnAddSizedUnpack(FOO(RGB565        ), LOCAL_GL_RGB, LOCAL_GL_UNSIGNED_SHORT_5_6_5        );
    fnAddSizedUnpack(FOO(RGB565        ), LOCAL_GL_RGB, LOCAL_GL_UNSIGNED_BYTE               );
    fnAddSizedUnpack(FOO(RGB8_SNORM    ), LOCAL_GL_RGB, LOCAL_GL_BYTE                        );
    fnAddSizedUnpack(FOO(R11F_G11F_B10F), LOCAL_GL_RGB, LOCAL_GL_UNSIGNED_INT_10F_11F_11F_REV);
    fnAddSizedUnpack(FOO(R11F_G11F_B10F), LOCAL_GL_RGB, LOCAL_GL_HALF_FLOAT                  );
    fnAddSizedUnpack(FOO(R11F_G11F_B10F), LOCAL_GL_RGB, LOCAL_GL_FLOAT                       );
    fnAddSizedUnpack(FOO(RGB16F        ), LOCAL_GL_RGB, LOCAL_GL_HALF_FLOAT                  );
    fnAddSizedUnpack(FOO(RGB16F        ), LOCAL_GL_RGB, LOCAL_GL_FLOAT                       );
    fnAddSizedUnpack(FOO(RGB9_E5       ), LOCAL_GL_RGB, LOCAL_GL_UNSIGNED_INT_5_9_9_9_REV    );
    fnAddSizedUnpack(FOO(RGB9_E5       ), LOCAL_GL_RGB, LOCAL_GL_HALF_FLOAT                  );
    fnAddSizedUnpack(FOO(RGB9_E5       ), LOCAL_GL_RGB, LOCAL_GL_FLOAT                       );
    fnAddSizedUnpack(FOO(RGB32F        ), LOCAL_GL_RGB, LOCAL_GL_FLOAT                       );

    // RGB_INTEGER
    fnAddSizedUnpack(FOO(RGB8UI ), LOCAL_GL_RGB_INTEGER, LOCAL_GL_UNSIGNED_BYTE );
    fnAddSizedUnpack(FOO(RGB8I  ), LOCAL_GL_RGB_INTEGER, LOCAL_GL_BYTE          );
    fnAddSizedUnpack(FOO(RGB16UI), LOCAL_GL_RGB_INTEGER, LOCAL_GL_UNSIGNED_SHORT);
    fnAddSizedUnpack(FOO(RGB16I ), LOCAL_GL_RGB_INTEGER, LOCAL_GL_SHORT         );
    fnAddSizedUnpack(FOO(RGB32UI), LOCAL_GL_RGB_INTEGER, LOCAL_GL_UNSIGNED_INT  );
    fnAddSizedUnpack(FOO(RGB32I ), LOCAL_GL_RGB_INTEGER, LOCAL_GL_INT           );

    // RG
    fnAddSizedUnpack(FOO(RG8      ), LOCAL_GL_RG, LOCAL_GL_UNSIGNED_BYTE);
    fnAddSizedUnpack(FOO(RG8_SNORM), LOCAL_GL_RG, LOCAL_GL_BYTE         );
    fnAddSizedUnpack(FOO(RG16F    ), LOCAL_GL_RG, LOCAL_GL_HALF_FLOAT   );
    fnAddSizedUnpack(FOO(RG16F    ), LOCAL_GL_RG, LOCAL_GL_FLOAT        );
    fnAddSizedUnpack(FOO(RG32F    ), LOCAL_GL_RG, LOCAL_GL_FLOAT        );

    // RG_INTEGER
    fnAddSizedUnpack(FOO(RG8UI ), LOCAL_GL_RG_INTEGER, LOCAL_GL_UNSIGNED_BYTE );
    fnAddSizedUnpack(FOO(RG8I  ), LOCAL_GL_RG_INTEGER, LOCAL_GL_BYTE          );
    fnAddSizedUnpack(FOO(RG16UI), LOCAL_GL_RG_INTEGER, LOCAL_GL_UNSIGNED_SHORT);
    fnAddSizedUnpack(FOO(RG16I ), LOCAL_GL_RG_INTEGER, LOCAL_GL_SHORT         );
    fnAddSizedUnpack(FOO(RG32UI), LOCAL_GL_RG_INTEGER, LOCAL_GL_UNSIGNED_INT  );
    fnAddSizedUnpack(FOO(RG32I ), LOCAL_GL_RG_INTEGER, LOCAL_GL_INT           );

    // RED
    fnAddSizedUnpack(FOO(R8      ), LOCAL_GL_RED, LOCAL_GL_UNSIGNED_BYTE);
    fnAddSizedUnpack(FOO(R8_SNORM), LOCAL_GL_RED, LOCAL_GL_BYTE         );
    fnAddSizedUnpack(FOO(R16F    ), LOCAL_GL_RED, LOCAL_GL_HALF_FLOAT   );
    fnAddSizedUnpack(FOO(R16F    ), LOCAL_GL_RED, LOCAL_GL_FLOAT        );
    fnAddSizedUnpack(FOO(R32F    ), LOCAL_GL_RED, LOCAL_GL_FLOAT        );

    // RED_INTEGER
    fnAddSizedUnpack(FOO(R8UI ), LOCAL_GL_RED_INTEGER, LOCAL_GL_UNSIGNED_BYTE );
    fnAddSizedUnpack(FOO(R8I  ), LOCAL_GL_RED_INTEGER, LOCAL_GL_BYTE          );
    fnAddSizedUnpack(FOO(R16UI), LOCAL_GL_RED_INTEGER, LOCAL_GL_UNSIGNED_SHORT);
    fnAddSizedUnpack(FOO(R16I ), LOCAL_GL_RED_INTEGER, LOCAL_GL_SHORT         );
    fnAddSizedUnpack(FOO(R32UI), LOCAL_GL_RED_INTEGER, LOCAL_GL_UNSIGNED_INT  );
    fnAddSizedUnpack(FOO(R32I ), LOCAL_GL_RED_INTEGER, LOCAL_GL_INT           );

    // DEPTH_COMPONENT
    fnAddSizedUnpack(FOO(DEPTH_COMPONENT16 ), LOCAL_GL_DEPTH_COMPONENT, LOCAL_GL_UNSIGNED_SHORT);
    fnAddSizedUnpack(FOO(DEPTH_COMPONENT16 ), LOCAL_GL_DEPTH_COMPONENT, LOCAL_GL_UNSIGNED_INT  );
    fnAddSizedUnpack(FOO(DEPTH_COMPONENT24 ), LOCAL_GL_DEPTH_COMPONENT, LOCAL_GL_UNSIGNED_INT  );
    fnAddSizedUnpack(FOO(DEPTH_COMPONENT32F), LOCAL_GL_DEPTH_COMPONENT, LOCAL_GL_FLOAT         );

    // DEPTH_STENCIL
    fnAddSizedUnpack(FOO(DEPTH24_STENCIL8 ), LOCAL_GL_DEPTH_STENCIL, LOCAL_GL_UNSIGNED_INT_24_8             );
    fnAddSizedUnpack(FOO(DEPTH32F_STENCIL8), LOCAL_GL_DEPTH_STENCIL, LOCAL_GL_FLOAT_32_UNSIGNED_INT_24_8_REV);

#undef FOO

    AddBasicUnsizedFormats(ptr, gl);

    return Move(ret);
}

//////////////////////////////////////////////////////////////////////////////////////////

void
FormatUsageAuthority::AddRBFormat(GLenum sizedFormat, const FormatUsageInfo* usage)
{
    AlwaysInsert(mRBFormatMap, sizedFormat, usage);
}

void
FormatUsageAuthority::AddSizedTexFormat(GLenum sizedFormat, const FormatUsageInfo* usage)
{
    MOZ_ASSERT(usage->isRenderable);
    AlwaysInsert(mSizedTexFormatMap, sizedFormat, usage);
}

void
FormatUsageAuthority::AddUnsizedTexFormat(const PackingInfo& pi,
                                          const FormatUsageInfo* usage)
{
    AlwaysInsert(mUnsizedTexFormatMap, pi, usage);
}

const FormatUsageInfo*
FormatUsageAuthority::GetRBUsage(GLenum sizedFormat) const
{
    return FindOrNull(mRBFormatMap, sizedFormat);
}

const FormatUsageInfo*
FormatUsageAuthority::GetSizedTexUsage(GLenum sizedFormat) const
{
    return FindOrNull(mSizedTexFormatMap, sizedFormat);
}

const FormatUsageInfo*
FormatUsageAuthority::GetUnsizedTexUsage(const PackingInfo& pi) const
{
    return FindOrNull(mUnsizedTexFormatMap, pi);
}

FormatUsageInfo*
FormatUsageAuthority::EditUsage(EffectiveFormat format)
{
    auto itr = mUsageMap.find(format);

    if (itr == mUsageMap.end()) {
        const FormatInfo* formatInfo = GetFormat(format);
        MOZ_RELEASE_ASSERT(formatInfo);

        FormatUsageInfo usage(formatInfo);
        itr = AlwaysInsert(mUsageMap, format, usage);
    }

    return &(itr->second);
}

const FormatUsageInfo*
FormatUsageAuthority::GetUsage(EffectiveFormat format) const
{
    MOZ_ASSERT(!mUsageMap.empty());
    auto itr = mUsageMap.find(format);
    if (itr == mUsageMap.end())
        return nullptr;

    return &(itr->second);
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
