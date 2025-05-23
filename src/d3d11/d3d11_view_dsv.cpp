#include "d3d11_device.h"
#include "d3d11_buffer.h"
#include "d3d11_resource.h"
#include "d3d11_texture.h"
#include "d3d11_view_dsv.h"

namespace dxvk {
  
  D3D11DepthStencilView::D3D11DepthStencilView(
          D3D11Device*                      pDevice,
          ID3D11Resource*                   pResource,
    const D3D11_DEPTH_STENCIL_VIEW_DESC*    pDesc)
  : D3D11DeviceChild<ID3D11DepthStencilView>(pDevice),
    m_resource(pResource), m_desc(*pDesc), m_d3d10(this),
    m_destructionNotifier(this) {
    ResourceAddRefPrivate(m_resource);

    D3D11_COMMON_RESOURCE_DESC resourceDesc;
    GetCommonResourceDesc(pResource, &resourceDesc);

    DxvkImageViewKey viewInfo;
    viewInfo.format = pDevice->LookupFormat(pDesc->Format, DXGI_VK_FORMAT_MODE_DEPTH).Format;
    viewInfo.layout = GetViewLayout();
    viewInfo.aspects = lookupFormatInfo(viewInfo.format)->aspectMask;
    viewInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    
    switch (pDesc->ViewDimension) {
      case D3D11_DSV_DIMENSION_TEXTURE1D:
        viewInfo.viewType   = VK_IMAGE_VIEW_TYPE_1D;
        viewInfo.mipIndex   = pDesc->Texture1D.MipSlice;
        viewInfo.mipCount   = 1;
        viewInfo.layerIndex = 0;
        viewInfo.layerCount = 1;
        break;
        
      case D3D11_DSV_DIMENSION_TEXTURE1DARRAY:
        viewInfo.viewType   = VK_IMAGE_VIEW_TYPE_1D_ARRAY;
        viewInfo.mipIndex   = pDesc->Texture1DArray.MipSlice;
        viewInfo.mipCount   = 1;
        viewInfo.layerIndex = pDesc->Texture1DArray.FirstArraySlice;
        viewInfo.layerCount = pDesc->Texture1DArray.ArraySize;
        break;
        
      case D3D11_DSV_DIMENSION_TEXTURE2D:
        viewInfo.viewType   = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.mipIndex   = pDesc->Texture2D.MipSlice;
        viewInfo.mipCount   = 1;
        viewInfo.layerIndex = 0;
        viewInfo.layerCount = 1;
        break;
        
      case D3D11_DSV_DIMENSION_TEXTURE2DARRAY:
        viewInfo.viewType   = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        viewInfo.mipIndex   = pDesc->Texture2DArray.MipSlice;
        viewInfo.mipCount   = 1;
        viewInfo.layerIndex = pDesc->Texture2DArray.FirstArraySlice;
        viewInfo.layerCount = pDesc->Texture2DArray.ArraySize;
        break;
        
      case D3D11_DSV_DIMENSION_TEXTURE2DMS:
        viewInfo.viewType   = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.mipIndex   = 0;
        viewInfo.mipCount   = 1;
        viewInfo.layerIndex = 0;
        viewInfo.layerCount = 1;
        break;
      
      case D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY:
        viewInfo.viewType   = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        viewInfo.mipIndex   = 0;
        viewInfo.mipCount   = 1;
        viewInfo.layerIndex = pDesc->Texture2DMSArray.FirstArraySlice;
        viewInfo.layerCount = pDesc->Texture2DMSArray.ArraySize;
        break;
      
      default:
        throw DxvkError("D3D11: Invalid view dimension for DSV");
    }
    
    // Normalize view type so that we won't accidentally
    // bind 2D array views and 2D views at the same time
    if (viewInfo.layerCount == 1) {
      if (viewInfo.viewType == VK_IMAGE_VIEW_TYPE_1D_ARRAY)
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_1D;
      if (viewInfo.viewType == VK_IMAGE_VIEW_TYPE_2D_ARRAY)
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    }

    // Populate view info struct
    m_info.pResource = pResource;
    m_info.Dimension = resourceDesc.Dim;
    m_info.BindFlags = resourceDesc.BindFlags;
    m_info.Image.Aspects   = viewInfo.aspects;
    m_info.Image.MinLevel  = viewInfo.mipIndex;
    m_info.Image.MinLayer  = viewInfo.layerIndex;
    m_info.Image.NumLevels = viewInfo.mipCount;
    m_info.Image.NumLayers = viewInfo.layerCount;

    if (m_desc.Flags & D3D11_DSV_READ_ONLY_DEPTH)
      m_info.Image.Aspects &= ~VK_IMAGE_ASPECT_DEPTH_BIT;

    if (m_desc.Flags & D3D11_DSV_READ_ONLY_STENCIL)
      m_info.Image.Aspects &= ~VK_IMAGE_ASPECT_STENCIL_BIT;

    // Create the underlying image view object
    m_view = GetCommonTexture(pResource)->GetImage()->createView(viewInfo);
  }
  
  
  D3D11DepthStencilView::~D3D11DepthStencilView() {
    m_destructionNotifier.Notify();

    ResourceReleasePrivate(m_resource);
    m_resource = nullptr;

    m_view = nullptr;
  }
  
  
  HRESULT STDMETHODCALLTYPE D3D11DepthStencilView::QueryInterface(REFIID riid, void** ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;
    
    if (riid == __uuidof(IUnknown)
     || riid == __uuidof(ID3D11DeviceChild)
     || riid == __uuidof(ID3D11View)
     || riid == __uuidof(ID3D11DepthStencilView)) {
      *ppvObject = ref(this);
      return S_OK;
    }
    
    if (riid == __uuidof(ID3D10DeviceChild)
     || riid == __uuidof(ID3D10View)
     || riid == __uuidof(ID3D10DepthStencilView)) {
      *ppvObject = ref(&m_d3d10);
      return S_OK;
    }

    if (riid == __uuidof(ID3DDestructionNotifier)) {
      *ppvObject = ref(&m_destructionNotifier);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D11DepthStencilView), riid)) {
      Logger::warn("D3D11DepthStencilView::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  }
  
  
  void STDMETHODCALLTYPE D3D11DepthStencilView::GetResource(ID3D11Resource** ppResource) {
    *ppResource = ref(m_resource);
  }
  
  
  void STDMETHODCALLTYPE D3D11DepthStencilView::GetDesc(D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc) {
    *pDesc = m_desc;
  }
  
  
  HRESULT D3D11DepthStencilView::GetDescFromResource(
          ID3D11Resource*                   pResource,
          D3D11_DEPTH_STENCIL_VIEW_DESC*    pDesc) {
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    pDesc->Flags = 0;
    
    switch (resourceDim) {
      case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
        D3D11_TEXTURE1D_DESC resourceDesc;
        static_cast<D3D11Texture1D*>(pResource)->GetDesc(&resourceDesc);
        
        pDesc->Format = resourceDesc.Format;
        
        if (resourceDesc.ArraySize == 1) {
          pDesc->ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1D;
          pDesc->Texture1D.MipSlice = 0;
        } else {
          pDesc->ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1DARRAY;
          pDesc->Texture1DArray.MipSlice        = 0;
          pDesc->Texture1DArray.FirstArraySlice = 0;
          pDesc->Texture1DArray.ArraySize       = resourceDesc.ArraySize;
        }
      } return S_OK;
      
      case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
        D3D11_TEXTURE2D_DESC resourceDesc;
        static_cast<D3D11Texture2D*>(pResource)->GetDesc(&resourceDesc);
        
        pDesc->Format = resourceDesc.Format;
        
        if (resourceDesc.SampleDesc.Count == 1) {
          if (resourceDesc.ArraySize == 1) {
            pDesc->ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            pDesc->Texture2D.MipSlice = 0;
          } else {
            pDesc->ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
            pDesc->Texture2DArray.MipSlice        = 0;
            pDesc->Texture2DArray.FirstArraySlice = 0;
            pDesc->Texture2DArray.ArraySize       = resourceDesc.ArraySize;
          }
        } else {
          if (resourceDesc.ArraySize == 1) {
            pDesc->ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
          } else {
            pDesc->ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
            pDesc->Texture2DMSArray.FirstArraySlice = 0;
            pDesc->Texture2DMSArray.ArraySize       = resourceDesc.ArraySize;
          }
        }
      } return S_OK;
        
      default:
        Logger::err(str::format(
          "D3D11: Unsupported dimension for depth stencil view: ",
          resourceDim));
        return E_INVALIDARG;
    }
  }
  
  
  HRESULT D3D11DepthStencilView::NormalizeDesc(
          ID3D11Resource*                   pResource,
          D3D11_DEPTH_STENCIL_VIEW_DESC*    pDesc) {
    D3D11_RESOURCE_DIMENSION resourceDim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    pResource->GetType(&resourceDim);
    
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    uint32_t numLayers = 0;
    
    switch (resourceDim) {
      case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
        D3D11_TEXTURE1D_DESC resourceDesc;
        static_cast<D3D11Texture1D*>(pResource)->GetDesc(&resourceDesc);
        
        if (pDesc->ViewDimension != D3D11_DSV_DIMENSION_TEXTURE1D
         && pDesc->ViewDimension != D3D11_DSV_DIMENSION_TEXTURE1DARRAY) {
          Logger::err("D3D11: Incompatible view dimension for Texture1D");
          return E_INVALIDARG;
        }
        
        format    = resourceDesc.Format;
        numLayers = resourceDesc.ArraySize;
      } break;
      
      case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
        D3D11_TEXTURE2D_DESC resourceDesc;
        static_cast<D3D11Texture2D*>(pResource)->GetDesc(&resourceDesc);
        
        if (pDesc->ViewDimension != D3D11_DSV_DIMENSION_TEXTURE2D
         && pDesc->ViewDimension != D3D11_DSV_DIMENSION_TEXTURE2DARRAY
         && pDesc->ViewDimension != D3D11_DSV_DIMENSION_TEXTURE2DMS
         && pDesc->ViewDimension != D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY) {
          Logger::err("D3D11: Incompatible view dimension for Texture2D");
          return E_INVALIDARG;
        }
        
        format    = resourceDesc.Format;
        numLayers = resourceDesc.ArraySize;
      } break;
      
      default:
        return E_INVALIDARG;
    }
    
    if (pDesc->Format == DXGI_FORMAT_UNKNOWN)
      pDesc->Format = format;
    
    switch (pDesc->ViewDimension) {
      case D3D11_DSV_DIMENSION_TEXTURE1DARRAY:
        if (pDesc->Texture1DArray.ArraySize > numLayers - pDesc->Texture1DArray.FirstArraySlice)
          pDesc->Texture1DArray.ArraySize = numLayers - pDesc->Texture1DArray.FirstArraySlice;
        break;
      
      case D3D11_DSV_DIMENSION_TEXTURE2DARRAY:
        if (pDesc->Texture2DArray.ArraySize > numLayers - pDesc->Texture2DArray.FirstArraySlice)
          pDesc->Texture2DArray.ArraySize = numLayers - pDesc->Texture2DArray.FirstArraySlice;
        break;
      
      case D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY:
        if (pDesc->Texture2DMSArray.ArraySize > numLayers - pDesc->Texture2DMSArray.FirstArraySlice)
          pDesc->Texture2DMSArray.ArraySize = numLayers - pDesc->Texture2DMSArray.FirstArraySlice;
        break;
      
      default:
        break;
    }
    
    return S_OK;
  }
  

  VkImageLayout D3D11DepthStencilView::GetViewLayout() const {
    switch (m_desc.Flags & (D3D11_DSV_READ_ONLY_DEPTH | D3D11_DSV_READ_ONLY_STENCIL)) {
      default:  // case 0
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      case D3D11_DSV_READ_ONLY_DEPTH:
        return VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR;
      case D3D11_DSV_READ_ONLY_STENCIL:
        return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR;
      case D3D11_DSV_READ_ONLY_DEPTH | D3D11_DSV_READ_ONLY_STENCIL:
        return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    }
  }
}
