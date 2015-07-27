/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_TEXTURE_H_
#define WEBGL_TEXTURE_H_

#include <algorithm>
#include <map>

#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/LinkedList.h"
#include "nsWrapperCache.h"

#include "WebGLFramebufferAttachable.h"
#include "WebGLObjectModel.h"
#include "WebGLStrongTypes.h"

namespace mozilla {
class ErrorResult;

namespace dom {
class Element;
class ImageData;
class ArrayBufferViewOrSharedArrayBufferView;
} // namespace dom

// Zero is not an integer power of two.
/*
*/

inline bool
IsPowerOfTwo(uint32_t x)
{
    return x && (x & (x-1)) == 0;
}

bool
DoesTargetMatchDimensions(WebGLContext* webgl, TexImageTarget target, uint8_t dims,
                          const char* funcName);

// NOTE: When this class is switched to new DOM bindings, update the (then-slow)
// WrapObject calls in GetParameter and GetFramebufferAttachmentParameter.
class WebGLTexture final
    : public nsWrapperCache
    , public WebGLRefCountedObject<WebGLTexture>
    , public LinkedListElement<WebGLTexture>
    , public WebGLContextBoundObject
    , public WebGLFramebufferAttachable
{
    // Friends
    friend class WebGLContext;
    friend class WebGLFramebuffer;

    // Forwards
protected:
    struct Face;
public:
    class ImageInfo;

    ////////////////////////////////////
    // Members

    const GLuint mGLName;

protected:
    TexTarget mTarget;
    uint8_t mFaceCount; // 6 for cube maps, 1 otherwise.
    static const uint8_t kMaxFaceCount = 6;

    std::map<size_t, Face> mImageInfoMap;

    TexMinFilter mMinFilter;
    TexMagFilter mMagFilter;
    TexWrap mWrapS, mWrapT;

    bool mImmutable; // Set by texStorage*
    size_t mImmutableLevelCount;

    size_t mBaseMipmapLevel; // Set by texParameter (defaults to 0)
    size_t mMaxMipmapLevel;  // Set by texParameter (defaults to 1000)

    WebGLTextureFakeBlackStatus mFakeBlackStatus;

    GLenum mTexCompareMode;

    ////////////////////////////////////
public:
    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLTexture)
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLTexture)

    explicit WebGLTexture(WebGLContext* webgl, GLuint tex);

    void Delete();

    bool HasEverBeenBound() const { return mTarget != LOCAL_GL_NONE; }
    TexTarget Target() const { return mTarget; }

    WebGLContext* GetParentObject() const {
        return Context();
    }

    virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto) override;

protected:
    ~WebGLTexture() {
        DeleteOnce();
    }
public:
    ////////////////////////////////////
    // GL calls
    bool BindTexture(TexTarget texTarget);
    void GenerateMipmap(TexTarget texTarget);
    JS::Value GetTexParameter(TexTarget texTarget, GLenum pname);
    bool IsTexture() const;
    void TexParameter(TexTarget texTarget, GLenum pname, GLint* maybeIntParam,
                      GLfloat* maybeFloatParam);

    ////////////////////////////////////
    // WebGLTextureUpload.cpp

    void CompressedTexImage2D(TexImageTarget texImageTarget, GLint level,
                              GLenum internalFormat, GLsizei width, GLsizei height,
                              GLint border, const dom::ArrayBufferViewOrSharedArrayBufferView& view);

    void CompressedTexImage3D(TexImageTarget texImageTarget, GLint level,
                              GLenum internalFormat, GLsizei width, GLsizei height,
                              GLsizei depth, GLint border, GLsizei imageSize,
                              const dom::ArrayBufferViewOrSharedArrayBufferView& view);


    void CompressedTexSubImage2D(TexImageTarget texImageTarget, GLint level,
                                 GLint xOffset, GLint yOffset, GLsizei width,
                                 GLsizei height, GLenum unpackFormat,
                                 const dom::ArrayBufferViewOrSharedArrayBufferView& view);

    void CompressedTexSubImage3D(TexImageTarget texImageTarget, GLint level,
                                 GLint xOffset, GLint yOffset, GLint zOffset,
                                 GLsizei width, GLsizei height, GLsizei depth,
                                 GLenum unpackFormat, GLsizei imageSize,
                                 const dom::ArrayBufferViewOrSharedArrayBufferView& view);


    void CopyTexImage2D(TexImageTarget texImageTarget, GLint level, GLenum internalFormat,
                        GLint x, GLint y, GLsizei width, GLsizei height, GLint border);


    void CopyTexSubImage2D(TexImageTarget texImageTarget, GLint level, GLint xOffset,
                           GLint yOffset, GLint x, GLint y, GLsizei width,
                           GLsizei height);

    void CopyTexSubImage3D(TexImageTarget texImageTarget, GLint level, GLint xOffset,
                           GLint yOffset, GLint zOffset, GLint x, GLint y, GLsizei width,
                           GLsizei height);


    void TexImage2D(TexImageTarget texImageTarget, GLint level, GLenum internalFormat,
                    GLsizei width, GLsizei height, GLint border, GLenum unpackFormat,
                    GLenum unpackType,
                    const dom::Nullable<dom::ArrayBufferViewOrSharedArrayBufferView>& maybeView,
                    ErrorResult* const out_rv);
    void TexImage2D(TexImageTarget texImageTarget, GLint level, GLenum internalFormat,
                    GLenum unpackFormat, GLenum unpackType, dom::ImageData* imageData,
                    ErrorResult* const out_rv);
    void TexImage2D(TexImageTarget texImageTarget, GLint level, GLenum internalFormat,
                    GLenum unpackFormat, GLenum unpackType, dom::Element* elem,
                    ErrorResult* const out_rv);

    void TexImage3D(TexImageTarget target, GLint level, GLenum internalFormat,
                    GLsizei width, GLsizei height, GLsizei depth, GLint border,
                    GLenum unpackFormat, GLenum unpackType,
                    const dom::Nullable<dom::ArrayBufferViewOrSharedArrayBufferView>& maybeView,
                    ErrorResult* const out_rv);


    void TexStorage2D(TexTarget texTarget, GLsizei levels, GLenum internalFormat,
                      GLsizei width, GLsizei height);
    void TexStorage3D(TexTarget texTarget, GLsizei levels, GLenum internalFormat,
                      GLsizei width, GLsizei height, GLsizei depth);


    void TexSubImage2D(TexImageTarget texImageTarget, GLint level, GLint xOffset,
                       GLint yOffset, GLsizei width, GLsizei height, GLenum unpackFormat,
                       GLenum unpackType,
                       const dom::Nullable<dom::ArrayBufferViewOrSharedArrayBufferView>& maybeView,
                       ErrorResult* const out_rv);
    void TexSubImage2D(TexImageTarget texImageTarget, GLint level, GLint xOffset,
                       GLint yOffset, GLenum unpackFormat, GLenum unpackType,
                       dom::ImageData* imageData, ErrorResult* const out_rv);
    void TexSubImage2D(TexImageTarget texImageTarget, GLint level, GLint xOffset,
                       GLint yOffset, GLenum unpackFormat, GLenum unpackType,
                       dom::Element* elem, ErrorResult* const out_rv);

    void TexSubImage3D(TexImageTarget texImageTarget, GLint level, GLint xOffset,
                       GLint yOffset, GLint zOffset, GLsizei width, GLsizei height,
                       GLsizei depth, GLenum unpackFormat, GLenum unpackType,
                       const dom::Nullable<dom::ArrayBufferViewOrSharedArrayBufferView>& maybeView,
                       ErrorResult* const out_rv);
    void TexSubImage3D(TexImageTarget texImageTarget, GLint level, GLint xOffset,
                       GLint yOffset, GLint zOffset, GLenum unpackFormat,
                       GLenum unpackType, dom::ImageData* imageData,
                       ErrorResult* const out_rv);
    void TexSubImage3D(TexImageTarget texImageTarget, GLint level, GLint xOffset,
                       GLint yOffset, GLint zOffset, GLenum unpackFormat,
                       GLenum unpackType, dom::Element* elem, ErrorResult* const out_rv);

protected:

    /** Like glTexImage2D, but if the call may change the texture size, checks
     * any GL error generated by this glTexImage2D call and returns it.
     */
    GLenum CheckedTexImage2D(TexImageTarget texImageTarget, GLint level,
                             TexInternalFormat internalFormat, GLsizei width,
                             GLsizei height, GLint border, TexFormat format,
                             TexType type, const GLvoid* data);

    bool ValidateTexStorage(TexImageTarget texImageTarget, GLsizei levels, GLenum internalFormat,
                            GLsizei width, GLsizei height, GLsizei depth,
                            const char* funcName);
    void SpecifyTexStorage(GLsizei levels, TexInternalFormat internalFormat,
                           GLsizei width, GLsizei height, GLsizei depth);

    void CopyTexSubImage2D_base(TexImageTarget texImageTarget,
                                GLint level, TexInternalFormat internalFormat,
                                GLint xoffset, GLint yoffset, GLint x, GLint y,
                                GLsizei width, GLsizei height, bool isSub);

    bool TexImageFromVideoElement(TexImageTarget texImageTarget, GLint level,
                                  GLenum internalFormat, GLenum unpackFormat,
                                  GLenum unpackType, dom::Element* elem);

    // If jsArrayType is MaxTypedArrayViewType, it means no array.
    void TexImage2D_base(TexImageTarget texImageTarget, GLint level,
                         GLenum internalFormat, GLsizei width, GLsizei height,
                         GLsizei srcStrideOrZero, GLint border, GLenum unpackFormat,
                         GLenum unpackType, void* data, uint32_t byteLength,
                         js::Scalar::Type jsArrayType, WebGLTexelFormat srcFormat,
                         bool srcPremultiplied);
    void TexSubImage2D_base(TexImageTarget texImageTarget, GLint level, GLint xOffset,
                            GLint yOffset, GLsizei width, GLsizei height,
                            GLsizei srcStrideOrZero, GLenum unpackFormat,
                            GLenum unpackType, void* pixels, uint32_t byteLength,
                            js::Scalar::Type jsArrayType, WebGLTexelFormat srcFormat,
                            bool srcPremultiplied);

    bool ValidateTexStorage(TexTarget texTarget, GLsizei levels, GLenum internalFormat,
                                      GLsizei width, GLsizei height, GLsizei depth,
                                      const char* info);
    bool ValidateSizedInternalFormat(GLenum internalFormat, const char* info);

    void ClampLevelBaseAndMax();

    void PopulateMipChain(size_t baseLevel, size_t maxLevel);

    ////////////////////////////////////

public:
    // We store information about the various images that are part of this
    // texture. (cubemap faces, mipmap levels)
    class ImageInfo
    {
        friend class WebGLTexture;

    public:
        // This is the "effective internal format" of the texture, an official
        // OpenGL spec concept, see OpenGL ES 3.0.3 spec, section 3.8.3, page
        // 126 and below.
        const TexInternalFormat mFormat;

        const uint32_t mWidth;
        const uint32_t mHeight;
        const uint32_t mDepth;

        bool mHasUninitData;

        ImageInfo()
            : mFormat(LOCAL_GL_NONE)
            , mWidth(0)
            , mHeight(0)
            , mDepth(0)
            , mHasUninitData(true)
        { }

        ImageInfo(TexInternalFormat format, uint32_t width, uint32_t height,
                  uint32_t depth, bool hasUninitData)
            : mFormat(format)
            , mWidth(width)
            , mHeight(height)
            , mDepth(depth)
            , mHasUninitData(hasUninitData)
        {
            MOZ_ASSERT(mWidth && mHeight && mDepth);
            MOZ_ASSERT(mFormat != LOCAL_GL_NONE);
        }

    protected:
        ImageInfo& operator =(const ImageInfo& a);

        /*
        bool operator==(const ImageInfo& a) const {
            return mStatus == a.mStatus &&
                   mWidth == a.mWidth &&
                   mHeight == a.mHeight &&
                   mDepth == a.mDepth &&
                   mFormat == a.mFormat;
        }
        bool operator!=(const ImageInfo& a) const {
            return !(*this == a);
        }
        */

    public:
        size_t MaxMipmapLevels() const {
            // GLES 3.0.4, 3.8 - Mipmapping: `floor(log2(largest_of_dims)) + 1`
            uint32_t largest = std::max(std::max(mWidth, mHeight), mDepth);
            return FloorLog2Size(largest) + 1;
        }

        bool IsPowerOfTwo() const {
            return mozilla::IsPowerOfTwo(mWidth) &&
                   mozilla::IsPowerOfTwo(mHeight) &&
                   mozilla::IsPowerOfTwo(mDepth);
        }

        size_t MemoryUsage() const;

        bool IsDefined() const {
            if (mFormat == LOCAL_GL_NONE) {
                MOZ_ASSERT(!mWidth && !mHeight && !mDepth);
                return false;
            }

            MOZ_ASSERT(mWidth && mHeight && mDepth);
            return true;
        }
    };

protected:
    struct Face {
        ImageInfo faces[kMaxFaceCount];
    };

    size_t MaxEffectiveMipmapLevel() const;


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

    static const Face kNewLevelFace;
    /*
    // Every time we query one, create it if it doesn't already exist.
    ImageInfo& ImageInfoAtFace(uint8_t face, size_t level) {
        MOZ_ASSERT(face < mFaceCount);

        auto res = mImageInfoMap.insert(std::make_pair(level, kNewLevelFace));
        auto& itr = res.first;

        return itr->second.faces[face];
    }
    */

    // This is always undefined. It's not const, since modifying mHasUninitData doesn't
    // matter for IsDefined().
    static ImageInfo sUndefinedImageInfo;

    ImageInfo& ImageInfoAtFace(uint8_t face, size_t level) {
        MOZ_ASSERT(face < mFaceCount);

        auto itr = mImageInfoMap.find(level);
        if (itr == mImageInfoMap.end())
            return sUndefinedImageInfo;

        return itr->second.faces[face];
    }

    const ImageInfo& ImageInfoAtFace(uint8_t face, size_t level) const {
        return const_cast<WebGLTexture*>(this)->ImageInfoAtFace(face, level);
    }

    void SetImageInfoAtFace(uint8_t face, size_t level, const ImageInfo& val) {
        MOZ_ASSERT(face < mFaceCount);

        auto res = mImageInfoMap.insert(std::make_pair(level, kNewLevelFace));
        auto& itr = res.first;
        itr->second.faces[face] = val;
    }

    void SetImageInfosAtLevel(size_t level, const ImageInfo& val) {
        auto res = mImageInfoMap.insert(std::make_pair(level, kNewLevelFace));
        auto& itr = res.first;

        for (size_t i = 0; i < mFaceCount; i++) {
            itr->second.faces[i] = val;
        }
    }

public:
    ImageInfo& ImageInfoAt(TexImageTarget texImageTarget, GLint level) {
        uint8_t face = FaceForTarget(texImageTarget);
        return ImageInfoAtFace(face, level);
    }

    const ImageInfo& ImageInfoAt(TexImageTarget texImageTarget, GLint level) const {
        return const_cast<WebGLTexture*>(this)->ImageInfoAt(texImageTarget, level);
    }

    void SetImageInfoAt(TexImageTarget texImageTarget, GLint level,
                        const ImageInfo& val)
    {
        uint8_t face = FaceForTarget(texImageTarget);
        return SetImageInfoAtFace(face, level, val);
    }

    const ImageInfo& BaseImageInfo() const {
        return ImageInfoAtFace(0, mBaseMipmapLevel);
    }
/*
    void SetImageDataStatus(TexImageTarget texImageTarget, size_t level,
                            WebGLImageDataStatus status)
    {
    }
*/
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
*/

protected:
    bool EnsureInitializedImageData(uint8_t face, size_t level);

    bool EnsureInitializedImageData(TexImageTarget texImageTarget, size_t level) {
        const uint8_t face = FaceForTarget(texImageTarget);
        return EnsureInitializedImageData(face, level);
    }
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
/*
    bool DoesMipmapHaveAllLevelsConsistentlyDefined(TexImageTarget texImageTarget) const;
*/
public:
    void Bind(TexTarget texTarget);

    void SetImageInfo(TexImageTarget target, GLint level, GLsizei width,
                      GLsizei height, GLsizei depth, TexInternalFormat format,
                      WebGLImageDataStatus status);

    bool DoesMinFilterRequireMipmap() const {
        return !(mMinFilter == LOCAL_GL_NEAREST ||
                 mMinFilter == LOCAL_GL_LINEAR);
    }

    void SetGeneratedMipmap();

    void SetCustomMipmap();

    bool AreAllLevel0ImageInfosEqual() const;

    bool IsMipmapComplete() const;

    bool IsCubeComplete() const;

    bool IsComplete(const char** const out_reason) const;

    bool IsMipmapCubeComplete() const;

    void SetFakeBlackStatus(WebGLTextureFakeBlackStatus x);

    bool IsCubeMap() const { return (mTarget == LOCAL_GL_TEXTURE_CUBE_MAP); }

protected:
    bool ResolveFakeBlackStatus();
public:
    bool ResolveFakeBlackStatus(WebGLTextureFakeBlackStatus* const out);
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
