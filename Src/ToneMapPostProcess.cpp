//--------------------------------------------------------------------------------------
// File: ToneMapPostProcess.cpp
//
// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.
//
// http://go.microsoft.com/fwlink/?LinkId=248929
//--------------------------------------------------------------------------------------

#include "pch.h"
#include "PostProcess.h"

#include "AlignedNew.h"
#include "CommonStates.h"
#include "ConstantBuffer.h"
#include "DemandCreate.h"
#include "DirectXHelpers.h"
#include "SharedResourcePool.h"

using namespace DirectX;

using Microsoft::WRL::ComPtr;

namespace
{
    const int Dirty_ConstantBuffer  = 0x01;
    const int Dirty_Parameters      = 0x02;

    // Constant buffer layout. Must match the shader!
    __declspec(align(16)) struct ToneMapConstants
    {
        // linearExposure is .x
        // paperWhiteNits is .y
        XMVECTOR parameters;
    };

}

// Include the precompiled shader code.
namespace
{
#include "Shaders/Compiled/ToneMap_VSQuad.inc"
#include "Shaders/Compiled/ToneMap_PSHDR10.inc"
}

namespace
{
    struct ShaderBytecode
    {
        void const* code;
        size_t length;
    };

    const ShaderBytecode pixelShader = { ToneMap_PSHDR10, sizeof(ToneMap_PSHDR10) };

    // Factory for lazily instantiating shaders.
    class DeviceResources
    {
    public:
        DeviceResources(_In_ ID3D11Device* device)
            : stateObjects(device),
            mDevice(device),
            mVertexShader{},
            mPixelShader(),
            mMutex{}
        { }

        // Gets or lazily creates the vertex shader.
        ID3D11VertexShader* GetVertexShader()
        {
            return DemandCreate(mVertexShader, mMutex, [&](ID3D11VertexShader** pResult) -> HRESULT
            {
                HRESULT hr = mDevice->CreateVertexShader(ToneMap_VSQuad, sizeof(ToneMap_VSQuad), nullptr, pResult);

                if (SUCCEEDED(hr))
                    SetDebugObjectName(*pResult, "ToneMapPostProcess");

                return hr;
            });
        }

        // Gets or lazily creates the specified pixel shader.
        ID3D11PixelShader* GetPixelShader()
        {

            return DemandCreate(mPixelShader, mMutex, [&](ID3D11PixelShader** pResult) -> HRESULT
            {
                HRESULT hr = mDevice->CreatePixelShader(pixelShader.code, pixelShader.length, nullptr, pResult);

                if (SUCCEEDED(hr))
                    SetDebugObjectName(*pResult, "ToneMapPostProcess");

                return hr;
            });
        }

        CommonStates                stateObjects;

    protected:
        ComPtr<ID3D11Device>        mDevice;
        ComPtr<ID3D11VertexShader>  mVertexShader;
        ComPtr<ID3D11PixelShader>   mPixelShader;
        std::mutex                  mMutex;
    };
}

class ToneMapPostProcess::Impl : public AlignedNew<ToneMapConstants>
{
public:
    Impl(_In_ ID3D11Device* device);

    void Process(_In_ ID3D11DeviceContext* deviceContext, std::function<void __cdecl()>& setCustomState);

    void SetDirtyFlag() { mDirtyFlags = INT_MAX; }

    // Fields.
    ToneMapConstants                        constants;
    ComPtr<ID3D11ShaderResourceView>        hdrTexture;
    float                                   linearExposure;
    float                                   paperWhiteNits;

    Operator                                op;
    TransferFunction                        func;
    bool                                    mrt;

private:
    int                                     mDirtyFlags;

    ConstantBuffer<ToneMapConstants>        mConstantBuffer;

    // Per-device resources.
    std::shared_ptr<DeviceResources>        mDeviceResources;

    static SharedResourcePool<ID3D11Device*, DeviceResources> deviceResourcesPool;
};


// Global pool of per-device ToneMapPostProcess resources.
SharedResourcePool<ID3D11Device*, DeviceResources> ToneMapPostProcess::Impl::deviceResourcesPool;


// Constructor.
ToneMapPostProcess::Impl::Impl(_In_ ID3D11Device* device)
    : constants{},
    linearExposure(1.f),
    paperWhiteNits(200.f),
    op(None),
    func(ST2084),
    mrt(false),
    mDirtyFlags(INT_MAX),
    mConstantBuffer(device),
    mDeviceResources(deviceResourcesPool.DemandCreate(device))
{
    if (device->GetFeatureLevel() < D3D_FEATURE_LEVEL_10_0)
    {
        throw std::exception("ToneMapPostProcess requires Feature Level 10.0 or later");
    }
}


// Sets our state onto the D3D device.
void ToneMapPostProcess::Impl::Process(_In_ ID3D11DeviceContext* deviceContext, std::function<void __cdecl()>& setCustomState)
{
    // Set the texture.
    ID3D11ShaderResourceView* textures[1] = { hdrTexture.Get() };
    deviceContext->PSSetShaderResources(0, 1, textures);

    auto sampler = mDeviceResources->stateObjects.PointClamp();
    deviceContext->PSSetSamplers(0, 1, &sampler);

    // Set state objects.
    deviceContext->OMSetBlendState(mDeviceResources->stateObjects.Opaque(), nullptr, 0xffffffff);
    deviceContext->OMSetDepthStencilState(mDeviceResources->stateObjects.DepthNone(), 0);
    deviceContext->RSSetState(mDeviceResources->stateObjects.CullNone());

    // Set shaders.
    auto vertexShader = mDeviceResources->GetVertexShader();
    auto pixelShader = mDeviceResources->GetPixelShader();

    deviceContext->VSSetShader(vertexShader, nullptr, 0);
    deviceContext->PSSetShader(pixelShader, nullptr, 0);

    // Set constants.
    if (mDirtyFlags & Dirty_Parameters)
    {
        mDirtyFlags &= ~Dirty_Parameters;
        mDirtyFlags |= Dirty_ConstantBuffer;

        constants.parameters = XMVectorSet(linearExposure, paperWhiteNits, 0.f, 0.f);
    }

    if (mDirtyFlags & Dirty_ConstantBuffer)
    {
        mDirtyFlags &= ~Dirty_ConstantBuffer;
        mConstantBuffer.SetData(deviceContext, constants);
    }

    // Set the constant buffer.
    auto buffer = mConstantBuffer.GetBuffer();

    deviceContext->PSSetConstantBuffers(0, 1, &buffer);

    if (setCustomState)
    {
        setCustomState();
    }

    // Draw quad.
    deviceContext->IASetInputLayout(nullptr);
    deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    deviceContext->Draw(3, 0);
}



// Public constructor.
ToneMapPostProcess::ToneMapPostProcess(_In_ ID3D11Device* device)
  : pImpl(std::make_unique<Impl>(device))
{
}


// Move constructor.
ToneMapPostProcess::ToneMapPostProcess(ToneMapPostProcess&& moveFrom) noexcept
  : pImpl(std::move(moveFrom.pImpl))
{
}


// Move assignment.
ToneMapPostProcess& ToneMapPostProcess::operator= (ToneMapPostProcess&& moveFrom) noexcept
{
    pImpl = std::move(moveFrom.pImpl);
    return *this;
}


// Public destructor.
ToneMapPostProcess::~ToneMapPostProcess()
{
}


// IPostProcess methods.
void ToneMapPostProcess::Process(_In_ ID3D11DeviceContext* deviceContext, _In_opt_ std::function<void __cdecl()> setCustomState)
{
    pImpl->Process(deviceContext, setCustomState);
}

// Properties
void ToneMapPostProcess::SetHDRSourceTexture(_In_opt_ ID3D11ShaderResourceView* value)
{
    pImpl->hdrTexture = value;
}


void ToneMapPostProcess::SetST2084Parameter(float scalingFactor)
{
    pImpl->paperWhiteNits = scalingFactor;
    pImpl->SetDirtyFlag();
}
