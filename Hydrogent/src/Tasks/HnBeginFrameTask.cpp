/*
 *  Copyright 2023-2024 Diligent Graphics LLC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#include "Tasks/HnBeginFrameTask.hpp"
#include "HnRenderDelegate.hpp"
#include "HnRenderPassState.hpp"
#include "HnTokens.hpp"
#include "HnRenderBuffer.hpp"
#include "HnCamera.hpp"
#include "HnLight.hpp"
#include "HnRenderParam.hpp"

#include "DebugUtilities.hpp"
#include "GraphicsAccessories.hpp"
#include "MapHelper.hpp"

namespace Diligent
{

namespace HLSL
{

#include "Shaders/Common/public/BasicStructures.fxh"
#include "Shaders/PBR/public/PBR_Structures.fxh"
#include "Shaders/PBR/private/RenderPBR_Structures.fxh"

} // namespace HLSL

namespace USD
{

HnBeginFrameTaskParams::RenderTargetFormats::RenderTargetFormats() noexcept
{
    GBuffer[HnFramebufferTargets::GBUFFER_TARGET_SCENE_COLOR]   = TEX_FORMAT_RGBA16_FLOAT;
    GBuffer[HnFramebufferTargets::GBUFFER_TARGET_MESH_ID]       = TEX_FORMAT_R32_FLOAT;
    GBuffer[HnFramebufferTargets::GBUFFER_TARGET_NOTION_VECTOR] = TEX_FORMAT_RG16_FLOAT;
    GBuffer[HnFramebufferTargets::GBUFFER_TARGET_NORMAL]        = TEX_FORMAT_RGBA16_FLOAT;
    GBuffer[HnFramebufferTargets::GBUFFER_TARGET_BASE_COLOR]    = TEX_FORMAT_RGBA8_UNORM;
    GBuffer[HnFramebufferTargets::GBUFFER_TARGET_MATERIAL]      = TEX_FORMAT_RG8_UNORM;
    GBuffer[HnFramebufferTargets::GBUFFER_TARGET_IBL]           = TEX_FORMAT_RGBA16_FLOAT;
    static_assert(HnFramebufferTargets::GBUFFER_TARGET_COUNT == 7, "Please initialize default render target formats.");
}

HnBeginFrameTask::HnBeginFrameTask(pxr::HdSceneDelegate* ParamsDelegate, const pxr::SdfPath& Id) :
    HnTask{Id},
    m_RenderPassState{std::make_shared<HnRenderPassState>()},
    m_FrameAttribs{std::make_unique<HLSL::PBRFrameAttribs>()}
{
    if (ParamsDelegate == nullptr)
    {
        UNEXPECTED("ParamsDelegate is null");
        return;
    }
    pxr::HdRenderIndex& RenderIndex = ParamsDelegate->GetRenderIndex();

    // Insert empty Bprims for offscreen render targets into the render index.
    // The render targets will be created when Prepare() is called and the
    // dimensions of the final color target are known.
    auto InitBrim = [&](const pxr::TfToken& Name) {
        pxr::SdfPath Id = GetId().AppendChild(Name);
        RenderIndex.InsertBprim(pxr::HdPrimTypeTokens->renderBuffer, ParamsDelegate, Id);
        return Id;
    };

    m_GBufferTargetIds[HnFramebufferTargets::GBUFFER_TARGET_SCENE_COLOR]   = InitBrim(HnRenderResourceTokens->offscreenColorTarget);
    m_GBufferTargetIds[HnFramebufferTargets::GBUFFER_TARGET_MESH_ID]       = InitBrim(HnRenderResourceTokens->meshIdTarget);
    m_GBufferTargetIds[HnFramebufferTargets::GBUFFER_TARGET_NOTION_VECTOR] = InitBrim(HnRenderResourceTokens->motionVectorsTarget);
    m_GBufferTargetIds[HnFramebufferTargets::GBUFFER_TARGET_NORMAL]        = InitBrim(HnRenderResourceTokens->normalTarget);
    m_GBufferTargetIds[HnFramebufferTargets::GBUFFER_TARGET_BASE_COLOR]    = InitBrim(HnRenderResourceTokens->baseColorTarget);
    m_GBufferTargetIds[HnFramebufferTargets::GBUFFER_TARGET_MATERIAL]      = InitBrim(HnRenderResourceTokens->materialDataTarget);
    m_GBufferTargetIds[HnFramebufferTargets::GBUFFER_TARGET_IBL]           = InitBrim(HnRenderResourceTokens->iblTarget);
    static_assert(HnFramebufferTargets::GBUFFER_TARGET_COUNT == 7, "Please initialize GBuffer BPrims.");

    m_SelectionDepthBufferId  = InitBrim(HnRenderResourceTokens->selectionDepthBuffer);
    m_DepthBufferId           = InitBrim(HnRenderResourceTokens->depthBuffer);
    m_ClosestSelLocn0TargetId = InitBrim(HnRenderResourceTokens->closestSelectedLocation0Target);
    m_ClosestSelLocn1TargetId = InitBrim(HnRenderResourceTokens->closestSelectedLocation1Target);
}

HnBeginFrameTask::~HnBeginFrameTask()
{
}

void HnBeginFrameTask::UpdateRenderPassState(const HnBeginFrameTaskParams& Params)
{
    m_RenderPassState->SetNumRenderTargets(HnFramebufferTargets::GBUFFER_TARGET_COUNT);
    for (Uint32 i = 0; i < HnFramebufferTargets::GBUFFER_TARGET_COUNT; ++i)
        m_RenderPassState->SetRenderTargetFormat(i, Params.Formats.GBuffer[i]);
    m_RenderPassState->SetDepthStencilFormat(Params.Formats.Depth);

    m_RenderPassState->SetDepthBias(Params.State.DepthBias, Params.State.SlopeScaledDepthBias);
    m_RenderPassState->SetDepthFunc(Params.State.DepthFunc);
    m_RenderPassState->SetDepthBiasEnabled(Params.State.DepthBiasEnabled);
    m_RenderPassState->SetEnableDepthTest(Params.State.DepthTestEnabled);
    m_RenderPassState->SetEnableDepthClamp(Params.State.DepthClampEnabled);

    m_RenderPassState->SetCullStyle(Params.State.CullStyle);

    m_RenderPassState->SetStencil(Params.State.StencilFunc, Params.State.StencilRef, Params.State.StencilMask,
                                  Params.State.StencilFailOp, Params.State.StencilZFailOp, Params.State.StencilZPassOp);

    m_RenderPassState->SetFrontFaceCCW(Params.State.FrontFaceCCW);
    m_RenderPassState->SetClearColor(Params.ClearColor);
    m_RenderPassState->SetClearDepth(Params.ClearDepth);
}

void HnBeginFrameTask::Sync(pxr::HdSceneDelegate* Delegate,
                            pxr::HdTaskContext*   TaskCtx,
                            pxr::HdDirtyBits*     DirtyBits)
{
    if (*DirtyBits & pxr::HdChangeTracker::DirtyParams)
    {
        HnBeginFrameTaskParams Params;
        if (GetTaskParams(Delegate, Params))
        {
            m_FinalColorTargetId            = Params.FinalColorTargetId;
            m_ClosestSelectedLocationFormat = Params.Formats.ClosestSelectedLocation;
            m_CameraId                      = Params.CameraId;
            m_RendererParams                = Params.Renderer;
            UpdateRenderPassState(Params);
        }
    }

    *DirtyBits = pxr::HdChangeTracker::Clean;
}

void HnBeginFrameTask::PrepareRenderTargets(pxr::HdRenderIndex* RenderIndex,
                                            pxr::HdTaskContext* TaskCtx,
                                            ITextureView*       pFinalColorRTV)
{
    if (pFinalColorRTV == nullptr)
    {
        UNEXPECTED("Final color target RTV is null");
        return;
    }
    const auto& FinalRTVDesc    = pFinalColorRTV->GetDesc();
    const auto& FinalTargetDesc = pFinalColorRTV->GetTexture()->GetDesc();

    m_FrameBufferWidth  = FinalTargetDesc.Width;
    m_FrameBufferHeight = FinalTargetDesc.Height;

    auto UpdateBrim = [&](const pxr::SdfPath& Id, TEXTURE_FORMAT Format, const std::string& Name) -> ITextureView* {
        if (Format == TEX_FORMAT_UNKNOWN)
            return nullptr;

        VERIFY_EXPR(!Id.IsEmpty());

        HnRenderBuffer* Renderbuffer = static_cast<HnRenderBuffer*>(RenderIndex->GetBprim(pxr::HdPrimTypeTokens->renderBuffer, Id));
        if (Renderbuffer == nullptr)
        {
            UNEXPECTED("Render buffer is not set at Id ", Id);
            return nullptr;
        }

        if (auto* pView = Renderbuffer->GetTarget())
        {
            const auto& ViewDesc   = pView->GetDesc();
            const auto& TargetDesc = pView->GetTexture()->GetDesc();
            if (TargetDesc.GetWidth() == FinalTargetDesc.GetWidth() &&
                TargetDesc.GetHeight() == FinalTargetDesc.GetHeight() &&
                ViewDesc.Format == Format)
                return pView;
        }

        const bool  IsDepth = GetTextureFormatAttribs(Format).IsDepthStencil();
        auto* const pDevice = static_cast<HnRenderDelegate*>(RenderIndex->GetRenderDelegate())->GetDevice();

        auto TargetDesc      = FinalTargetDesc;
        TargetDesc.Name      = Name.c_str();
        TargetDesc.Format    = Format;
        TargetDesc.BindFlags = (IsDepth ? BIND_DEPTH_STENCIL : BIND_RENDER_TARGET) | BIND_SHADER_RESOURCE;

        RefCntAutoPtr<ITexture> pTarget;
        pDevice->CreateTexture(TargetDesc, nullptr, &pTarget);
        if (!pTarget)
        {
            UNEXPECTED("Failed to create ", Name, " texture");
            return nullptr;
        }
        LOG_INFO_MESSAGE("HnBeginFrameTask: created ", TargetDesc.GetWidth(), "x", TargetDesc.GetHeight(), " ", Name, " texture");

        auto* const pView = pTarget->GetDefaultView(IsDepth ? TEXTURE_VIEW_DEPTH_STENCIL : TEXTURE_VIEW_RENDER_TARGET);
        VERIFY(pView != nullptr, "Failed to get texture view for target ", Name);

        Renderbuffer->SetTarget(pView);

        return pView;
    };

    HnFramebufferTargets FBTargets;
    FBTargets.FinalColorRTV = pFinalColorRTV;

    for (Uint32 i = 0; i < HnFramebufferTargets::GBUFFER_TARGET_COUNT; ++i)
    {
        const std::string Name   = std::string{"GBuffer target "} + std::to_string(i);
        FBTargets.GBufferRTVs[i] = UpdateBrim(m_GBufferTargetIds[i], m_RenderPassState->GetRenderTargetFormat(i), Name);
        if (FBTargets.GBufferRTVs[i])
        {
            FBTargets.GBufferSRVs[i] = FBTargets.GBufferRTVs[i]->GetTexture()->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
            VERIFY_EXPR(FBTargets.GBufferSRVs[i] != nullptr);
        }
        else
        {
            UNEXPECTED("Unable to get GBuffer target from Bprim ", m_GBufferTargetIds[i]);
        }
    }
    FBTargets.SelectionDepthDSV           = UpdateBrim(m_SelectionDepthBufferId, m_RenderPassState->GetDepthStencilFormat(), "Selection depth buffer");
    FBTargets.DepthDSV                    = UpdateBrim(m_DepthBufferId, m_RenderPassState->GetDepthStencilFormat(), "Depth buffer");
    FBTargets.ClosestSelectedLocation0RTV = UpdateBrim(m_ClosestSelLocn0TargetId, m_ClosestSelectedLocationFormat, "Closest selected location 0");
    FBTargets.ClosestSelectedLocation1RTV = UpdateBrim(m_ClosestSelLocn1TargetId, m_ClosestSelectedLocationFormat, "Closest selected location 1");
    m_RenderPassState->SetFramebufferTargets(FBTargets);
}

void HnBeginFrameTask::Prepare(pxr::HdTaskContext* TaskCtx,
                               pxr::HdRenderIndex* RenderIndex)
{
    (*TaskCtx)[HnTokens->renderPassState]                = pxr::VtValue{m_RenderPassState};
    (*TaskCtx)[HnRenderResourceTokens->finalColorTarget] = pxr::VtValue{m_FinalColorTargetId};

    (*TaskCtx)[HnRenderResourceTokens->offscreenColorTarget] = pxr::VtValue{m_GBufferTargetIds[HnFramebufferTargets::GBUFFER_TARGET_SCENE_COLOR]};
    (*TaskCtx)[HnRenderResourceTokens->meshIdTarget]         = pxr::VtValue{m_GBufferTargetIds[HnFramebufferTargets::GBUFFER_TARGET_MESH_ID]};
    (*TaskCtx)[HnRenderResourceTokens->normalTarget]         = pxr::VtValue{m_GBufferTargetIds[HnFramebufferTargets::GBUFFER_TARGET_NORMAL]};
    (*TaskCtx)[HnRenderResourceTokens->motionVectorsTarget]  = pxr::VtValue{m_GBufferTargetIds[HnFramebufferTargets::GBUFFER_TARGET_NOTION_VECTOR]};
    (*TaskCtx)[HnRenderResourceTokens->baseColorTarget]      = pxr::VtValue{m_GBufferTargetIds[HnFramebufferTargets::GBUFFER_TARGET_BASE_COLOR]};
    (*TaskCtx)[HnRenderResourceTokens->materialDataTarget]   = pxr::VtValue{m_GBufferTargetIds[HnFramebufferTargets::GBUFFER_TARGET_MATERIAL]};
    (*TaskCtx)[HnRenderResourceTokens->iblTarget]            = pxr::VtValue{m_GBufferTargetIds[HnFramebufferTargets::GBUFFER_TARGET_IBL]};
    static_assert(HnFramebufferTargets::GBUFFER_TARGET_COUNT == 7, "Please initialize all GBuffer targets.");

    (*TaskCtx)[HnRenderResourceTokens->depthBuffer]                    = pxr::VtValue{m_DepthBufferId};
    (*TaskCtx)[HnRenderResourceTokens->selectionDepthBuffer]           = pxr::VtValue{m_SelectionDepthBufferId};
    (*TaskCtx)[HnRenderResourceTokens->closestSelectedLocation0Target] = pxr::VtValue{m_ClosestSelLocn0TargetId};
    (*TaskCtx)[HnRenderResourceTokens->closestSelectedLocation1Target] = pxr::VtValue{m_ClosestSelLocn1TargetId};

    if (ITextureView* pFinalColorRTV = GetRenderBufferTarget(*RenderIndex, m_FinalColorTargetId))
    {
        PrepareRenderTargets(RenderIndex, TaskCtx, pFinalColorRTV);
    }
    else
    {
        UNEXPECTED("Unable to get final color target from Bprim ", m_FinalColorTargetId);
    }

    m_RenderIndex = RenderIndex;
}

void HnBeginFrameTask::UpdateFrameConstants(IDeviceContext* pCtx, IBuffer* pFrameAttrbisCB)
{
    HLSL::PBRFrameAttribs& FrameAttribs = *m_FrameAttribs;

    FrameAttribs.PrevCamera = FrameAttribs.Camera;
    if (!m_CameraId.IsEmpty())
    {
        if (const HnCamera* pCamera = static_cast<const HnCamera*>(m_RenderIndex->GetSprim(pxr::HdPrimTypeTokens->camera, m_CameraId)))
        {
            HLSL::CameraAttribs& CamAttribs = FrameAttribs.Camera;

            const float4x4& ProjMatrix  = pCamera->GetProjectionMatrix();
            const float4x4& ViewMatrix  = pCamera->GetViewMatrix();
            const float4x4& WorldMatrix = pCamera->GetWorldMatrix();
            const float4x4  ViewProj    = ViewMatrix * ProjMatrix;

            VERIFY_EXPR(m_FrameBufferWidth > 0 && m_FrameBufferHeight > 0);
            CamAttribs.f4ViewportSize = float4{
                static_cast<float>(m_FrameBufferWidth),
                static_cast<float>(m_FrameBufferHeight),
                1.f / static_cast<float>(m_FrameBufferWidth),
                1.f / static_cast<float>(m_FrameBufferHeight),
            };

            CamAttribs.mViewT        = ViewMatrix.Transpose();
            CamAttribs.mProjT        = ProjMatrix.Transpose();
            CamAttribs.mViewProjT    = ViewProj.Transpose();
            CamAttribs.mViewInvT     = WorldMatrix.Transpose();
            CamAttribs.mProjInvT     = ProjMatrix.Inverse().Transpose();
            CamAttribs.mViewProjInvT = ViewProj.Inverse().Transpose();
            CamAttribs.f4Position    = float4{float3::MakeVector(WorldMatrix[3]), 1};

            if (FrameAttribs.PrevCamera.f4ViewportSize.x == 0)
            {
                // First frame
                FrameAttribs.PrevCamera = CamAttribs;
            }
        }
        else
        {
            LOG_ERROR_MESSAGE("Camera is not set at Id ", m_CameraId);
        }
    }
    else
    {
        LOG_ERROR_MESSAGE("Camera Id is empty");
    }

    // For now, use the first light that is initialized.
    HnRenderDelegate* RenderDelegate = static_cast<HnRenderDelegate*>(m_RenderIndex->GetRenderDelegate());
    for (HnLight* Light : RenderDelegate->GetLights())
    {
        if (Light->GetDirection() != float3{})
        {
            HLSL::PBRLightAttribs& LightAttribs = FrameAttribs.Light;

            LightAttribs.Direction = Light->GetDirection();
            LightAttribs.Intensity = Light->GetIntensity();

            break;
        }
    }

    {
        HLSL::PBRRendererShaderParameters& RendererParams = FrameAttribs.Renderer;
        RenderDelegate->GetUSDRenderer()->SetInternalShaderParameters(RendererParams);

        RendererParams.OcclusionStrength = m_RendererParams.OcclusionStrength;
        RendererParams.EmissionScale     = m_RendererParams.EmissionScale;
        RendererParams.IBLScale          = m_RendererParams.IBLScale;

        RendererParams.UnshadedColor  = m_RendererParams.UnshadedColor;
        RendererParams.HighlightColor = float4{0, 0, 0, 0};
        RendererParams.PointSize      = m_RendererParams.PointSize;

        // Tone mapping is performed in the post-processing pass
        RendererParams.AverageLogLum = 0.3f;
        RendererParams.MiddleGray    = 0.18f;
        RendererParams.WhitePoint    = 3.0f;
    }

    pCtx->UpdateBuffer(pFrameAttrbisCB, 0, sizeof(FrameAttribs), &FrameAttribs, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    StateTransitionDesc Barriers[] =
        {
            {pFrameAttrbisCB, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE},
        };
    pCtx->TransitionResourceStates(_countof(Barriers), Barriers);
}

void HnBeginFrameTask::Execute(pxr::HdTaskContext* TaskCtx)
{
    if (m_RenderIndex == nullptr)
    {
        UNEXPECTED("Render index is null. This likely indicates that Prepare() has not been called.");
        return;
    }

    const HnFramebufferTargets& Targets = m_RenderPassState->GetFramebufferTargets();
    if (!Targets)
    {
        UNEXPECTED("Framebuffer targets are not set");
        return;
    }

    HnRenderDelegate* RenderDelegate = static_cast<HnRenderDelegate*>(m_RenderIndex->GetRenderDelegate());
    IDeviceContext*   pCtx           = RenderDelegate->GetDeviceContext();

    if (HnRenderParam* pRenderParam = static_cast<HnRenderParam*>(RenderDelegate->GetRenderParam()))
    {
        double CurrFrameTime = m_FrameTimer.GetElapsedTime();
        pRenderParam->SetFrameTime(CurrFrameTime);
        pRenderParam->SetElapsedTime(static_cast<float>(CurrFrameTime - m_LastFrameTime));
        m_LastFrameTime = CurrFrameTime;
    }

    if (IBuffer* pFrameAttribsCB = RenderDelegate->GetFrameAttribsCB())
    {
        UpdateFrameConstants(pCtx, pFrameAttribsCB);
    }
    else
    {
        UNEXPECTED("Frame attribs constant buffer is null");
    }

    auto pRTVs = Targets.GBufferRTVs;

    // We first render selected objects using the selection depth buffer.
    // Selection depth buffer is copied to the main depth buffer by the HnCopySelectionDepthTask.
    pCtx->SetRenderTargets(HnFramebufferTargets::GBUFFER_TARGET_COUNT, pRTVs.data(), Targets.SelectionDepthDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    for (Uint32 i = 0; i < HnFramebufferTargets::GBUFFER_TARGET_COUNT; ++i)
    {
        constexpr float Zero[]     = {0, 0, 0, 0};
        const float*    ClearColor = (i == HnFramebufferTargets::GBUFFER_TARGET_SCENE_COLOR) ? m_RenderPassState->GetClearColor().Data() : Zero;
        pCtx->ClearRenderTarget(pRTVs[i], ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    pCtx->ClearDepthStencil(Targets.SelectionDepthDSV, CLEAR_DEPTH_FLAG, m_RenderPassState->GetClearDepth(), 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pCtx->SetStencilRef(m_RenderPassState->GetStencilRef());
}

} // namespace USD

} // namespace Diligent
