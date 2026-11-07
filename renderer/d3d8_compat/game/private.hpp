#pragma once
#include "../../d3d11/texture_handle.hpp"
#include "../../shaders/d3d8_tokens.hpp"
#include "../../shaders/vs11.hpp"
#include "../../state/fvf.hpp"
#include "../../textures/upload.hpp"
#include "factory.hpp"
#include <algorithm>
#include <bitset>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <utility>
namespace a2fo::renderer::game {
class Factory;
class Device;
class Texture8;
class Surface8;
class VertexBuffer8;
class IndexBuffer8;
class PrivateData {
  struct Value {
    std::vector<BYTE> bytes;
    IUnknown *object = nullptr;
    ~Value() {
      if (object)
        object->Release();
    }
  };
  std::map<std::array<BYTE, 16>, std::shared_ptr<Value>> values;

public:
  HRESULT set(REFGUID, const void *, DWORD, DWORD) noexcept;
  HRESULT get(REFGUID, void *, DWORD *) noexcept;
  HRESULT erase(REFGUID) noexcept;
};
struct Resource {
  enum Kind { texture, vertex, index, surface };
  Device *owner;
  Kind kind;
  void *api = nullptr;
  LONG refs = 1, implicit = 0;
  DWORD priority = 0;
  PrivateData private_data;
  void (*destroy)(Resource *) = nullptr;
  Resource(Device *, Kind);
  ~Resource();
  ULONG retain();
  ULONG release();
  void hold();
  void drop();
  HRESULT get_device(IDirect3DDevice8 **);
};
struct Binding {
  Resource *resource = nullptr;
  Binding() = default;
  explicit Binding(Resource *r) : resource(r) {
    if (r)
      r->hold();
  }
  Binding(const Binding &b) : Binding(b.resource) {}
  Binding(Binding &&b) noexcept : resource(b.resource) { b.resource = nullptr; }
  ~Binding() {
    if (resource)
      resource->drop();
  }
  Binding &operator=(Binding b) {
    std::swap(resource, b.resource);
    return *this;
  }
};
struct Shader;
struct Data {
  State gpu;
  std::array<Binding, 8> textures;
  std::array<Binding, 16> streams;
  std::array<UINT, 16> strides{};
  Binding indices;
  UINT base = 0;
  DWORD vertex_shader = D3DFVF_XYZ, pixel_shader = 0;
  std::shared_ptr<Shader> vertex_object;
  D3DCLIPSTATUS8 clip{};
  float planes[6][4]{};
};
struct Mask {
  std::bitset<256> render;
  std::array<std::bitset<33>, 8> stage;
  std::bitset<11> transforms;
  std::bitset<8> textures, lights;
  std::bitset<16> streams;
  std::bitset<96> constants;
  bool viewport = false, material = false, indices = false,
       vertex_shader = false, pixel_shader = false, clip = false;
};
struct Snapshot {
  Data data;
  Mask mask;
};
struct Shader {
  struct Element {
    UINT stream = 0, offset = 0, reg = 0, type = 0;
  };
  std::vector<DWORD> declaration, code;
  std::vector<Element> elements;
  std::string hlsl;
};
class Factory final : public IDirect3D8 {
public:
  IDirect3D8 *native;
  Configuration config;
  LONG refs = 1;
  Factory(IDirect3D8 *, const Configuration &);
  ~Factory();
  void caps(D3DCAPS8 &);
  // IDirect3D8 public SDK declarations.
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release() override;
  HRESULT STDMETHODCALLTYPE
  RegisterSoftwareDevice(void *pInitializeFunction) override;
  UINT STDMETHODCALLTYPE GetAdapterCount() override;
  HRESULT STDMETHODCALLTYPE GetAdapterIdentifier(
      UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER8 *pIdentifier) override;
  UINT STDMETHODCALLTYPE GetAdapterModeCount(UINT Adapter) override;
  HRESULT STDMETHODCALLTYPE EnumAdapterModes(UINT Adapter, UINT Mode,
                                             D3DDISPLAYMODE *pMode) override;
  HRESULT STDMETHODCALLTYPE
  GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE *pMode) override;
  HRESULT STDMETHODCALLTYPE CheckDeviceType(UINT Adapter, D3DDEVTYPE CheckType,
                                            D3DFORMAT DisplayFormat,
                                            D3DFORMAT BackBufferFormat,
                                            WINBOOL Windowed) override;
  HRESULT STDMETHODCALLTYPE CheckDeviceFormat(
      UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage,
      D3DRESOURCETYPE RType, D3DFORMAT CheckFormat) override;
  HRESULT STDMETHODCALLTYPE CheckDeviceMultiSampleType(
      UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat,
      WINBOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType) override;
  HRESULT STDMETHODCALLTYPE CheckDepthStencilMatch(
      UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat,
      D3DFORMAT RenderTargetFormat, D3DFORMAT DepthStencilFormat) override;
  HRESULT STDMETHODCALLTYPE GetDeviceCaps(UINT Adapter, D3DDEVTYPE DeviceType,
                                          D3DCAPS8 *pCaps) override;
  HMONITOR STDMETHODCALLTYPE GetAdapterMonitor(UINT Adapter) override;
  HRESULT STDMETHODCALLTYPE CreateDevice(
      UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow,
      DWORD BehaviorFlags, D3DPRESENT_PARAMETERS *pPresentationParameters,
      struct IDirect3DDevice8 **ppReturnedDeviceInterface) override;
};
class Device final : public IDirect3DDevice8 {
public:
  Factory *factory;
  Configuration config;
  Backend backend;
  LONG refs = 1;
  DWORD thread = 0;
  D3DDEVICE_CREATION_PARAMETERS creation{};
  D3DPRESENT_PARAMETERS presentation{};
  D3DCAPS8 capabilities{};
  Data current;
  std::unique_ptr<Snapshot> recording;
  std::map<DWORD, Snapshot> blocks;
  DWORD next_block = 1;
  std::map<DWORD, std::shared_ptr<Shader>> shaders;
  DWORD next_shader = 0x80000001;
  std::map<void *, Resource *> resources;
  Surface8 *back = nullptr;
  Surface8 *depth = nullptr;
  Binding render_target, depth_target;
  bool in_scene = false, destroying = false;
  std::set<std::string> failures;
  unsigned shader_diagnostics = 0;
  unsigned long long frames = 0, draws = 0, compressed_draws = 0,
                     bc_uploads = 0, bc_live_bytes = 0;
  Device(Factory *, UINT, D3DDEVTYPE, HWND, DWORD,
         const D3DPRESENT_PARAMETERS &);
  ~Device();
  HRESULT initialize();
  HRESULT fail(const char *, HRESULT = invalid_call);
  void defaults();
  Data &setting() { return recording ? recording->data : current; }
  Mask *mask() { return recording ? &recording->mask : nullptr; }
  Resource *find(void *, Resource::Kind);
  HRESULT rebuild_surfaces();
  void apply(Data &, const Data &, const Mask &);
  HRESULT draw(UINT, UINT, UINT, const void *, UINT, const void *, UINT, UINT,
               UINT, UINT);
  HRESULT sample_views(std::array<Sampling, 8> &);
  Matrix *matrix(Data &, DWORD);
  int matrix_slot(DWORD);

  // IDirect3DDevice8 public SDK declarations.
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release() override;
  HRESULT STDMETHODCALLTYPE TestCooperativeLevel() override;
  UINT STDMETHODCALLTYPE GetAvailableTextureMem() override;
  HRESULT STDMETHODCALLTYPE ResourceManagerDiscardBytes(DWORD Bytes) override;
  HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D8 **ppD3D8) override;
  HRESULT STDMETHODCALLTYPE GetDeviceCaps(D3DCAPS8 *pCaps) override;
  HRESULT STDMETHODCALLTYPE GetDisplayMode(D3DDISPLAYMODE *pMode) override;
  HRESULT STDMETHODCALLTYPE
  GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters) override;
  HRESULT STDMETHODCALLTYPE SetCursorProperties(
      UINT XHotSpot, UINT YHotSpot, IDirect3DSurface8 *pCursorBitmap) override;
  void STDMETHODCALLTYPE SetCursorPosition(UINT XScreenSpace, UINT YScreenSpace,
                                           DWORD Flags) override;
  WINBOOL STDMETHODCALLTYPE ShowCursor(WINBOOL bShow) override;
  HRESULT STDMETHODCALLTYPE
  CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS *pPresentationParameters,
                            IDirect3DSwapChain8 **pSwapChain) override;
  HRESULT STDMETHODCALLTYPE
  Reset(D3DPRESENT_PARAMETERS *pPresentationParameters) override;
  HRESULT STDMETHODCALLTYPE Present(const RECT *src_rect, const RECT *dst_rect,
                                    HWND dst_window_override,
                                    const RGNDATA *dirty_region) override;
  HRESULT STDMETHODCALLTYPE
  GetBackBuffer(UINT BackBuffer, D3DBACKBUFFER_TYPE Type,
                IDirect3DSurface8 **ppBackBuffer) override;
  HRESULT STDMETHODCALLTYPE
  GetRasterStatus(D3DRASTER_STATUS *pRasterStatus) override;
  void STDMETHODCALLTYPE SetGammaRamp(DWORD flags,
                                      const D3DGAMMARAMP *ramp) override;
  void STDMETHODCALLTYPE GetGammaRamp(D3DGAMMARAMP *pRamp) override;
  HRESULT STDMETHODCALLTYPE CreateTexture(
      UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format,
      D3DPOOL Pool, IDirect3DTexture8 **ppTexture) override;
  HRESULT STDMETHODCALLTYPE
  CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels,
                      DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
                      IDirect3DVolumeTexture8 **ppVolumeTexture) override;
  HRESULT STDMETHODCALLTYPE CreateCubeTexture(
      UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
      IDirect3DCubeTexture8 **ppCubeTexture) override;
  HRESULT STDMETHODCALLTYPE
  CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool,
                     IDirect3DVertexBuffer8 **ppVertexBuffer) override;
  HRESULT STDMETHODCALLTYPE
  CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
                    IDirect3DIndexBuffer8 **ppIndexBuffer) override;
  HRESULT STDMETHODCALLTYPE
  CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format,
                     D3DMULTISAMPLE_TYPE MultiSample, WINBOOL Lockable,
                     IDirect3DSurface8 **ppSurface) override;
  HRESULT STDMETHODCALLTYPE CreateDepthStencilSurface(
      UINT Width, UINT Height, D3DFORMAT Format,
      D3DMULTISAMPLE_TYPE MultiSample, IDirect3DSurface8 **ppSurface) override;
  HRESULT STDMETHODCALLTYPE
  CreateImageSurface(UINT Width, UINT Height, D3DFORMAT Format,
                     IDirect3DSurface8 **ppSurface) override;
  HRESULT STDMETHODCALLTYPE CopyRects(IDirect3DSurface8 *src_surface,
                                      const RECT *src_rects, UINT rect_count,
                                      IDirect3DSurface8 *dst_surface,
                                      const POINT *dst_points) override;
  HRESULT STDMETHODCALLTYPE
  UpdateTexture(IDirect3DBaseTexture8 *pSourceTexture,
                IDirect3DBaseTexture8 *pDestinationTexture) override;
  HRESULT STDMETHODCALLTYPE
  GetFrontBuffer(IDirect3DSurface8 *pDestSurface) override;
  HRESULT STDMETHODCALLTYPE
  SetRenderTarget(IDirect3DSurface8 *pRenderTarget,
                  IDirect3DSurface8 *pNewZStencil) override;
  HRESULT STDMETHODCALLTYPE
  GetRenderTarget(IDirect3DSurface8 **ppRenderTarget) override;
  HRESULT STDMETHODCALLTYPE
  GetDepthStencilSurface(IDirect3DSurface8 **ppZStencilSurface) override;
  HRESULT STDMETHODCALLTYPE BeginScene() override;
  HRESULT STDMETHODCALLTYPE EndScene() override;
  HRESULT STDMETHODCALLTYPE Clear(DWORD rect_count, const D3DRECT *rects,
                                  DWORD flags, D3DCOLOR color, float z,
                                  DWORD stencil) override;
  HRESULT STDMETHODCALLTYPE SetTransform(D3DTRANSFORMSTATETYPE state,
                                         const D3DMATRIX *matrix) override;
  HRESULT STDMETHODCALLTYPE GetTransform(D3DTRANSFORMSTATETYPE State,
                                         D3DMATRIX *pMatrix) override;
  HRESULT STDMETHODCALLTYPE MultiplyTransform(D3DTRANSFORMSTATETYPE state,
                                              const D3DMATRIX *matrix) override;
  HRESULT STDMETHODCALLTYPE SetViewport(const D3DVIEWPORT8 *viewport) override;
  HRESULT STDMETHODCALLTYPE GetViewport(D3DVIEWPORT8 *pViewport) override;
  HRESULT STDMETHODCALLTYPE SetMaterial(const D3DMATERIAL8 *material) override;
  HRESULT STDMETHODCALLTYPE GetMaterial(D3DMATERIAL8 *pMaterial) override;
  HRESULT STDMETHODCALLTYPE SetLight(DWORD index,
                                     const D3DLIGHT8 *light) override;
  HRESULT STDMETHODCALLTYPE GetLight(DWORD Index, D3DLIGHT8 *pLight) override;
  HRESULT STDMETHODCALLTYPE LightEnable(DWORD Index, WINBOOL Enable) override;
  HRESULT STDMETHODCALLTYPE GetLightEnable(DWORD Index,
                                           WINBOOL *pEnable) override;
  HRESULT STDMETHODCALLTYPE SetClipPlane(DWORD index,
                                         const float *plane) override;
  HRESULT STDMETHODCALLTYPE GetClipPlane(DWORD Index, float *pPlane) override;
  HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE State,
                                           DWORD Value) override;
  HRESULT STDMETHODCALLTYPE GetRenderState(D3DRENDERSTATETYPE State,
                                           DWORD *pValue) override;
  HRESULT STDMETHODCALLTYPE BeginStateBlock() override;
  HRESULT STDMETHODCALLTYPE EndStateBlock(DWORD *pToken) override;
  HRESULT STDMETHODCALLTYPE ApplyStateBlock(DWORD Token) override;
  HRESULT STDMETHODCALLTYPE CaptureStateBlock(DWORD Token) override;
  HRESULT STDMETHODCALLTYPE DeleteStateBlock(DWORD Token) override;
  HRESULT STDMETHODCALLTYPE CreateStateBlock(D3DSTATEBLOCKTYPE Type,
                                             DWORD *pToken) override;
  HRESULT STDMETHODCALLTYPE
  SetClipStatus(const D3DCLIPSTATUS8 *clip_status) override;
  HRESULT STDMETHODCALLTYPE GetClipStatus(D3DCLIPSTATUS8 *pClipStatus) override;
  HRESULT STDMETHODCALLTYPE
  GetTexture(DWORD Stage, IDirect3DBaseTexture8 **ppTexture) override;
  HRESULT STDMETHODCALLTYPE
  SetTexture(DWORD Stage, IDirect3DBaseTexture8 *pTexture) override;
  HRESULT STDMETHODCALLTYPE GetTextureStageState(DWORD Stage,
                                                 D3DTEXTURESTAGESTATETYPE Type,
                                                 DWORD *pValue) override;
  HRESULT STDMETHODCALLTYPE SetTextureStageState(DWORD Stage,
                                                 D3DTEXTURESTAGESTATETYPE Type,
                                                 DWORD Value) override;
  HRESULT STDMETHODCALLTYPE ValidateDevice(DWORD *pNumPasses) override;
  HRESULT STDMETHODCALLTYPE GetInfo(DWORD DevInfoID, void *pDevInfoStruct,
                                    DWORD DevInfoStructSize) override;
  HRESULT STDMETHODCALLTYPE
  SetPaletteEntries(UINT palette_idx, const PALETTEENTRY *entries) override;
  HRESULT STDMETHODCALLTYPE GetPaletteEntries(UINT PaletteNumber,
                                              PALETTEENTRY *pEntries) override;
  HRESULT STDMETHODCALLTYPE
  SetCurrentTexturePalette(UINT PaletteNumber) override;
  HRESULT STDMETHODCALLTYPE
  GetCurrentTexturePalette(UINT *PaletteNumber) override;
  HRESULT STDMETHODCALLTYPE DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType,
                                          UINT StartVertex,
                                          UINT PrimitiveCount) override;
  HRESULT STDMETHODCALLTYPE DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType,
                                                 UINT minIndex,
                                                 UINT NumVertices,
                                                 UINT startIndex,
                                                 UINT primCount) override;
  HRESULT STDMETHODCALLTYPE DrawPrimitiveUP(D3DPRIMITIVETYPE primitive_type,
                                            UINT primitive_count,
                                            const void *data,
                                            UINT stride) override;
  HRESULT STDMETHODCALLTYPE DrawIndexedPrimitiveUP(
      D3DPRIMITIVETYPE primitive_type, UINT min_vertex_idx, UINT vertex_count,
      UINT primitive_count, const void *index_data, D3DFORMAT index_format,
      const void *data, UINT stride) override;
  HRESULT STDMETHODCALLTYPE ProcessVertices(UINT SrcStartIndex, UINT DestIndex,
                                            UINT VertexCount,
                                            IDirect3DVertexBuffer8 *pDestBuffer,
                                            DWORD Flags) override;
  HRESULT STDMETHODCALLTYPE CreateVertexShader(const DWORD *declaration,
                                               const DWORD *byte_code,
                                               DWORD *shader,
                                               DWORD usage) override;
  HRESULT STDMETHODCALLTYPE SetVertexShader(DWORD Handle) override;
  HRESULT STDMETHODCALLTYPE GetVertexShader(DWORD *pHandle) override;
  HRESULT STDMETHODCALLTYPE DeleteVertexShader(DWORD Handle) override;
  HRESULT STDMETHODCALLTYPE SetVertexShaderConstant(DWORD reg_idx,
                                                    const void *data,
                                                    DWORD count) override;
  HRESULT STDMETHODCALLTYPE GetVertexShaderConstant(
      DWORD Register, void *pConstantData, DWORD ConstantCount) override;
  HRESULT STDMETHODCALLTYPE GetVertexShaderDeclaration(
      DWORD Handle, void *pData, DWORD *pSizeOfData) override;
  HRESULT STDMETHODCALLTYPE GetVertexShaderFunction(
      DWORD Handle, void *pData, DWORD *pSizeOfData) override;
  HRESULT STDMETHODCALLTYPE SetStreamSource(UINT StreamNumber,
                                            IDirect3DVertexBuffer8 *pStreamData,
                                            UINT Stride) override;
  HRESULT STDMETHODCALLTYPE
  GetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer8 **ppStreamData,
                  UINT *pStride) override;
  HRESULT STDMETHODCALLTYPE SetIndices(IDirect3DIndexBuffer8 *pIndexData,
                                       UINT BaseVertexIndex) override;
  HRESULT STDMETHODCALLTYPE GetIndices(IDirect3DIndexBuffer8 **ppIndexData,
                                       UINT *pBaseVertexIndex) override;
  HRESULT STDMETHODCALLTYPE CreatePixelShader(const DWORD *byte_code,
                                              DWORD *shader) override;
  HRESULT STDMETHODCALLTYPE SetPixelShader(DWORD Handle) override;
  HRESULT STDMETHODCALLTYPE GetPixelShader(DWORD *pHandle) override;
  HRESULT STDMETHODCALLTYPE DeletePixelShader(DWORD Handle) override;
  HRESULT STDMETHODCALLTYPE SetPixelShaderConstant(DWORD reg_idx,
                                                   const void *data,
                                                   DWORD count) override;
  HRESULT STDMETHODCALLTYPE GetPixelShaderConstant(
      DWORD Register, void *pConstantData, DWORD ConstantCount) override;
  HRESULT STDMETHODCALLTYPE GetPixelShaderFunction(DWORD Handle, void *pData,
                                                   DWORD *pSizeOfData) override;
  HRESULT STDMETHODCALLTYPE
  DrawRectPatch(UINT handle, const float *segment_count,
                const D3DRECTPATCH_INFO *patch_info) override;
  HRESULT STDMETHODCALLTYPE
  DrawTriPatch(UINT handle, const float *segment_count,
               const D3DTRIPATCH_INFO *patch_info) override;
  HRESULT STDMETHODCALLTYPE DeletePatch(UINT Handle) override;
};
class Texture8 final : public IDirect3DTexture8 {
public:
  Resource resource;
  UINT width, height;
  DWORD usage, lod = 0;
  D3DFORMAT format;
  D3DPOOL pool;
  std::vector<MipUpload> mips;
  std::vector<bool> locked, readonly;
  std::vector<Surface8 *> surfaces;
  std::shared_ptr<Texture> gpu;
  std::unique_ptr<CompressedTextureHandle> compressed;
  bool normal_xy = false;
  std::string asset;
  std::vector<std::uint64_t> expected_hashes;
  std::uint64_t bc_size = 0;
  Texture8(Device *, UINT, UINT, UINT, DWORD, D3DFORMAT, D3DPOOL);
  ~Texture8();
  HRESULT initialize();
  HRESULT upload(UINT);

  // IDirect3DTexture8 public SDK declarations.
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release() override;
  HRESULT STDMETHODCALLTYPE
  GetDevice(struct IDirect3DDevice8 **ppDevice) override;
  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid, const void *data,
                                           DWORD data_size,
                                           DWORD flags) override;
  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid, void *pData,
                                           DWORD *pSizeOfData) override;
  HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid) override;
  DWORD STDMETHODCALLTYPE SetPriority(DWORD PriorityNew) override;
  DWORD STDMETHODCALLTYPE GetPriority() override;
  void STDMETHODCALLTYPE PreLoad() override;
  D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override;
  DWORD STDMETHODCALLTYPE SetLOD(DWORD LODNew) override;
  DWORD STDMETHODCALLTYPE GetLOD() override;
  DWORD STDMETHODCALLTYPE GetLevelCount() override;
  HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT Level,
                                         D3DSURFACE_DESC *pDesc) override;
  HRESULT STDMETHODCALLTYPE
  GetSurfaceLevel(UINT Level, IDirect3DSurface8 **ppSurfaceLevel) override;
  HRESULT STDMETHODCALLTYPE LockRect(UINT level, D3DLOCKED_RECT *locked_rect,
                                     const RECT *rect, DWORD flags) override;
  HRESULT STDMETHODCALLTYPE UnlockRect(UINT Level) override;
  HRESULT STDMETHODCALLTYPE AddDirtyRect(const RECT *dirty_rect) override;
};
class Surface8 final : public IDirect3DSurface8 {
public:
  Resource resource;
  Texture8 *texture = nullptr;
  UINT level = 0;
  D3DSURFACE_DESC desc{};
  std::shared_ptr<Target> target;
  std::vector<BYTE> bytes;
  bool locked = false, readonly = false;
  bool implicit_surface = false;
  Surface8(Device *, const D3DSURFACE_DESC &, bool implicit = false);
  ~Surface8();

  // IDirect3DSurface8 public SDK declarations.
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release() override;
  HRESULT STDMETHODCALLTYPE
  GetDevice(struct IDirect3DDevice8 **ppDevice) override;
  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid, const void *data,
                                           DWORD data_size,
                                           DWORD flags) override;
  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid, void *pData,
                                           DWORD *pSizeOfData) override;
  HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid) override;
  HRESULT STDMETHODCALLTYPE GetContainer(REFIID riid,
                                         void **ppContainer) override;
  HRESULT STDMETHODCALLTYPE GetDesc(D3DSURFACE_DESC *pDesc) override;
  HRESULT STDMETHODCALLTYPE LockRect(D3DLOCKED_RECT *locked_rect,
                                     const RECT *rect, DWORD flags) override;
  HRESULT STDMETHODCALLTYPE UnlockRect() override;
};
class VertexBuffer8 final : public IDirect3DVertexBuffer8 {
public:
  Resource resource;
  D3DVERTEXBUFFER_DESC desc{};
  std::vector<BYTE> bytes;
  bool locked = false;
  VertexBuffer8(Device *, UINT, DWORD, DWORD, D3DPOOL);
  ~VertexBuffer8() = default;

  // IDirect3DVertexBuffer8 public SDK declarations.
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release() override;
  HRESULT STDMETHODCALLTYPE
  GetDevice(struct IDirect3DDevice8 **ppDevice) override;
  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid, const void *data,
                                           DWORD data_size,
                                           DWORD flags) override;
  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid, void *pData,
                                           DWORD *pSizeOfData) override;
  HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid) override;
  DWORD STDMETHODCALLTYPE SetPriority(DWORD PriorityNew) override;
  DWORD STDMETHODCALLTYPE GetPriority() override;
  void STDMETHODCALLTYPE PreLoad() override;
  D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override;
  HRESULT STDMETHODCALLTYPE Lock(UINT OffsetToLock, UINT SizeToLock,
                                 BYTE **ppbData, DWORD Flags) override;
  HRESULT STDMETHODCALLTYPE Unlock() override;
  HRESULT STDMETHODCALLTYPE GetDesc(D3DVERTEXBUFFER_DESC *pDesc) override;
};
class IndexBuffer8 final : public IDirect3DIndexBuffer8 {
public:
  Resource resource;
  D3DINDEXBUFFER_DESC desc{};
  std::vector<BYTE> bytes;
  bool locked = false;
  IndexBuffer8(Device *, UINT, DWORD, D3DFORMAT, D3DPOOL);
  ~IndexBuffer8() = default;

  // IDirect3DIndexBuffer8 public SDK declarations.
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release() override;
  HRESULT STDMETHODCALLTYPE
  GetDevice(struct IDirect3DDevice8 **ppDevice) override;
  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID refguid, const void *data,
                                           DWORD data_size,
                                           DWORD flags) override;
  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID refguid, void *pData,
                                           DWORD *pSizeOfData) override;
  HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID refguid) override;
  DWORD STDMETHODCALLTYPE SetPriority(DWORD PriorityNew) override;
  DWORD STDMETHODCALLTYPE GetPriority() override;
  void STDMETHODCALLTYPE PreLoad() override;
  D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override;
  HRESULT STDMETHODCALLTYPE Lock(UINT OffsetToLock, UINT SizeToLock,
                                 BYTE **ppbData, DWORD Flags) override;
  HRESULT STDMETHODCALLTYPE Unlock() override;
  HRESULT STDMETHODCALLTYPE GetDesc(D3DINDEXBUFFER_DESC *pDesc) override;
};
UINT pixel_pitch(D3DFORMAT, UINT);
UINT pixel_rows(D3DFORMAT, UINT);
bool block_format(D3DFORMAT);
void identity(Matrix &);
void multiply(Matrix &, const Matrix &, const Matrix &);
} // namespace a2fo::renderer::game
