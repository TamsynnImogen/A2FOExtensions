// Synthetic PE32 and COM fixtures only. No game, window or graphics device.
#include "../renderer/d3d8_compat/observer.cpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace a2fo::renderer {
HRESULT run_d3d11_window_probe(HWND, UINT, UINT, bool,
                               void (*)(const std::string &)) {
  std::abort(); // This fixture must never reach the graphics probe.
}
} // namespace a2fo::renderer

namespace {
using namespace a2fo::renderer;

struct FakeComObject {
  void **vtable;
  ULONG references = 1;
};

void *native_factory_vtable[16]{};
FakeComObject native_factory{native_factory_vtable};
unsigned native_factory_calls = 0;
unsigned native_device_calls = 0;

void require(bool condition, const char *message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

HRESULT STDMETHODCALLTYPE query_interface(FakeComObject *self, REFIID,
                                           void **result) {
  if (!result)
    return E_POINTER;
  *result = self;
  ++self->references;
  return S_OK;
}

ULONG STDMETHODCALLTYPE add_ref(FakeComObject *self) {
  return ++self->references;
}

ULONG STDMETHODCALLTYPE release(FakeComObject *self) {
  return --self->references;
}

HRESULT STDMETHODCALLTYPE native_create_device(
    IDirect3D8 *, UINT, D3DDEVTYPE, HWND, DWORD, D3DPRESENT_PARAMETERS *,
    IDirect3DDevice8 **) {
  ++native_device_calls;
  return E_FAIL;
}

IDirect3D8 *WINAPI native_factory_entry(UINT sdk) {
  require(sdk == D3D_SDK_VERSION, "D3D8 SDK version forwarded");
  ++native_factory_calls;
  return reinterpret_cast<IDirect3D8 *>(&native_factory);
}

std::string child_path(const std::string &root) {
  return root + "\\Mods\\Fixture DX11 Child";
}

void ensure_directory(const std::string &path) {
  if (CreateDirectoryA(path.c_str(), nullptr))
    return;
  const DWORD error = GetLastError();
  const DWORD attributes = GetFileAttributesA(path.c_str());
  require(error == ERROR_ALREADY_EXISTS &&
              attributes != INVALID_FILE_ATTRIBUTES &&
              (attributes & FILE_ATTRIBUTE_DIRECTORY),
          "create isolated fixture directory");
}

void write_setting(const std::string &path, const char *key,
                   const char *value) {
  require(WritePrivateProfileStringA("D3D8Compat", key, value,
                                     path.c_str()) != FALSE,
          "write fixture INI setting");
}

HMODULE synthetic_image() {
  auto *data = static_cast<BYTE *>(VirtualAlloc(
      nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  require(data != nullptr, "allocate synthetic PE32 image");

  auto *dos = reinterpret_cast<IMAGE_DOS_HEADER *>(data);
  dos->e_magic = IMAGE_DOS_SIGNATURE;
  dos->e_lfanew = 128;
  auto *nt = reinterpret_cast<IMAGE_NT_HEADERS32 *>(data + 128);
  nt->Signature = IMAGE_NT_SIGNATURE;
  nt->FileHeader.Machine = IMAGE_FILE_MACHINE_I386;
  nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
  nt->OptionalHeader.SizeOfImage = 4096;
  nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT] = {512, 40};

  auto *imports = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR *>(data + 512);
  imports[0].Name = 600;
  imports[0].OriginalFirstThunk = 640;
  imports[0].FirstThunk = 680;
  std::memcpy(data + 600, "d3d8.dll", sizeof("d3d8.dll"));

  auto *names = reinterpret_cast<IMAGE_THUNK_DATA32 *>(data + 640);
  names[0].u1.AddressOfData = 720;
  auto *slots = reinterpret_cast<IMAGE_THUNK_DATA32 *>(data + 680);
  slots[0].u1.Function = static_cast<DWORD>(reinterpret_cast<std::uintptr_t>(
      address(&native_factory_entry)));
  std::memcpy(data + 722, "Direct3DCreate8", sizeof("Direct3DCreate8"));
  return reinterpret_cast<HMODULE>(data);
}

void initialize_native_factory() {
  native_factory_vtable[0] = address(&query_interface);
  native_factory_vtable[1] = address(&add_ref);
  native_factory_vtable[2] = address(&release);
  native_factory_vtable[15] = address(&native_create_device);
  native_factory.references = 1;
  native_factory_calls = 0;
  native_device_calls = 0;
}

void *factory_import_slot(HMODULE image) {
  auto *base = reinterpret_cast<BYTE *>(image);
  auto *imports = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR *>(base + 512);
  require(imports[0].Name != 0, "synthetic D3D8 import exists");
  auto *slots = reinterpret_cast<void **>(base + imports[0].FirstThunk);
  return slots;
}

void prepare_files(const std::string &root, const std::string &test_case) {
  ensure_directory(root);
  ensure_directory(root + "\\Mods");
  const std::string child = child_path(root);
  ensure_directory(child);

  const std::string global_ini = root + "\\A2FORenderer.ini";
  write_setting(global_ini, "Backend", "d3d8");
  write_setting(global_ini, "Trace", "0");
  const std::string child_ini = child + "\\A2FORenderer.ini";
  if (test_case == "normal") {
    require(GetFileAttributesA(child_ini.c_str()) == INVALID_FILE_ATTRIBUTES,
            "normal case requires no child renderer profile");
  } else {
    write_setting(child_ini, "Backend", "d3d11-test");
    write_setting(child_ini, "BCTextures",
                  test_case == "failclosed" ? "1" : "0");
    write_setting(child_ini, "Trace", "0");
  }
}

} // namespace

int main(int argc, char **argv) {
  require(argc == 5, "arguments: OUTPUT CASE /mod \"Fixture DX11 Child\"");
  const std::string root = argv[1];
  const std::string test_case = argv[2];
  require(std::strcmp(argv[3], "/mod") == 0 &&
              std::strcmp(argv[4], "Fixture DX11 Child") == 0,
          "fixture is launched with the expected active child");
  require(test_case == "normal" || test_case == "baseline" ||
              test_case == "failclosed",
          "known activation case");

  prepare_files(root, test_case);
  require(a2fo::active_mod_from_command_line(GetCommandLineA()) ==
              "Fixture DX11 Child",
          "active child parsed from /mod command line");
  initialize_native_factory();
  HMODULE image = synthetic_image();
  auto **factory_slot = static_cast<void **>(factory_import_slot(image));
  const void *original_factory_slot = *factory_slot;
  const bool installed = install_d3d8_observer(image, nullptr, root, nullptr);

  if (test_case == "normal") {
    require(installed && runtime == nullptr,
            "normal FO4 route allocates no observer runtime");
    require(!d3d11_game_test_requested(),
            "normal FO4 route does not request child rendering");
    require(*factory_slot == original_factory_slot,
            "normal FO4 route leaves factory IAT unchanged");
    const auto entry = function_at<Factory>(*factory_slot);
    IDirect3D8 *native = entry(D3D_SDK_VERSION);
    require(native == reinterpret_cast<IDirect3D8 *>(&native_factory) &&
                native_factory_calls == 1,
            "normal import still calls the native D3D8 factory");
    require(native->Release() == 0 && native_factory.references == 0,
            "normal native factory reference count released");
  } else {
    require(runtime && runtime->game_test && d3d11_game_test_requested(),
            "child profile selects the experimental D3D11 route");
    require(runtime->game_configuration.bc_assets ==
                (test_case == "failclosed"),
            "child BCTextures setting is applied");
    const auto hooked_factory = address(&factory_hook);
    require(*factory_slot == hooked_factory,
            "child route patches the named Direct3DCreate8 import");
    const auto entry = function_at<Factory>(*factory_slot);

    if (test_case == "baseline") {
      require(installed && runtime->game_ready,
              "baseline child without BC textures activates");
      IDirect3D8 *game_factory = entry(D3D_SDK_VERSION);
      require(game_factory &&
                  game_factory != reinterpret_cast<IDirect3D8 *>(
                                      &native_factory),
              "child import returns the compatibility factory");
      require(native_factory_calls == 1 && native_factory.references == 1 &&
                  native_device_calls == 0,
              "child retains only the native query factory, no native device");
      require(game_factory->Release() == 0 &&
                  native_factory.references == 0,
              "compatibility factory releases its native query reference");
    } else {
      require(!installed && !runtime->game_ready,
              "BC child without named fopen import fails closed");
      IDirect3D8 *result = entry(D3D_SDK_VERSION);
      require(result == nullptr && native_factory_calls == 0 &&
                  native_factory.references == 1 && native_device_calls == 0,
              "fail-closed import does not call native factory or device");
    }
  }

  VirtualFree(image, 0, MEM_RELEASE);
  std::printf("PASS: D3D11 child activation case %s\n", test_case.c_str());
  return 0;
}
