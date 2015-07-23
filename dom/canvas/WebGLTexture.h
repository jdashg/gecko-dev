/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_TEXTURE_H_
#define WEBGL_TEXTURE_H_

#include <algorithm>

#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/LinkedList.h"
#include "nsWrapperCache.h"

#include "WebGLFramebufferAttachable.h"
#include "WebGLObjectModel.h"
#include "WebGLStrongTypes.h"

namespace mozilla {

// Zero is not an integer power of two.
inline bool
IsPOTAssumingNonnegative(GLsizei x)
{
    MOZ_ASSERT(x >= 0);
    return x && (x & (x-1)) == 0;
}

// NOTE: When this class is switched to new DOM bindings, update the (then-slow)
// WrapObject calls in GetParameter and GetFramebufferAttachmentParameter.
class WebGLTexture final
    : public nsWrapperCache
    , public WebGLRefCountedObject<WebGLTexture>
    , public LinkedListElement<WebGLTexture>
    , public WebGLContextBoundObject
    , public WebGLFramebufferAttachable
{
    friend class WebGLContext;
    friend class WebGLFramebuffer;

public:
    const GLuint mGLName;

protected:
    GLenum mTarget;
    uint8_t mFaceCount; // 6 for cube maps, 1 otherwise.
    static const uint8_t kMaxFaceCount = 6;

    TexMinFilter mMinFilter;
    TexMagFilter mMagFilter;
    TexWrap mWrapS, mWrapT;

    size_t mImmutableLevelCount;

public:
    class ImageInfo;
protected:
    // We need to use a map because this needs to be sparse. `INT32_MAX - 100` is a valid
    // mBaseMipmapLevel.

    struct Face;
    std::map<size_t, Face> mImageInfoMap;

    bool mImmutable; // Set by texStorage*

    size_t mBaseMipmapLevel; // Set by texParameter (defaults to 0)
    size_t mMaxMipmapLevel;  // Set by texParameter (defaults to 1000)

    WebGLTextureFakeBlackStatus mFakeBlackStatus;

public:
    explicit WebGLTexture(WebGLContext* webgl, GLuint tex);

    void Delete();

    bool HasEverBeenBound() const { return mTarget != LOCAL_GL_NONE; }

    WebGLContext* GetParentObject() const {
        return Context();
    }

    virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto) override;

    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLTexture)
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLTexture)

protected:
    ~WebGLTexture() {
        DeleteOnce();
    }

public:
    // GL calls
    bool BindTexture(TexTarget texTarget);
    void GenerateMipmap(TexTarget texTarget);
    JS::Value GetTexParameter(TexTarget texTarget, GLenum pname);
    bool IsTexture() const;
    void TexParameter(TexTarget texTarget, GLenum pname, GLint* maybeIntParam,
                      GLfloat* maybeFloatParam);

public:
    class ImageInfo
        : public WebGLRectangleObject
    {
        friend class WebGLTexture;

        // This is the "effective internal format" of the texture, an official
        // OpenGL spec concept, see OpenGL ES 3.0.3 spec, section 3.8.3, page
        // 126 and below.
        TexInternalFormat mEffectiveInternalFormat;

        /* Used only for 3D textures.
         * Note that mWidth and mHeight are inherited from WebGLRectangleObject.
         * It's a pity to store a useless mDepth on non-3D texture images, but
         * the size of GLsizei is negligible compared to the typical size of a texture image.
         */
        GLsizei mDepth;

        WebGLImageDataStatus mImageDataStatus;

    public:
        ImageInfo()
            : mEffectiveInternalFormat(LOCAL_GL_NONE)
            , mDepth(0)
            , mImageDataStatus(WebGLImageDataStatus::NoImageData)
        { }

        ImageInfo(GLsizei width, GLsizei height, GLsizei depth,
                  TexInternalFormat effectiveInternalFormat,
                  WebGLImageDataStatus status)
            : WebGLRectangleObject(width, height)
            , mEffectiveInternalFormat(effectiveInternalFormat)
            , mDepth(depth)
            , mImageDataStatus(status)
        {
            // shouldn't use this constructor to construct a null ImageInfo
            MOZ_ASSERT(status != WebGLImageDataStatus::NoImageData);
        }

        /*
        bool operator==(const ImageInfo& a) const {
            return mImageDataStatus == a.mImageDataStatus &&
                   mWidth == a.mWidth &&
                   mHeight == a.mHeight &&
                   mDepth == a.mDepth &&
                   mEffectiveInternalFormat == a.mEffectiveInternalFormat;
        }

        bool operator!=(const ImageInfo& a) const {
            return !(*this == a);
        }
        */

        bool IsPowerOfTwo() const {
            MOZ_ASSERT(mWidth >= 0);
            MOZ_ASSERT(mHeight >= 0);
            MOZ_ASSERT(mDepth >= 0);
            return IsPOTAssumingNonnegative(mWidth) &&
                   IsPOTAssumingNonnegative(mHeight) &&
                   IsPOTAssumingNonnegative(mDepth);
        }

        TexInternalFormat EffectiveInternalFormat() const {
            return mEffectiveInternalFormat;
        }

        GLsizei Depth() const { return mDepth; }

        bool HasUninitializedImageData() const {
            MOZ_ASSERT(mImageDataStatus != WebGLImageDataStatus::NoImageData);
            return mImageDataStatus == WebGLImageDataStatus::UninitializedImageData;
        }

        size_t MemoryUsage() const;


        bool IsDefined() const {
            if (!mEffectiveInternalFormat.get()) {
                MOZ_ASSERT(mImageDataStatus == WebGLImageDataStatus::NoImageData);
                MOZ_ASSERT(!mWidth && !mHeight && !mDepth);
                return false;
            }

            MOZ_ASSERT(mImageDataStatus != WebGLImageDataStatus::NoImageData);
            MOZ_ASSERT(mWidth && mHeight && mDepth);
            return true;
        }
    };

protected:
    struct Face {
        ImageInfo faces[kMaxFaceCount];
    };

    static uint8_t FaceForTarget(TexImageTarget texImageTarget) {
        GLenum rawTexImageTarget = texImageTarget.get();
        switch (rawTexImageTarget) {
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
            return rawTexImageTarget - LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X;

        default:
            return 0;
        }
    }

    static const ImageInfo kUndefinedImageInfo;

    const ImageInfo& ImageInfoAtFace(uint8_t face, size_t level) const {
        MOZ_ASSERT(face < mFaceCount);

        auto itr = mImageInfoMap.find(level);
        if (itr == mImageInfoMap.end())
            return kUndefinedImageInfo;

        return itr->second.faces[face];
    }

    static const Face kNewLevelFace;

    void SetImageInfosAtLevel(size_t level, const ImageInfo& val) {
        auto res = mImageInfoMap.insert(std::make_pair(level, kNewLevelFace));
        auto& itr = res.first;

        for (size_t i = 0; i < mFaceCount; i++) {
            itr->second.faces[i] = val;
        }
    }

    void SetImageInfoAtFace(uint8_t face, size_t level, const ImageInfo& val) {
        MOZ_ASSERT(face < mFaceCount);

        auto res = mImageInfoMap.insert(std::make_pair(level, kNewLevelFace));
        auto& itr = res.first;
        itr->second.faces[face] = val;
    }

    template<typename T>
    T Clamp(T val, T min, T max) {
        return std::max(min, std::min(val, max));
    }

    void ClampLevelBaseAndMax() {
        if (!mImmutable)
            return;

        // GLES 3.0.4, p158:
        // "For immutable-format textures, `level_base` is clamped to the range
        //  `[0, levels-1]`, `level_max` is then clamped to the range `
        //  `[level_base, levels-1]`, where `levels` is the parameter passed to
        //   TexStorage* for the texture object."
        mBaseMipmapLevel = Clamp(mBaseMipmapLevel, size_t(0), mImmutableLevelCount - 1);
        mMaxMipmapLevel = Clamp(mMaxMipmapLevel, mBaseMipmapLevel,
                                mImmutableLevelCount - 1);
    }

public:
    const ImageInfo& ImageInfoAt(TexImageTarget texImageTarget, GLint level) const {
        uint8_t face = FaceForTarget(texImageTarget);
        return ImageInfoAtFace(face, level);
    }

    void SetImageInfoAt(TexImageTarget texImageTarget, GLint level,
                        const ImageInfo& val)
    {
        uint8_t face = FaceForTarget(texImageTarget);
        return SetImageInfoAtFace(face, level, val);
    }

    size_t MemoryUsage() const;
/*
    void SetImageDataStatus(TexImageTarget imageTarget, GLint level,
                            WebGLImageDataStatus newStatus)
    {
        MOZ_ASSERT(HasImageInfoAt(imageTarget, level));
        ImageInfo& imageInfo = ImageInfoAt(imageTarget, level);
        // There is no way to go from having image data to not having any.
        MOZ_ASSERT(newStatus != WebGLImageDataStatus::NoImageData ||
                   imageInfo.mImageDataStatus == WebGLImageDataStatus::NoImageData);

        if (imageInfo.mImageDataStatus != newStatus)
            SetFakeBlackStatus(WebGLTextureFakeBlackStatus::Unknown);

        imageInfo.mImageDataStatus = newStatus;
    }

    bool EnsureInitializedImageData(TexImageTarget imageTarget, GLint level);
*/
protected:
/*
    void EnsureMaxLevelWithCustomImagesAtLeast(size_t maxLevelWithCustomImages) {
        mMaxLevelWithCustomImages = std::max(mMaxLevelWithCustomImages,
                                             maxLevelWithCustomImages);
        mImageInfos.EnsureLengthAtLeast((mMaxLevelWithCustomImages + 1) * mFacesCount);
    }
*/
    bool CheckFloatTextureFilterParams() const {
        // Without OES_texture_float_linear, only NEAREST and
        // NEAREST_MIMPAMP_NEAREST are supported.
        return mMagFilter == LOCAL_GL_NEAREST &&
               (mMinFilter == LOCAL_GL_NEAREST ||
                mMinFilter == LOCAL_GL_NEAREST_MIPMAP_NEAREST);
    }

    bool AreBothWrapModesClampToEdge() const {
        return mWrapS == LOCAL_GL_CLAMP_TO_EDGE &&
               mWrapT == LOCAL_GL_CLAMP_TO_EDGE;
    }

    bool DoesMipmapHaveAllLevelsConsistentlyDefined(TexImageTarget texImageTarget) const;

public:
    void Bind(TexTarget texTarget);

    void SetImageInfo(TexImageTarget target, GLint level, GLsizei width,
                      GLsizei height, GLsizei depth, TexInternalFormat format,
                      WebGLImageDataStatus status);

    bool DoesMinFilterRequireMipmap() const {
        return !(mMinFilter == LOCAL_GL_NEAREST ||
                 mMinFilter == LOCAL_GL_LINEAR);
    }

    bool IsMipmapComplete() const;

    bool IsCubeComplete() const;

    void SetFakeBlackStatus(WebGLTextureFakeBlackStatus x);

    bool IsImmutable() const { return mImmutable; }
    void SetImmutable() { mImmutable = true; }

    // Returns the current fake-black-status, except if it was Unknown,
    // in which case this function resolves it first, so it never returns Unknown.
    WebGLTextureFakeBlackStatus ResolvedFakeBlackStatus();

    TexTarget Target() const { return mTarget; }
    bool IsCubeMap() const { return mFaceCount != 1; }
};

inline TexImageTarget
TexImageTargetForTargetAndFace(TexTarget target, size_t face)
{
    switch (target.get()) {
    case LOCAL_GL_TEXTURE_2D:
    case LOCAL_GL_TEXTURE_3D:
        MOZ_ASSERT(face == 0);
        return target.get();
    case LOCAL_GL_TEXTURE_CUBE_MAP:
        MOZ_ASSERT(face < 6);
        return LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;
    default:
        MOZ_CRASH();
    }
}

} // namespace mozilla

#endif // WEBGL_TEXTURE_H_
