CXX := i686-w64-mingw32-g++
CXX_HOST ?= g++
OBJDUMP := i686-w64-mingw32-objdump

LUA_DIR := third_party/lua-5.4.8/src

.DEFAULT_GOAL := release

CPPFLAGS := -DWIN32_LEAN_AND_MEAN -DLUA_USE_JUMPTABLE=0 -I$(LUA_DIR)
CXXFLAGS := -std=gnu++17 -O2 -Wall -Wextra -Wpedantic -DNDEBUG
DLLFLAGS := -shared -static -static-libgcc -static-libstdc++ \
	-Wl,--enable-stdcall-fixup

BUILD_DIR := build
MODULE_DIR := $(BUILD_DIR)/modules
SMOKE_TEST := $(BUILD_DIR)/dll_load_smoke.exe
FPQ_PATHS_TEST := $(BUILD_DIR)/fpq_paths_test
ODF_PATHS_TEST := $(BUILD_DIR)/odf_paths_test
ODF_MODULE_SMOKE := $(BUILD_DIR)/odf_module_init_smoke.exe
EXTENSION_ROOTS_TEST := $(BUILD_DIR)/extension_roots_test
EXTENSION_ROOT_SMOKE := $(BUILD_DIR)/extension_root_discovery_smoke.exe
LUA_HOST_SMOKE := $(BUILD_DIR)/lua_host_smoke.exe
MODULE_API_TEST := $(BUILD_DIR)/module_api_test

LUA_SOURCES := \
	$(LUA_DIR)/lapi.c \
	$(LUA_DIR)/lcode.c \
	$(LUA_DIR)/lctype.c \
	$(LUA_DIR)/ldebug.c \
	$(LUA_DIR)/ldo.c \
	$(LUA_DIR)/ldump.c \
	$(LUA_DIR)/lfunc.c \
	$(LUA_DIR)/lgc.c \
	$(LUA_DIR)/llex.c \
	$(LUA_DIR)/lmem.c \
	$(LUA_DIR)/lobject.c \
	$(LUA_DIR)/lopcodes.c \
	$(LUA_DIR)/lparser.c \
	$(LUA_DIR)/lstate.c \
	$(LUA_DIR)/lstring.c \
	$(LUA_DIR)/ltable.c \
	$(LUA_DIR)/ltm.c \
	$(LUA_DIR)/lundump.c \
	$(LUA_DIR)/lvm.c \
	$(LUA_DIR)/lzio.c \
	$(LUA_DIR)/lauxlib.c \
	$(LUA_DIR)/lbaselib.c \
	$(LUA_DIR)/lmathlib.c \
	$(LUA_DIR)/lstrlib.c \
	$(LUA_DIR)/ltablib.c \
	$(LUA_DIR)/lutf8lib.c

CORE_SOURCES := \
	core/dllmain.cpp \
	core/extension_roots.cpp \
	core/lua_host.cpp \
	core/module_loader.cpp \
	core/hook.cpp \
	core/delphi_bridge.S \
	$(LUA_SOURCES)

.PHONY: all release sdk-examples clean verify verify-sdk test smoke \
	odf-module-smoke extension-root-smoke lua-host-smoke

all: release

release: \
	$(BUILD_DIR)/A2FOExtensions.dll \
	$(BUILD_DIR)/Win2kDisableTaskSwitch.dll \
	$(MODULE_DIR)/A2FOFeaturePack.dll

sdk-examples: $(MODULE_DIR)/ExampleModule.dll

$(BUILD_DIR):
	mkdir -p $@

$(MODULE_DIR):
	mkdir -p $@

$(BUILD_DIR)/A2FOExtensions.dll: $(CORE_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ $(CORE_SOURCES)

$(BUILD_DIR)/Win2kDisableTaskSwitch.dll: \
		core/startup_proxy.cpp core/startup_proxy.def | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ core/startup_proxy.cpp core/startup_proxy.def

$(MODULE_DIR)/ExampleModule.dll: \
		sdk/examples/ExampleModule/example_module.cpp \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-Isdk/include \
		-o $@ sdk/examples/ExampleModule/example_module.cpp

$(MODULE_DIR)/A2FOFeaturePack.dll: \
		modules/A2FOFeaturePack/odf_recursive.cpp \
		modules/A2FOFeaturePack/queue_enhancement.cpp \
		modules/A2FOFeaturePack/queue_enhancement.hpp \
		modules/A2FOFeaturePack/upgrade_pods.cpp \
		modules/A2FOFeaturePack/upgrade_pods.hpp \
		modules/A2FOFeaturePack/delphi_bridge.S \
		core/fpq_paths.cpp core/fpq_paths.hpp \
		core/odf_paths.cpp core/odf_paths.hpp \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ \
		modules/A2FOFeaturePack/odf_recursive.cpp \
		modules/A2FOFeaturePack/queue_enhancement.cpp \
		modules/A2FOFeaturePack/upgrade_pods.cpp \
		modules/A2FOFeaturePack/delphi_bridge.S \
		core/fpq_paths.cpp core/odf_paths.cpp

$(SMOKE_TEST): tests/dll_load_smoke.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -static -static-libgcc -static-libstdc++ \
		-o $@ $<

$(FPQ_PATHS_TEST): tests/fpq_paths_test.cpp \
		core/fpq_paths.cpp core/fpq_paths.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Icore \
		-o $@ tests/fpq_paths_test.cpp core/fpq_paths.cpp

$(ODF_PATHS_TEST): tests/odf_paths_test.cpp \
		core/odf_paths.cpp core/odf_paths.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Icore \
		-o $@ tests/odf_paths_test.cpp core/odf_paths.cpp

$(EXTENSION_ROOTS_TEST): tests/extension_roots_test.cpp \
		core/extension_roots.cpp core/extension_roots.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Icore \
		-o $@ tests/extension_roots_test.cpp core/extension_roots.cpp

$(EXTENSION_ROOT_SMOKE): tests/extension_root_discovery_smoke.cpp \
		core/extension_roots.cpp core/extension_roots.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -static -static-libgcc -static-libstdc++ -Icore \
		-o $@ tests/extension_root_discovery_smoke.cpp \
		core/extension_roots.cpp

$(LUA_HOST_SMOKE): tests/lua_host_smoke.cpp core/lua_host.cpp \
		core/lua_host.hpp $(LUA_SOURCES) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -static -static-libgcc -static-libstdc++ \
		-Icore -o $@ tests/lua_host_smoke.cpp core/lua_host.cpp \
		$(LUA_SOURCES)

$(ODF_MODULE_SMOKE): tests/odf_module_init_smoke.cpp \
		sdk/include/a2fo_module_api.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -static -static-libgcc -static-libstdc++ \
		-Isdk/include -o $@ $<

$(MODULE_API_TEST): tests/module_api_test.cpp \
		sdk/include/a2fo_module_api.h | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Isdk/include -o $@ $<

verify: release
	@echo "A2FOExtensions exports:"
	@$(OBJDUMP) -p $(BUILD_DIR)/A2FOExtensions.dll | \
		grep -E "A2FO_Initialize|DLL Name" || true
	@echo
	@echo "Proxy exports:"
	@$(OBJDUMP) -p $(BUILD_DIR)/Win2kDisableTaskSwitch.dll | \
		grep -E "LowLevelKeyboardProc|SetHookID|DLL Name" || true
	@echo
	@echo "A2FOFeaturePack module exports:"
	@$(OBJDUMP) -p $(MODULE_DIR)/A2FOFeaturePack.dll | \
		grep -E "A2FO_ModuleInit|DLL Name" || true
	@echo
	@echo "Checking for non-system MinGW runtime dependencies:"
	@for dll in \
		$(BUILD_DIR)/A2FOExtensions.dll \
		$(BUILD_DIR)/Win2kDisableTaskSwitch.dll \
		$(MODULE_DIR)/A2FOFeaturePack.dll; do \
		if $(OBJDUMP) -p "$$dll" | \
			grep -Eiq 'DLL Name: (libgcc|libstdc\+\+|libwinpthread)'; then \
			echo "Unexpected MinGW runtime dependency in $$dll" >&2; \
			$(OBJDUMP) -p "$$dll" | grep -Ei 'DLL Name:' >&2; \
			exit 1; \
		fi; \
	done
	@echo "No external MinGW runtime DLLs required."

verify-sdk: sdk-examples
	@$(OBJDUMP) -p $(MODULE_DIR)/ExampleModule.dll | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true

test: $(FPQ_PATHS_TEST) $(ODF_PATHS_TEST) $(EXTENSION_ROOTS_TEST) \
	$(MODULE_API_TEST)
	$(FPQ_PATHS_TEST)
	$(ODF_PATHS_TEST)
	$(EXTENSION_ROOTS_TEST)
	$(MODULE_API_TEST)

smoke: release $(SMOKE_TEST)
	cd $(BUILD_DIR) && wine dll_load_smoke.exe

odf-module-smoke: release $(ODF_MODULE_SMOKE)
	@test -f $(BUILD_DIR)/FleetOpsHook.fixture.dll || \
		(echo "Copy the supported FleetOpsHook.dll to build/FleetOpsHook.fixture.dll" >&2; exit 1)
	cd $(BUILD_DIR) && wine odf_module_init_smoke.exe

extension-root-smoke: $(EXTENSION_ROOT_SMOKE)
	@test -n "$(A2FO_TEST_DATA_ROOT)" || \
		(echo "Set A2FO_TEST_DATA_ROOT to the Fleet Ops Data path" >&2; exit 1)
	cd $(BUILD_DIR) && wine extension_root_discovery_smoke.exe \
		"$(A2FO_TEST_DATA_ROOT)" "$(A2FO_TEST_ACTIVE_MOD)" \
		"$(A2FO_TEST_PARENT_MOD)"

lua-host-smoke: $(LUA_HOST_SMOKE)
	cd $(BUILD_DIR) && wine lua_host_smoke.exe

clean:
	rm -rf $(BUILD_DIR)
