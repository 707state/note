// Minimal D3DX12 compatibility helpers used by the HelloTriangle sample.
// This keeps the CMake port on the installed Windows SDK without depending on
// the Agility SDK header package.

#pragma once

#include <d3d12.h>

struct CD3DX12_DEFAULT {};
extern const __declspec(selectany) CD3DX12_DEFAULT D3D12_DEFAULT;

struct CD3DX12_RECT : public D3D12_RECT
{
    CD3DX12_RECT() = default;
    explicit CD3DX12_RECT(const D3D12_RECT& other) noexcept : D3D12_RECT(other) {}
    CD3DX12_RECT(LONG left, LONG top, LONG right, LONG bottom) noexcept
    {
        this->left = left;
        this->top = top;
        this->right = right;
        this->bottom = bottom;
    }
};

struct CD3DX12_VIEWPORT : public D3D12_VIEWPORT
{
    CD3DX12_VIEWPORT() = default;
    explicit CD3DX12_VIEWPORT(const D3D12_VIEWPORT& other) noexcept : D3D12_VIEWPORT(other) {}
    CD3DX12_VIEWPORT(
        FLOAT topLeftX,
        FLOAT topLeftY,
        FLOAT width,
        FLOAT height,
        FLOAT minDepth = D3D12_MIN_DEPTH,
        FLOAT maxDepth = D3D12_MAX_DEPTH) noexcept
    {
        TopLeftX = topLeftX;
        TopLeftY = topLeftY;
        Width = width;
        Height = height;
        MinDepth = minDepth;
        MaxDepth = maxDepth;
    }
};

struct CD3DX12_CPU_DESCRIPTOR_HANDLE : public D3D12_CPU_DESCRIPTOR_HANDLE
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE() = default;
    explicit CD3DX12_CPU_DESCRIPTOR_HANDLE(const D3D12_CPU_DESCRIPTOR_HANDLE& other) noexcept
        : D3D12_CPU_DESCRIPTOR_HANDLE(other) {}

    CD3DX12_CPU_DESCRIPTOR_HANDLE(
        const D3D12_CPU_DESCRIPTOR_HANDLE& other,
        INT offsetScaledByIncrementSize,
        UINT descriptorIncrementSize) noexcept
        : D3D12_CPU_DESCRIPTOR_HANDLE(other)
    {
        Offset(offsetScaledByIncrementSize, descriptorIncrementSize);
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE& Offset(INT offsetScaledByIncrementSize, UINT descriptorIncrementSize) noexcept
    {
        ptr = SIZE_T(INT64(ptr) + INT64(offsetScaledByIncrementSize) * INT64(descriptorIncrementSize));
        return *this;
    }
};

struct CD3DX12_ROOT_SIGNATURE_DESC : public D3D12_ROOT_SIGNATURE_DESC
{
    CD3DX12_ROOT_SIGNATURE_DESC() = default;

    CD3DX12_ROOT_SIGNATURE_DESC(
        UINT numParameters,
        const D3D12_ROOT_PARAMETER* parameters,
        UINT numStaticSamplers = 0,
        const D3D12_STATIC_SAMPLER_DESC* staticSamplers = nullptr,
        D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE) noexcept
    {
        Init(numParameters, parameters, numStaticSamplers, staticSamplers, flags);
    }

    void Init(
        UINT numParameters,
        const D3D12_ROOT_PARAMETER* parameters,
        UINT numStaticSamplers = 0,
        const D3D12_STATIC_SAMPLER_DESC* staticSamplers = nullptr,
        D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE) noexcept
    {
        NumParameters = numParameters;
        pParameters = parameters;
        NumStaticSamplers = numStaticSamplers;
        pStaticSamplers = staticSamplers;
        Flags = flags;
    }
};

struct CD3DX12_SHADER_BYTECODE : public D3D12_SHADER_BYTECODE
{
    CD3DX12_SHADER_BYTECODE() = default;
    CD3DX12_SHADER_BYTECODE(const void* shaderBytecode, SIZE_T bytecodeLength) noexcept
    {
        pShaderBytecode = shaderBytecode;
        BytecodeLength = bytecodeLength;
    }
};

struct CD3DX12_RASTERIZER_DESC : public D3D12_RASTERIZER_DESC
{
    CD3DX12_RASTERIZER_DESC() = default;

    explicit CD3DX12_RASTERIZER_DESC(CD3DX12_DEFAULT) noexcept
    {
        FillMode = D3D12_FILL_MODE_SOLID;
        CullMode = D3D12_CULL_MODE_BACK;
        FrontCounterClockwise = FALSE;
        DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        DepthClipEnable = TRUE;
        MultisampleEnable = FALSE;
        AntialiasedLineEnable = FALSE;
        ForcedSampleCount = 0;
        ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    }
};

struct CD3DX12_RENDER_TARGET_BLEND_DESC : public D3D12_RENDER_TARGET_BLEND_DESC
{
    CD3DX12_RENDER_TARGET_BLEND_DESC() = default;

    explicit CD3DX12_RENDER_TARGET_BLEND_DESC(CD3DX12_DEFAULT) noexcept
    {
        BlendEnable = FALSE;
        LogicOpEnable = FALSE;
        SrcBlend = D3D12_BLEND_ONE;
        DestBlend = D3D12_BLEND_ZERO;
        BlendOp = D3D12_BLEND_OP_ADD;
        SrcBlendAlpha = D3D12_BLEND_ONE;
        DestBlendAlpha = D3D12_BLEND_ZERO;
        BlendOpAlpha = D3D12_BLEND_OP_ADD;
        LogicOp = D3D12_LOGIC_OP_NOOP;
        RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }
};

struct CD3DX12_BLEND_DESC : public D3D12_BLEND_DESC
{
    CD3DX12_BLEND_DESC() = default;

    explicit CD3DX12_BLEND_DESC(CD3DX12_DEFAULT) noexcept
    {
        AlphaToCoverageEnable = FALSE;
        IndependentBlendEnable = FALSE;
        const CD3DX12_RENDER_TARGET_BLEND_DESC defaultRenderTarget(D3D12_DEFAULT);
        for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
        {
            RenderTarget[i] = defaultRenderTarget;
        }
    }
};

struct CD3DX12_HEAP_PROPERTIES : public D3D12_HEAP_PROPERTIES
{
    CD3DX12_HEAP_PROPERTIES() = default;

    explicit CD3DX12_HEAP_PROPERTIES(
        D3D12_HEAP_TYPE type,
        UINT creationNodeMask = 1,
        UINT nodeMask = 1) noexcept
    {
        Type = type;
        CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        CreationNodeMask = creationNodeMask;
        VisibleNodeMask = nodeMask;
    }
};

struct CD3DX12_RESOURCE_DESC : public D3D12_RESOURCE_DESC
{
    CD3DX12_RESOURCE_DESC() = default;

    static CD3DX12_RESOURCE_DESC Buffer(
        UINT64 width,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
        UINT64 alignment = 0) noexcept
    {
        CD3DX12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Alignment = alignment;
        desc.Width = width;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        desc.Flags = flags;
        return desc;
    }
};

struct CD3DX12_RANGE : public D3D12_RANGE
{
    CD3DX12_RANGE() = default;
    CD3DX12_RANGE(SIZE_T begin, SIZE_T end) noexcept
    {
        Begin = begin;
        End = end;
    }
};

struct CD3DX12_RESOURCE_BARRIER : public D3D12_RESOURCE_BARRIER
{
    CD3DX12_RESOURCE_BARRIER() = default;

    static CD3DX12_RESOURCE_BARRIER Transition(
        ID3D12Resource* resource,
        D3D12_RESOURCE_STATES stateBefore,
        D3D12_RESOURCE_STATES stateAfter,
        UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        D3D12_RESOURCE_BARRIER_FLAGS flags = D3D12_RESOURCE_BARRIER_FLAG_NONE) noexcept
    {
        CD3DX12_RESOURCE_BARRIER result = {};
        result.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        result.Flags = flags;
        auto& transition = static_cast<D3D12_RESOURCE_BARRIER&>(result).Transition;
        transition.pResource = resource;
        transition.StateBefore = stateBefore;
        transition.StateAfter = stateAfter;
        transition.Subresource = subresource;
        return result;
    }
};
