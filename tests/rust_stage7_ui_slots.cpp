#define CINTERFACE
#include <d3d11.h>
#include <cstddef>
#include <cstdint>
#define S(type,method) static_cast<std::uint32_t>(offsetof(type##Vtbl,method)/sizeof(void*))
extern "C" std::uint32_t ui_device_slot(unsigned method){
 const std::uint32_t slots[]={S(ID3D11Device,QueryInterface),S(ID3D11Device,AddRef),S(ID3D11Device,Release),S(ID3D11Device,CreateBuffer),S(ID3D11Device,CreateTexture2D),S(ID3D11Device,CreateShaderResourceView),S(ID3D11Device,CreateInputLayout),S(ID3D11Device,CreateVertexShader),S(ID3D11Device,CreatePixelShader),S(ID3D11Device,CreateBlendState),S(ID3D11Device,CreateDepthStencilState),S(ID3D11Device,CreateRasterizerState),S(ID3D11Device,CreateSamplerState),S(ID3D11Device,GetFeatureLevel)};
 return slots[method];
}
extern "C" std::uint32_t ui_context_slot(unsigned method){
 const std::uint32_t slots[]={S(ID3D11DeviceContext,GetDevice),S(ID3D11DeviceContext,GetType),S(ID3D11DeviceContext,Map),S(ID3D11DeviceContext,Unmap),S(ID3D11DeviceContext,OMSetRenderTargets),S(ID3D11DeviceContext,RSSetViewports),S(ID3D11DeviceContext,IASetInputLayout),S(ID3D11DeviceContext,IASetVertexBuffers),S(ID3D11DeviceContext,IASetIndexBuffer),S(ID3D11DeviceContext,IASetPrimitiveTopology),S(ID3D11DeviceContext,VSSetShader),S(ID3D11DeviceContext,HSSetShader),S(ID3D11DeviceContext,DSSetShader),S(ID3D11DeviceContext,GSSetShader),S(ID3D11DeviceContext,VSSetConstantBuffers),S(ID3D11DeviceContext,PSSetShader),S(ID3D11DeviceContext,PSSetConstantBuffers),S(ID3D11DeviceContext,PSSetShaderResources),S(ID3D11DeviceContext,PSSetSamplers),S(ID3D11DeviceContext,RSSetState),S(ID3D11DeviceContext,OMSetBlendState),S(ID3D11DeviceContext,OMSetDepthStencilState),S(ID3D11DeviceContext,DrawIndexed),S(ID3D11DeviceContext,Draw)};
 return slots[method];
}
extern "C" std::uint32_t ui_child_slot(unsigned method){
 const std::uint32_t slots[]={S(ID3D11DeviceChild,GetDevice),S(ID3D11View,GetResource),S(ID3D11RenderTargetView,GetDesc),S(ID3D11Texture2D,GetDesc)};return slots[method];
}
