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
STA1_CLASSIC_DIR := $(BUILD_DIR)/sta1-classic
STA1_CLASSIC_MODULE_DIR := $(STA1_CLASSIC_DIR)/modules
STA1_COMPAT_MODULE := $(MODULE_DIR)/A1Compat.dll
CHEATS_MODULE := $(MODULE_DIR)/A2FOCheats.dll
CRAFT_IDENTITY_MODULE := $(MODULE_DIR)/A2FOCraftIdentity.dll
FIRE_ARCS_MODULE := $(MODULE_DIR)/A2FOFireArcs.dll
NORMAL_WEAPON_TECH_MODULE := $(MODULE_DIR)/A2FONormalWeaponTech.dll
TURRETS_MODULE := $(MODULE_DIR)/A2FOTurrets.dll
STA1_CLASSIC_GUI_CFG := \
	mods/STA1Classic/misc/gui_fed.cfg \
	mods/STA1Classic/misc/gui_bor.cfg \
	mods/STA1Classic/misc/gui_kli.cfg \
	mods/STA1Classic/misc/gui_rom.cfg
STA1_CLASSIC_SPRITE_REGISTRY := mods/STA1Classic/Sprites/sprites.spr
SMOKE_TEST := $(BUILD_DIR)/dll_load_smoke.exe
FPQ_PATHS_TEST := $(BUILD_DIR)/fpq_paths_test
ODF_PATHS_TEST := $(BUILD_DIR)/odf_paths_test
ODF_MODULE_SMOKE := $(BUILD_DIR)/odf_module_init_smoke.exe
EXTENSION_ROOTS_TEST := $(BUILD_DIR)/extension_roots_test
EXTENSION_ROOT_SMOKE := $(BUILD_DIR)/extension_root_discovery_smoke.exe
LUA_HOST_SMOKE := $(BUILD_DIR)/lua_host_smoke.exe
MODULE_API_TEST := $(BUILD_DIR)/module_api_test
HYBRID_PRODUCTION_TEST := $(BUILD_DIR)/hybrid_production_test
TURRET_MATH_TEST := $(BUILD_DIR)/turret_math_test
CRAFT_IDENTITY_TEST := $(BUILD_DIR)/craft_identity_test
FIRE_ARC_TEST := $(BUILD_DIR)/fire_arc_test

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

.PHONY: all release sta1-classic verify-sta1-classic sdk-examples clean \
	verify verify-sdk test smoke odf-module-smoke extension-root-smoke \
	lua-host-smoke

all: release

release: \
	$(BUILD_DIR)/A2FOExtensions.dll \
	$(BUILD_DIR)/Win2kDisableTaskSwitch.dll \
	$(MODULE_DIR)/A2FOFeaturePack.dll \
	$(MODULE_DIR)/A2FOHybridBuild.dll \
	$(MODULE_DIR)/A2FOInfoIni.dll \
	$(CHEATS_MODULE) \
	$(CRAFT_IDENTITY_MODULE) \
	$(FIRE_ARCS_MODULE) \
	$(NORMAL_WEAPON_TECH_MODULE) \
	$(TURRETS_MODULE) \
	$(MODULE_DIR)/A2FORGBTextures.dll

sta1-classic: release $(STA1_CLASSIC_MODULE_DIR) $(STA1_COMPAT_MODULE) $(STA1_CLASSIC_GUI_CFG) \
		$(STA1_CLASSIC_SPRITE_REGISTRY)
	mkdir -p $(STA1_CLASSIC_DIR)/AI $(STA1_CLASSIC_DIR)/bzn \
		$(STA1_CLASSIC_DIR)/misc $(STA1_CLASSIC_DIR)/odf \
		$(STA1_CLASSIC_DIR)/sod $(STA1_CLASSIC_DIR)/sounds \
		$(STA1_CLASSIC_DIR)/sprites $(STA1_CLASSIC_DIR)/techtree \
		$(STA1_CLASSIC_DIR)/textures
	cp mods/STA1Classic/info.ini $(STA1_CLASSIC_DIR)/info.ini
	cp mods/STA1Classic/a1compat.ini $(STA1_CLASSIC_DIR)/a1compat.ini
	cp mods/STA1Classic/README.md $(STA1_CLASSIC_DIR)/README.md
	cp $(STA1_CLASSIC_GUI_CFG) $(STA1_CLASSIC_DIR)/misc/
	cp $(STA1_CLASSIC_SPRITE_REGISTRY) $(STA1_CLASSIC_DIR)/sprites/
	cp $(STA1_COMPAT_MODULE) \
		$(STA1_CLASSIC_MODULE_DIR)/A1Compat.dll
	cp $(MODULE_DIR)/A2FORGBTextures.dll \
		$(STA1_CLASSIC_MODULE_DIR)/A2FORGBTextures.dll

sdk-examples: $(MODULE_DIR)/ExampleModule.dll

$(BUILD_DIR):
	mkdir -p $@

$(MODULE_DIR):
	mkdir -p $@

$(STA1_CLASSIC_MODULE_DIR):
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
		modules/A2FOFeaturePack/bink_video.cpp \
		modules/A2FOFeaturePack/bink_video.hpp \
		modules/A2FOFeaturePack/hybrid_bridge_api.hpp \
		modules/A2FOFeaturePack/hybrid_bridge_client.cpp \
		modules/A2FOFeaturePack/hybrid_bridge_client.hpp \
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
		modules/A2FOFeaturePack/bink_video.cpp \
		modules/A2FOFeaturePack/hybrid_bridge_client.cpp \
		modules/A2FOFeaturePack/queue_enhancement.cpp \
		modules/A2FOFeaturePack/upgrade_pods.cpp \
		modules/A2FOFeaturePack/delphi_bridge.S \
		core/fpq_paths.cpp core/odf_paths.cpp \
		-lgdi32

$(MODULE_DIR)/A2FOHybridBuild.dll: \
		modules/A2FOHybridBuild/module.cpp \
		modules/A2FOHybridBuild/delphi_bridge.S \
		modules/A2FOFeaturePack/hybrid_bridge_api.hpp \
		modules/A2FOHybridBuild/hybrid_production.cpp \
		modules/A2FOHybridBuild/hybrid_production.hpp \
		modules/A2FOHybridBuild/hybrid_production_runtime.cpp \
		modules/A2FOHybridBuild/hybrid_production_runtime.hpp \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ \
		modules/A2FOHybridBuild/module.cpp \
		modules/A2FOHybridBuild/hybrid_production.cpp \
		modules/A2FOHybridBuild/hybrid_production_runtime.cpp \
		modules/A2FOHybridBuild/delphi_bridge.S

$(MODULE_DIR)/A2FOInfoIni.dll: \
		modules/A2FOInfoIni/module.cpp \
		core/extension_roots.cpp core/extension_roots.hpp \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOInfoIni/module.cpp core/extension_roots.cpp

$(CHEATS_MODULE): \
		modules/A2FOCheats/module.cpp \
		modules/A2FOCheats/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOCheats/module.cpp \
		modules/A2FOCheats/thiscall_bridge.S

$(CRAFT_IDENTITY_MODULE): \
		modules/A2FOCraftIdentity/module.cpp \
		modules/A2FOCraftIdentity/identity_selection.cpp \
		modules/A2FOCraftIdentity/identity_selection.hpp \
		modules/A2FOCraftIdentity/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOCraftIdentity/module.cpp \
		modules/A2FOCraftIdentity/identity_selection.cpp \
		modules/A2FOCraftIdentity/thiscall_bridge.S

$(FIRE_ARCS_MODULE): \
		modules/A2FOFireArcs/module.cpp \
		modules/A2FOFireArcs/fire_arc.cpp \
		modules/A2FOFireArcs/fire_arc.hpp \
		modules/A2FOFireArcs/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOFireArcs/module.cpp \
		modules/A2FOFireArcs/fire_arc.cpp \
		modules/A2FOFireArcs/thiscall_bridge.S

$(NORMAL_WEAPON_TECH_MODULE): \
		modules/A2FONormalWeaponTech/module.cpp \
		modules/A2FONormalWeaponTech/delphi_bridge.S \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FONormalWeaponTech/module.cpp \
		modules/A2FONormalWeaponTech/delphi_bridge.S

$(TURRETS_MODULE): \
		modules/A2FOTurrets/module.cpp \
		modules/A2FOTurrets/turret_math.cpp \
		modules/A2FOTurrets/turret_math.hpp \
		modules/A2FOTurrets/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOTurrets/module.cpp \
		modules/A2FOTurrets/turret_math.cpp \
		modules/A2FOTurrets/thiscall_bridge.S

$(MODULE_DIR)/A2FORGBTextures.dll: \
		modules/A2FORGBTextures/module.cpp \
		modules/A2FORGBTextures/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FORGBTextures/module.cpp \
		modules/A2FORGBTextures/thiscall_bridge.S

$(STA1_COMPAT_MODULE): \
		modules/A1Compat/module.cpp \
		modules/A1Compat/thiscall_bridge.S \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A1Compat/module.cpp \
		modules/A1Compat/thiscall_bridge.S

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

$(HYBRID_PRODUCTION_TEST): tests/hybrid_production_test.cpp \
		modules/A2FOHybridBuild/hybrid_production.cpp \
		modules/A2FOHybridBuild/hybrid_production.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOHybridBuild -o $@ \
		tests/hybrid_production_test.cpp \
		modules/A2FOHybridBuild/hybrid_production.cpp

$(TURRET_MATH_TEST): tests/turret_math_test.cpp \
		modules/A2FOTurrets/turret_math.cpp \
		modules/A2FOTurrets/turret_math.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOTurrets -o $@ \
		tests/turret_math_test.cpp \
		modules/A2FOTurrets/turret_math.cpp

$(CRAFT_IDENTITY_TEST): tests/craft_identity_test.cpp \
		modules/A2FOCraftIdentity/identity_selection.cpp \
		modules/A2FOCraftIdentity/identity_selection.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOCraftIdentity -o $@ \
		tests/craft_identity_test.cpp \
		modules/A2FOCraftIdentity/identity_selection.cpp

$(FIRE_ARC_TEST): tests/fire_arc_test.cpp \
		modules/A2FOFireArcs/fire_arc.cpp \
		modules/A2FOFireArcs/fire_arc.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOFireArcs -o $@ \
		tests/fire_arc_test.cpp modules/A2FOFireArcs/fire_arc.cpp

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
	@echo "A2FOHybridBuild module exports:"
	@$(OBJDUMP) -p $(MODULE_DIR)/A2FOHybridBuild.dll | \
		grep -E "A2FO_ModuleInit|DLL Name" || true
	@echo
	@echo "A2FOInfoIni module exports:"
	@$(OBJDUMP) -p $(MODULE_DIR)/A2FOInfoIni.dll | \
		grep -E "A2FO_ModuleInit|DLL Name" || true
	@echo
	@echo "A2FOCheats module exports:"
	@$(OBJDUMP) -p $(CHEATS_MODULE) | \
		grep -E "A2FO_ModuleInit|DLL Name" || true
	@echo
	@echo "A2FOCraftIdentity module exports:"
	@$(OBJDUMP) -p $(CRAFT_IDENTITY_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true
	@echo
	@echo "A2FOFireArcs module exports:"
	@$(OBJDUMP) -p $(FIRE_ARCS_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|A2FOFireArcs_AllowWeaponTrigger|DLL Name" || true
	@echo
	@echo "A2FONormalWeaponTech module exports:"
	@$(OBJDUMP) -p $(NORMAL_WEAPON_TECH_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|A2FONormalWeaponTech_AllowWeaponTrigger|DLL Name" || true
	@echo
	@echo "A2FOTurrets module exports:"
	@$(OBJDUMP) -p $(TURRETS_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true
	@echo
	@echo "A2FORGBTextures module exports:"
	@$(OBJDUMP) -p $(MODULE_DIR)/A2FORGBTextures.dll | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true
	@echo
	@echo "Checking for non-system MinGW runtime dependencies:"
	@for dll in \
		$(BUILD_DIR)/A2FOExtensions.dll \
		$(BUILD_DIR)/Win2kDisableTaskSwitch.dll \
		$(MODULE_DIR)/A2FOFeaturePack.dll \
		$(MODULE_DIR)/A2FOHybridBuild.dll \
		$(MODULE_DIR)/A2FOInfoIni.dll \
		$(CHEATS_MODULE) \
		$(CRAFT_IDENTITY_MODULE) \
		$(FIRE_ARCS_MODULE) \
		$(NORMAL_WEAPON_TECH_MODULE) \
		$(TURRETS_MODULE) \
		$(MODULE_DIR)/A2FORGBTextures.dll; do \
		if $(OBJDUMP) -p "$$dll" | \
			grep -Eiq 'DLL Name: (libgcc|libstdc\+\+|libwinpthread)'; then \
			echo "Unexpected MinGW runtime dependency in $$dll" >&2; \
			$(OBJDUMP) -p "$$dll" | grep -Ei 'DLL Name:' >&2; \
			exit 1; \
		fi; \
	done
	@echo "No external MinGW runtime DLLs required."

verify-sta1-classic: sta1-classic
	@for cfg in gui_fed.cfg gui_bor.cfg gui_kli.cfg gui_rom.cfg; do \
		test -f "$(STA1_CLASSIC_DIR)/misc/$$cfg" || exit 1; \
	done
	@echo "A1Compat module exports:"
	@$(OBJDUMP) -p $(STA1_COMPAT_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true
	@if $(OBJDUMP) -p $(STA1_COMPAT_MODULE) | \
		grep -Eiq 'DLL Name: (libgcc|libstdc\+\+|libwinpthread)'; then \
		echo "Unexpected MinGW runtime dependency in $(STA1_COMPAT_MODULE)" >&2; \
		$(OBJDUMP) -p $(STA1_COMPAT_MODULE) | grep -Ei 'DLL Name:' >&2; \
		exit 1; \
	fi
	@echo "STA1 Classic module package is self-contained."

verify-sdk: sdk-examples
	@$(OBJDUMP) -p $(MODULE_DIR)/ExampleModule.dll | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true

test: $(FPQ_PATHS_TEST) $(ODF_PATHS_TEST) $(EXTENSION_ROOTS_TEST) \
	$(MODULE_API_TEST) $(HYBRID_PRODUCTION_TEST) $(TURRET_MATH_TEST) \
	$(CRAFT_IDENTITY_TEST) $(FIRE_ARC_TEST)
	$(FPQ_PATHS_TEST)
	$(ODF_PATHS_TEST)
	$(EXTENSION_ROOTS_TEST)
	$(MODULE_API_TEST)
	$(HYBRID_PRODUCTION_TEST)
	$(TURRET_MATH_TEST)
	$(CRAFT_IDENTITY_TEST)
	$(FIRE_ARC_TEST)

smoke: release $(SMOKE_TEST)
	cd $(BUILD_DIR) && wine dll_load_smoke.exe

odf-module-smoke: release $(STA1_COMPAT_MODULE) $(ODF_MODULE_SMOKE)
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
