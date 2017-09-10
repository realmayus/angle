//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// StateManager11.h: Defines a class for caching D3D11 state

#ifndef LIBANGLE_RENDERER_D3D11_STATEMANAGER11_H_
#define LIBANGLE_RENDERER_D3D11_STATEMANAGER11_H_

#include <array>

#include "libANGLE/ContextState.h"
#include "libANGLE/State.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/d3d/IndexDataManager.h"
#include "libANGLE/renderer/d3d/RendererD3D.h"
#include "libANGLE/renderer/d3d/d3d11/InputLayoutCache.h"
#include "libANGLE/renderer/d3d/d3d11/Query11.h"
#include "libANGLE/renderer/d3d/d3d11/RenderStateCache.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"

namespace rx
{

struct RenderTargetDesc;
struct Renderer11DeviceCaps;

class ShaderConstants11 : angle::NonCopyable
{
  public:
    ShaderConstants11();

    void init(const gl::Caps &caps);
    size_t getRequiredBufferSize(gl::SamplerType samplerType) const;
    void markDirty();

    void setComputeWorkGroups(GLuint numGroupsX, GLuint numGroupsY, GLuint numGroupsZ);
    void setMultiviewWriteToViewportIndex(GLfloat index);
    void onViewportChange(const gl::Rectangle &glViewport,
                          const D3D11_VIEWPORT &dxViewport,
                          bool is9_3,
                          bool presentPathFast);
    void onSamplerChange(gl::SamplerType samplerType,
                         unsigned int samplerIndex,
                         const gl::Texture &texture);

    gl::Error updateBuffer(ID3D11DeviceContext *deviceContext,
                           gl::SamplerType samplerType,
                           const ProgramD3D &programD3D,
                           const d3d11::Buffer &driverConstantBuffer);

  private:
    struct Vertex
    {
        Vertex()
            : depthRange{.0f},
              viewAdjust{.0f},
              viewCoords{.0f},
              viewScale{.0f},
              multiviewWriteToViewportIndex{.0f},
              padding{.0f}
        {
        }

        float depthRange[4];
        float viewAdjust[4];
        float viewCoords[4];
        float viewScale[2];
        // multiviewWriteToViewportIndex is used to select either the side-by-side or layered
        // code-path in the GS. It's value, if set, is either 0.0f or 1.0f. The value is updated
        // whenever a multi-view draw framebuffer is made active.
        float multiviewWriteToViewportIndex;

        // Added here to manually pad the struct.
        float padding;
    };

    struct Pixel
    {
        Pixel()
            : depthRange{.0f},
              viewCoords{.0f},
              depthFront{.0f},
              viewScale{.0f},
              multiviewWriteToViewportIndex(0),
              padding(0)
        {
        }

        float depthRange[4];
        float viewCoords[4];
        float depthFront[4];
        float viewScale[2];
        // multiviewWriteToViewportIndex is used to select either the side-by-side or layered
        // code-path in the GS. It's value, if set, is either 0.0f or 1.0f. The value is updated
        // whenever a multi-view draw framebuffer is made active.
        float multiviewWriteToViewportIndex;

        // Added here to manually pad the struct.
        float padding;
    };

    struct Compute
    {
        Compute() : numWorkGroups{0u}, padding(0u) {}
        unsigned int numWorkGroups[3];
        unsigned int padding;  // This just pads the struct to 16 bytes
    };

    struct SamplerMetadata
    {
        SamplerMetadata() : baseLevel(0), internalFormatBits(0), wrapModes(0), padding(0) {}

        int baseLevel;
        int internalFormatBits;
        int wrapModes;
        int padding;  // This just pads the struct to 16 bytes
    };

    static_assert(sizeof(SamplerMetadata) == 16u,
                  "Sampler metadata struct must be one 4-vec / 16 bytes.");

    // Return true if dirty.
    bool updateSamplerMetadata(SamplerMetadata *data, const gl::Texture &texture);

    Vertex mVertex;
    bool mVertexDirty;
    Pixel mPixel;
    bool mPixelDirty;
    Compute mCompute;
    bool mComputeDirty;

    std::vector<SamplerMetadata> mSamplerMetadataVS;
    bool mSamplerMetadataVSDirty;
    std::vector<SamplerMetadata> mSamplerMetadataPS;
    bool mSamplerMetadataPSDirty;
    std::vector<SamplerMetadata> mSamplerMetadataCS;
    bool mSamplerMetadataCSDirty;
};

class StateManager11 final : angle::NonCopyable
{
  public:
    StateManager11(Renderer11 *renderer);
    ~StateManager11();

    gl::Error initialize(const gl::Caps &caps, const gl::Extensions &extensions);
    void deinitialize();

    void syncState(const gl::Context *context, const gl::State::DirtyBits &dirtyBits);

    gl::Error updateStateForCompute(const gl::Context *context,
                                    GLuint numGroupsX,
                                    GLuint numGroupsY,
                                    GLuint numGroupsZ);

    void updateStencilSizeIfChanged(bool depthStencilInitialized, unsigned int stencilSize);

    void setShaderResource(gl::SamplerType shaderType,
                           UINT resourceSlot,
                           ID3D11ShaderResourceView *srv);

    // Checks are done on a framebuffer state change to trigger other state changes.
    // The Context is allowed to be nullptr for these methods, when called in EGL init code.
    void invalidateRenderTarget(const gl::Context *context);
    void invalidateBoundViews(const gl::Context *context);
    void invalidateVertexBuffer();
    void invalidateEverything(const gl::Context *context);
    void invalidateViewport(const gl::Context *context);
    void invalidateTexturesAndSamplers();
    void invalidateSwizzles();
    void invalidateDriverUniforms();

    // Called from VertexArray11::updateVertexAttribStorage.
    void invalidateCurrentValueAttrib(size_t attribIndex);

    void setOneTimeRenderTarget(const gl::Context *context,
                                ID3D11RenderTargetView *rtv,
                                ID3D11DepthStencilView *dsv);
    void setOneTimeRenderTargets(const gl::Context *context,
                                 ID3D11RenderTargetView **rtvs,
                                 UINT numRtvs,
                                 ID3D11DepthStencilView *dsv);

    void onBeginQuery(Query11 *query);
    void onDeleteQueryObject(Query11 *query);
    gl::Error onMakeCurrent(const gl::Context *context);

    void setInputLayout(const d3d11::InputLayout *inputLayout);

    // TODO(jmadill): Migrate to d3d11::Buffer.
    bool queueVertexBufferChange(size_t bufferIndex,
                                 ID3D11Buffer *buffer,
                                 UINT stride,
                                 UINT offset);
    bool queueVertexOffsetChange(size_t bufferIndex, UINT offsetOnly);
    void applyVertexBufferChanges();

    void setSingleVertexBuffer(const d3d11::Buffer *buffer, UINT stride, UINT offset);

    gl::Error updateState(const gl::Context *context, GLenum drawMode);

    void setPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY primitiveTopology);

    void setDrawShaders(const d3d11::VertexShader *vertexShader,
                        const d3d11::GeometryShader *geometryShader,
                        const d3d11::PixelShader *pixelShader);
    void setVertexShader(const d3d11::VertexShader *shader);
    void setGeometryShader(const d3d11::GeometryShader *shader);
    void setPixelShader(const d3d11::PixelShader *shader);
    void setComputeShader(const d3d11::ComputeShader *shader);

    // Not handled by an internal dirty bit because of the extra draw parameters.
    gl::Error applyVertexBuffer(const gl::Context *context,
                                GLenum mode,
                                GLint first,
                                GLsizei count,
                                GLsizei instances,
                                TranslatedIndexData *indexInfo);

    gl::Error applyIndexBuffer(const gl::ContextState &data,
                               const void *indices,
                               GLsizei count,
                               GLenum type,
                               TranslatedIndexData *indexInfo);

    void setIndexBuffer(ID3D11Buffer *buffer,
                        DXGI_FORMAT indexFormat,
                        unsigned int offset,
                        bool indicesChanged);

    gl::Error updateVertexOffsetsForPointSpritesEmulation(GLint startVertex,
                                                          GLsizei emulatedInstanceId);

    // TODO(jmadill): Should be private.
    gl::Error applyComputeUniforms(ProgramD3D *programD3D);

    // Only used in testing.
    InputLayoutCache *getInputLayoutCache() { return &mInputLayoutCache; }

  private:
    void unsetConflictingSRVs(gl::SamplerType shaderType,
                              uintptr_t resource,
                              const gl::ImageIndex &index);
    void unsetConflictingAttachmentResources(const gl::FramebufferAttachment *attachment,
                                             ID3D11Resource *resource);

    gl::Error syncBlendState(const gl::Context *context,
                             const gl::Framebuffer *framebuffer,
                             const gl::BlendState &blendState,
                             const gl::ColorF &blendColor,
                             unsigned int sampleMask);

    gl::Error syncDepthStencilState(const gl::State &glState);

    gl::Error syncRasterizerState(const gl::Context *context, bool pointDrawMode);

    void syncScissorRectangle(const gl::Rectangle &scissor, bool enabled);

    void syncViewport(const gl::Context *context);

    void checkPresentPath(const gl::Context *context);

    gl::Error syncFramebuffer(const gl::Context *context, gl::Framebuffer *framebuffer);
    gl::Error syncProgram(const gl::Context *context, GLenum drawMode);

    gl::Error syncTextures(const gl::Context *context);
    gl::Error applyTextures(const gl::Context *context,
                            gl::SamplerType shaderType,
                            const FramebufferTextureArray &framebufferTextures,
                            size_t framebufferTextureCount);

    gl::Error setSamplerState(const gl::Context *context,
                              gl::SamplerType type,
                              int index,
                              gl::Texture *texture,
                              const gl::SamplerState &sampler);
    gl::Error setTexture(const gl::Context *context,
                         gl::SamplerType type,
                         int index,
                         gl::Texture *texture);

    // Faster than calling setTexture a jillion times
    gl::Error clearTextures(gl::SamplerType samplerType, size_t rangeStart, size_t rangeEnd);
    void handleMultiviewDrawFramebufferChange(const gl::Context *context);

    gl::Error syncCurrentValueAttribs(const gl::State &state);

    gl::Error generateSwizzle(const gl::Context *context, gl::Texture *texture);
    gl::Error generateSwizzlesForShader(const gl::Context *context, gl::SamplerType type);
    gl::Error generateSwizzles(const gl::Context *context);

    gl::Error applyDriverUniforms(const ProgramD3D &programD3D, GLenum drawMode);
    gl::Error applyUniforms(ProgramD3D *programD3D);

    enum DirtyBitType
    {
        DIRTY_BIT_RENDER_TARGET,
        DIRTY_BIT_VIEWPORT_STATE,
        DIRTY_BIT_SCISSOR_STATE,
        DIRTY_BIT_RASTERIZER_STATE,
        DIRTY_BIT_BLEND_STATE,
        DIRTY_BIT_DEPTH_STENCIL_STATE,
        DIRTY_BIT_TEXTURE_AND_SAMPLER_STATE,
        DIRTY_BIT_INVALID,
        DIRTY_BIT_MAX = DIRTY_BIT_INVALID,
    };

    using DirtyBits = angle::BitSet<DIRTY_BIT_MAX>;

    Renderer11 *mRenderer;

    // Internal dirty bits.
    DirtyBits mInternalDirtyBits;

    // Blend State
    gl::BlendState mCurBlendState;
    gl::ColorF mCurBlendColor;
    unsigned int mCurSampleMask;

    // Currently applied depth stencil state
    gl::DepthStencilState mCurDepthStencilState;
    int mCurStencilRef;
    int mCurStencilBackRef;
    unsigned int mCurStencilSize;
    Optional<bool> mCurDisableDepth;
    Optional<bool> mCurDisableStencil;

    // Currently applied rasterizer state
    gl::RasterizerState mCurRasterState;

    // Currently applied scissor rectangle state
    bool mCurScissorEnabled;
    gl::Rectangle mCurScissorRect;

    // Currently applied viewport state
    gl::Rectangle mCurViewport;
    float mCurNear;
    float mCurFar;

    // The viewport offsets are guaranteed to be updated whenever the gl::State::DirtyBits are
    // resolved and can be applied to the viewport and scissor whenever the internal viewport and
    // scissor bits are resolved.
    std::vector<gl::Offset> mViewportOffsets;

    // Things needed in viewport state
    ShaderConstants11 mShaderConstants;

    // Render target variables
    gl::Extents mViewportBounds;

    // EGL_ANGLE_experimental_present_path variables
    bool mCurPresentPathFastEnabled;
    int mCurPresentPathFastColorBufferHeight;

    // Queries that are currently active in this state
    std::set<Query11 *> mCurrentQueries;

    // Currently applied textures
    struct SRVRecord
    {
        uintptr_t srv;
        uintptr_t resource;
        D3D11_SHADER_RESOURCE_VIEW_DESC desc;
    };

    // A cache of current SRVs that also tracks the highest 'used' (non-NULL) SRV
    // We might want to investigate a more robust approach that is also fast when there's
    // a large gap between used SRVs (e.g. if SRV 0 and 7 are non-NULL, this approach will
    // waste time on SRVs 1-6.)
    class SRVCache : angle::NonCopyable
    {
      public:
        SRVCache() : mHighestUsedSRV(0) {}

        void initialize(size_t size) { mCurrentSRVs.resize(size); }

        size_t size() const { return mCurrentSRVs.size(); }
        size_t highestUsed() const { return mHighestUsedSRV; }

        const SRVRecord &operator[](size_t index) const { return mCurrentSRVs[index]; }
        void clear();
        void update(size_t resourceIndex, ID3D11ShaderResourceView *srv);

      private:
        std::vector<SRVRecord> mCurrentSRVs;
        size_t mHighestUsedSRV;
    };

    SRVCache mCurVertexSRVs;
    SRVCache mCurPixelSRVs;

    // A block of NULL pointers, cached so we don't re-allocate every draw call
    std::vector<ID3D11ShaderResourceView *> mNullSRVs;

    // Current translations of "Current-Value" data - owned by Context, not VertexArray.
    gl::AttributesMask mDirtyCurrentValueAttribs;
    std::vector<TranslatedAttribute> mCurrentValueAttribs;

    // Current applied input layout.
    ResourceSerial mCurrentInputLayout;
    bool mInputLayoutIsDirty;

    // Current applied vertex states.
    // TODO(jmadill): Figure out how to use ResourceSerial here.
    std::array<ID3D11Buffer *, gl::MAX_VERTEX_ATTRIBS> mCurrentVertexBuffers;
    std::array<UINT, gl::MAX_VERTEX_ATTRIBS> mCurrentVertexStrides;
    std::array<UINT, gl::MAX_VERTEX_ATTRIBS> mCurrentVertexOffsets;
    gl::RangeUI mDirtyVertexBufferRange;

    // Currently applied primitive topology
    D3D11_PRIMITIVE_TOPOLOGY mCurrentPrimitiveTopology;

    // Currently applied shaders
    ResourceSerial mAppliedVertexShader;
    ResourceSerial mAppliedGeometryShader;
    ResourceSerial mAppliedPixelShader;
    ResourceSerial mAppliedComputeShader;

    // Currently applied sampler states
    std::vector<bool> mForceSetVertexSamplerStates;
    std::vector<gl::SamplerState> mCurVertexSamplerStates;

    std::vector<bool> mForceSetPixelSamplerStates;
    std::vector<gl::SamplerState> mCurPixelSamplerStates;

    std::vector<bool> mForceSetComputeSamplerStates;
    std::vector<gl::SamplerState> mCurComputeSamplerStates;

    // Special dirty bit for swizzles. Since they use internal shaders, must be done in a pre-pass.
    bool mDirtySwizzles;

    // Currently applied index buffer
    ID3D11Buffer *mAppliedIB;
    DXGI_FORMAT mAppliedIBFormat;
    unsigned int mAppliedIBOffset;
    bool mAppliedIBChanged;

    // Vertex, index and input layouts
    VertexDataManager mVertexDataManager;
    IndexDataManager mIndexDataManager;
    InputLayoutCache mInputLayoutCache;
    std::vector<const TranslatedAttribute *> mCurrentAttributes;
    Optional<GLint> mLastFirstVertex;

    // ANGLE_multiview.
    bool mIsMultiviewEnabled;

    // Driver Constants.
    d3d11::Buffer mDriverConstantBufferVS;
    d3d11::Buffer mDriverConstantBufferPS;
    d3d11::Buffer mDriverConstantBufferCS;

    ResourceSerial mCurrentVertexConstantBuffer;
    ResourceSerial mCurrentPixelConstantBuffer;
    ResourceSerial mCurrentComputeConstantBuffer;
    ResourceSerial mCurrentGeometryConstantBuffer;
};

}  // namespace rx
#endif  // LIBANGLE_RENDERER_D3D11_STATEMANAGER11_H_
