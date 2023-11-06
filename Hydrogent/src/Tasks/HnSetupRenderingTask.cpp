/*
 *  Copyright 2023 Diligent Graphics LLC
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

#include "Tasks/HnSetupRenderingTask.hpp"
#include "HnRenderDelegate.hpp"
#include "HnRenderPassState.hpp"
#include "HnTokens.hpp"
#include "HnRenderBuffer.hpp"

#include "DebugUtilities.hpp"
#include "GraphicsAccessories.hpp"

namespace Diligent
{

namespace USD
{

HnSetupRenderingTask::HnSetupRenderingTask(pxr::HdSceneDelegate* ParamsDelegate, const pxr::SdfPath& Id) :
    HnTask{Id},
    m_RenderPassState{std::make_shared<HnRenderPassState>()}
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
    m_OffscreenColorTargetId  = InitBrim(HnRenderResourceTokens->offscreenColorTarget);
    m_MeshIdTargetId          = InitBrim(HnRenderResourceTokens->meshIdTarget);
    m_SelectionDepthBufferId  = InitBrim(HnRenderResourceTokens->selectionDepthBuffer);
    m_DepthBufferId           = InitBrim(HnRenderResourceTokens->depthBuffer);
    m_ClosestSelLocn0TargetId = InitBrim(HnRenderResourceTokens->closestSelectedLocation0Target);
    m_ClosestSelLocn1TargetId = InitBrim(HnRenderResourceTokens->closestSelectedLocation1Target);
}

HnSetupRenderingTask::~HnSetupRenderingTask()
{
}

void HnSetupRenderingTask::UpdateRenderPassState(const HnSetupRenderingTaskParams& Params)
{
    VERIFY_EXPR(Params.ColorFormat != TEX_FORMAT_UNKNOWN);
    m_RenderPassState->SetNumRenderTargets(2);
    m_RenderPassState->SetRenderTargetFormat(0, Params.ColorFormat);
    m_RenderPassState->SetRenderTargetFormat(1, Params.MeshIdFormat);
    m_RenderPassState->SetDepthStencilFormat(Params.DepthFormat);

    m_RenderPassState->SetDepthBias(Params.DepthBias, Params.SlopeScaledDepthBias);
    m_RenderPassState->SetDepthFunc(Params.DepthFunc);
    m_RenderPassState->SetDepthBiasEnabled(Params.DepthBiasEnabled);
    m_RenderPassState->SetEnableDepthTest(Params.DepthTestEnabled);
    m_RenderPassState->SetEnableDepthClamp(Params.DepthClampEnabled);

    m_RenderPassState->SetCullStyle(Params.CullStyle);

    m_RenderPassState->SetStencil(Params.StencilFunc, Params.StencilRef, Params.StencilMask,
                                  Params.StencilFailOp, Params.StencilZFailOp, Params.StencilZPassOp);

    m_RenderPassState->SetFrontFaceCCW(Params.FrontFaceCCW);
    m_RenderPassState->SetClearColor(Params.ClearColor);
    m_RenderPassState->SetClearDepth(Params.ClearDepth);
}

void HnSetupRenderingTask::Sync(pxr::HdSceneDelegate* Delegate,
                                pxr::HdTaskContext*   TaskCtx,
                                pxr::HdDirtyBits*     DirtyBits)
{
    if (*DirtyBits & pxr::HdChangeTracker::DirtyParams)
    {
        HnSetupRenderingTaskParams Params;
        if (GetTaskParams(Delegate, Params))
        {
            m_FinalColorTargetId            = Params.FinalColorTargetId;
            m_ClosestSelectedLocationFormat = Params.ClosestSelectedLocationFormat;
            UpdateRenderPassState(Params);
        }
    }

    *DirtyBits = pxr::HdChangeTracker::Clean;
}

void HnSetupRenderingTask::PrepareRenderTargets(pxr::HdRenderIndex* RenderIndex,
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

    auto UpdateBrim = [&](const pxr::SdfPath& Id, TEXTURE_FORMAT Format, const char* Name) -> ITextureView* {
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
        TargetDesc.Name      = Name;
        TargetDesc.Format    = Format;
        TargetDesc.BindFlags = (IsDepth ? BIND_DEPTH_STENCIL : BIND_RENDER_TARGET) | BIND_SHADER_RESOURCE;

        RefCntAutoPtr<ITexture> pTarget;
        pDevice->CreateTexture(TargetDesc, nullptr, &pTarget);
        if (!pTarget)
        {
            UNEXPECTED("Failed to create ", Name, " texture");
            return nullptr;
        }
        LOG_INFO_MESSAGE("HnSetupRenderingTask: created ", TargetDesc.GetWidth(), "x", TargetDesc.GetHeight(), " ", Name, " texture");

        auto* const pView = pTarget->GetDefaultView(IsDepth ? TEXTURE_VIEW_DEPTH_STENCIL : TEXTURE_VIEW_RENDER_TARGET);
        VERIFY(pView != nullptr, "Failed to get texture view for target ", Name);

        Renderbuffer->SetTarget(pView);

        return pView;
    };

    HnRenderPassState::FramebufferTargets FBTargets;
    FBTargets.FinalColorRTV               = pFinalColorRTV;
    FBTargets.OffscreenColorRTV           = UpdateBrim(m_OffscreenColorTargetId, m_RenderPassState->GetRenderTargetFormat(0), "Offscreen color target");
    FBTargets.MeshIdRTV                   = UpdateBrim(m_MeshIdTargetId, m_RenderPassState->GetRenderTargetFormat(1), "Mesh Id target");
    FBTargets.SelectionDepthDSV           = UpdateBrim(m_SelectionDepthBufferId, m_RenderPassState->GetDepthStencilFormat(), "Selection depth buffer");
    FBTargets.DepthDSV                    = UpdateBrim(m_DepthBufferId, m_RenderPassState->GetDepthStencilFormat(), "Depth buffer");
    FBTargets.ClosestSelectedLocation0RTV = UpdateBrim(m_ClosestSelLocn0TargetId, m_ClosestSelectedLocationFormat, "Closest selected location 0");
    FBTargets.ClosestSelectedLocation1RTV = UpdateBrim(m_ClosestSelLocn1TargetId, m_ClosestSelectedLocationFormat, "Closest selected location 1");
    m_RenderPassState->SetFramebufferTargets(FBTargets);
}

void HnSetupRenderingTask::Prepare(pxr::HdTaskContext* TaskCtx,
                                   pxr::HdRenderIndex* RenderIndex)
{
    (*TaskCtx)[HnTokens->renderPassState]                              = pxr::VtValue{m_RenderPassState};
    (*TaskCtx)[HnRenderResourceTokens->finalColorTarget]               = pxr::VtValue{m_FinalColorTargetId};
    (*TaskCtx)[HnRenderResourceTokens->offscreenColorTarget]           = pxr::VtValue{m_OffscreenColorTargetId};
    (*TaskCtx)[HnRenderResourceTokens->meshIdTarget]                   = pxr::VtValue{m_MeshIdTargetId};
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

void HnSetupRenderingTask::Execute(pxr::HdTaskContext* TaskCtx)
{
    if (m_RenderIndex == nullptr)
    {
        UNEXPECTED("Render index is null. This likely indicates that Prepare() has not been called.");
        return;
    }

    const auto& Targets = m_RenderPassState->GetFramebufferTargets();
    if (!Targets)
    {
        UNEXPECTED("Framebuffer targets are not set");
        return;
    }

    ITextureView* pRTVs[] = {Targets.OffscreenColorRTV, Targets.MeshIdRTV};

    auto* pCtx = static_cast<HnRenderDelegate*>(m_RenderIndex->GetRenderDelegate())->GetDeviceContext();

    // We first render selected objects using the selection depth buffer.
    // Selection depth buffer is copied to the main depth buffer by the HnCopySelectionDepthTask.
    pCtx->SetRenderTargets(_countof(pRTVs), pRTVs, Targets.SelectionDepthDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pCtx->ClearRenderTarget(Targets.OffscreenColorRTV, m_RenderPassState->GetClearColor().Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    constexpr float Zero[] = {0, 0, 0, 0};
    pCtx->ClearRenderTarget(Targets.MeshIdRTV, Zero, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pCtx->ClearDepthStencil(Targets.SelectionDepthDSV, CLEAR_DEPTH_FLAG, m_RenderPassState->GetClearDepth(), 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pCtx->SetStencilRef(m_RenderPassState->GetStencilRef());
}

} // namespace USD

} // namespace Diligent
