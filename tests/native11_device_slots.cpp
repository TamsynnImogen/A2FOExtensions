// Ask the C form of the installed Windows SDK for COM layout. The private
// failure fixture does not guess vtable lengths or method slot offsets.
#define CINTERFACE
#include <d3d11.h>
#include <cstddef>
extern "C" unsigned native11_sdk_device_slots(){return sizeof(ID3D11DeviceVtbl)/sizeof(void*);}
extern "C" unsigned native11_sdk_depth_view_slot(){return offsetof(ID3D11DeviceVtbl,CreateDepthStencilView)/sizeof(void*);}
extern "C" unsigned native11_sdk_texture_slot(){return offsetof(ID3D11DeviceVtbl,CreateTexture2D)/sizeof(void*);}

extern "C" unsigned native11_sdk_context_map_slot(){return offsetof(ID3D11DeviceContextVtbl,Map)/sizeof(void*);}
extern "C" unsigned native11_sdk_swap_slots(){return sizeof(IDXGISwapChainVtbl)/sizeof(void*);}
extern "C" unsigned native11_sdk_swap_present_slot(){return offsetof(IDXGISwapChainVtbl,Present)/sizeof(void*);}
extern "C" unsigned native11_sdk_swap_resize_slot(){return offsetof(IDXGISwapChainVtbl,ResizeBuffers)/sizeof(void*);}
