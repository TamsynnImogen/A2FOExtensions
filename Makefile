CC := i686-w64-mingw32-gcc
CXX := i686-w64-mingw32-g++
CXX_HOST ?= g++
OBJDUMP := i686-w64-mingw32-objdump

.DEFAULT_GOAL := release

CPPFLAGS := -DWIN32_LEAN_AND_MEAN
CXXFLAGS := -std=gnu++17 -O2 -Wall -Wextra -Wpedantic -DNDEBUG
DLLFLAGS := -shared -static -static-libgcc -static-libstdc++ \
	-Wl,--enable-stdcall-fixup

BUILD_DIR := build
RUST_WIN_LIB := $(BUILD_DIR)/rust/i686-pc-windows-gnu/release/liba2fo_core.a

# Independent scene renderer. These targets never link the D3D8/9 facade.
NATIVE11_SOURCES := renderer/native11/renderer.cpp renderer/native11/engine_camera.cpp renderer/native11/texture.cpp renderer/native11/shadow_plan.cpp renderer/native11/shadow_target.cpp renderer/native11/live_material.cpp
NATIVE11_HEADERS := renderer/native11/renderer.hpp renderer/native11/scene.hpp renderer/native11/shaders.hpp renderer/native11/engine_camera.hpp renderer/native11/texture.hpp renderer/native11/shadow_plan.hpp renderer/native11/shadow_target.hpp
NATIVE11_HEADERS += sdk/include/a2fo_native11_model_scope_api.h sdk/include/a2fo_native11_geometry_policy_api.h sdk/include/a2fo_native11_live_material_api.h renderer/native11/model_geometry_policy.hpp renderer/native11/native_arithmetic.hpp renderer/native11/live_material.hpp
NATIVE11_SPRITE_CAPTURE_SOURCES := core/native11_sprites.cpp core/native11_sprite_bridge.S renderer/native11/sprite_observation.cpp renderer/native11/sprite_colour_policy.cpp
NATIVE11_SPRITE_CAPTURE_HEADERS := core/native11_sprites.hpp core/native11_sprite_profile.hpp renderer/native11/sprite_observation.hpp renderer/native11/sprite_colour_policy.hpp
NATIVE11_VIEW_SOURCES := renderer/native11/craft_view_policy.cpp renderer/native11/craft_view_capture.cpp
NATIVE11_VIEW_HEADERS := renderer/native11/craft_view_policy.hpp renderer/native11/craft_view_capture.hpp core/native11_craft_view_profile.hpp
NATIVE11_GEOMETRY_SOURCES := renderer/native11/model_geometry_policy.cpp renderer/native11/native_arithmetic.cpp
NATIVE11_ASSET_MANIFEST := rust/a2fo_native11_assets/Cargo.toml
NATIVE11_ASSET_LIB := $(BUILD_DIR)/native11-assets/i686-pc-windows-gnu/release/liba2fo_native11_assets.a
NATIVE11_MODEL_INPUTS := $(wildcard rust/a2fo_native11_model/src/*.rs) rust/a2fo_native11_model/Cargo.toml rust/a2fo_native11_model/Cargo.lock $(shell rg --files third_party/armada2-refit/menu-renderer/a2fmt/src -g '*.rs') third_party/armada2-refit/menu-renderer/a2fmt/Cargo.toml
NATIVE11_ASSET_INPUTS := $(wildcard rust/a2fo_native11_assets/src/*.rs) rust/a2fo_native11_assets/Cargo.lock $(NATIVE11_MODEL_INPUTS)
.PHONY: native11-build native11-tests-build native11-assets-test
native11-build: $(BUILD_DIR)/native11_preview.exe $(BUILD_DIR)/native11_scene_preview.exe $(BUILD_DIR)/A2FONative11Renderer.dll
native11-tests-build: $(BUILD_DIR)/native11_renderer_native.exe $(BUILD_DIR)/native11_assets_native.exe $(BUILD_DIR)/native11_texture_native.exe $(BUILD_DIR)/native11_content_native.exe $(BUILD_DIR)/native11_submission_native.exe $(BUILD_DIR)/native11_offscreen_native.exe $(BUILD_DIR)/native11_service_native.exe
native11-tests-build: $(BUILD_DIR)/native11_shadows_native.exe
native11-tests-build: $(BUILD_DIR)/native11_bound_service_native.exe
native11-tests-build: $(BUILD_DIR)/native11_dependency_native.exe
native11-tests-build: $(BUILD_DIR)/native11_texture_bindings_native.exe
$(BUILD_DIR)/native11_texture_bindings_native.exe: tests/native11_texture_bindings_native.cpp tests/native11_model_fixture.hpp $(BUILD_DIR)/native11_api_c.o sdk/include/a2fo_native11_api.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(BUILD_DIR)/native11_api_c.o
$(BUILD_DIR)/native11_dependency_native.exe: tests/native11_dependency_native.cpp tests/native11_model_fixture.hpp renderer/native11/model_dependencies.cpp renderer/native11/model_dependencies.hpp renderer/native11/dependency_plan.cpp renderer/native11/dependency_plan.hpp renderer/native11/texture_store.cpp renderer/native11/texture_store.hpp renderer/native11/model_store.cpp renderer/native11/asset_snapshot.cpp renderer/native11/capture_service.cpp rust/native11_model_ffi.h $(RUST_WIN_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/model_dependencies.cpp renderer/native11/dependency_plan.cpp renderer/native11/texture_store.cpp renderer/native11/model_store.cpp renderer/native11/asset_snapshot.cpp renderer/native11/capture_service.cpp $(RUST_WIN_LINK) -ladvapi32
$(BUILD_DIR)/native11_bound_service_native.exe: tests/native11_bound_service_native.cpp renderer/native11/asset_snapshot.cpp renderer/native11/asset_snapshot.hpp renderer/native11/model_store.cpp renderer/native11/model_store.hpp renderer/native11/capture_service.cpp renderer/native11/capture_service.hpp renderer/native11/scene_capture.hpp sdk/include/a2fo_native11_api.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/asset_snapshot.cpp renderer/native11/model_store.cpp renderer/native11/capture_service.cpp
native11-assets-test:
	cargo test --offline --manifest-path $(NATIVE11_ASSET_MANIFEST)
$(NATIVE11_ASSET_LIB): $(NATIVE11_ASSET_MANIFEST) $(NATIVE11_ASSET_INPUTS) | $(BUILD_DIR)
	CARGO_TARGET_DIR="$(abspath $(BUILD_DIR)/native11-assets)" cargo build --offline --release --target i686-pc-windows-gnu --manifest-path $(NATIVE11_ASSET_MANIFEST)
$(BUILD_DIR)/native11_renderer_native.exe: tests/native11_renderer_native.cpp renderer/native11/assets.h $(NATIVE11_SOURCES) $(NATIVE11_HEADERS) $(NATIVE11_ASSET_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(NATIVE11_SOURCES) $(NATIVE11_ASSET_LIB) -ldxguid -luuid -lntdll -luserenv -lws2_32 -lbcrypt -ladvapi32
$(BUILD_DIR)/native11_assets_native.exe: tests/native11_assets_native.cpp renderer/native11/assets.h $(NATIVE11_ASSET_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(NATIVE11_ASSET_LIB) -lntdll -luserenv -lws2_32 -lbcrypt -ladvapi32
$(BUILD_DIR)/native11_preview.exe: tools/native11_preview.cpp renderer/native11/assets.h renderer/shadows/math.hpp $(NATIVE11_SOURCES) $(NATIVE11_HEADERS) $(NATIVE11_ASSET_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(NATIVE11_SOURCES) $(NATIVE11_ASSET_LIB) -ldxguid -luuid -lntdll -luserenv -lws2_32 -lbcrypt -ladvapi32


$(BUILD_DIR)/native11_scene_preview.exe: tools/native11_scene_preview.cpp renderer/native11/content.cpp $(NATIVE11_GEOMETRY_SOURCES) renderer/native11/content.hpp renderer/native11/submission.cpp renderer/native11/submission.hpp renderer/native11/scene_capture.cpp renderer/native11/scene_capture.hpp renderer/native11/assets.h renderer/shadows/math.hpp $(NATIVE11_SOURCES) $(NATIVE11_HEADERS) $(NATIVE11_ASSET_LIB) renderer/native11/asset_snapshot.cpp renderer/native11/asset_snapshot.hpp renderer/native11/model_store.cpp renderer/native11/model_store.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/content.cpp $(NATIVE11_GEOMETRY_SOURCES) renderer/native11/submission.cpp renderer/native11/scene_capture.cpp renderer/native11/asset_snapshot.cpp renderer/native11/model_store.cpp $(NATIVE11_SOURCES) $(NATIVE11_ASSET_LIB) -ldxguid -luuid -lntdll -luserenv -lws2_32 -lbcrypt -ladvapi32

.PHONY: native11-texture-test
native11-texture-test: $(BUILD_DIR)/native11_texture_test
	$(BUILD_DIR)/native11_texture_test
$(BUILD_DIR)/native11_texture_test: tests/native11_texture_test.cpp renderer/native11/texture.cpp renderer/native11/texture.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< renderer/native11/texture.cpp
.PHONY: native11-shadow-plan-test
native11-shadow-plan-test: $(BUILD_DIR)/native11_shadow_plan_test
	$(BUILD_DIR)/native11_shadow_plan_test
$(BUILD_DIR)/native11_shadow_plan_test: tests/native11_shadow_plan_test.cpp renderer/native11/shadow_plan.cpp renderer/native11/shadow_plan.hpp renderer/native11/scene.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< renderer/native11/shadow_plan.cpp
$(BUILD_DIR)/native11_texture_native.exe: tests/native11_texture_native.cpp renderer/native11/assets.h $(NATIVE11_SOURCES) $(NATIVE11_HEADERS) $(NATIVE11_ASSET_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(NATIVE11_SOURCES) $(NATIVE11_ASSET_LIB) -ldxguid -luuid -lntdll -luserenv -lws2_32 -lbcrypt -ladvapi32

$(BUILD_DIR)/native11_content_native.exe: tests/native11_content_native.cpp renderer/native11/content.cpp $(NATIVE11_GEOMETRY_SOURCES) renderer/native11/content.hpp renderer/native11/live_material.cpp renderer/native11/live_material.hpp renderer/native11/texture.cpp renderer/native11/texture.hpp renderer/native11/assets.h $(NATIVE11_ASSET_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/content.cpp $(NATIVE11_GEOMETRY_SOURCES) renderer/native11/live_material.cpp renderer/native11/texture.cpp $(NATIVE11_ASSET_LIB) -lntdll -luserenv -lws2_32 -lbcrypt -ladvapi32
$(BUILD_DIR)/native11_submission_native.exe: tests/native11_submission_native.cpp tests/native11_device_slots.cpp renderer/native11/content.cpp $(NATIVE11_GEOMETRY_SOURCES) renderer/native11/content.hpp renderer/native11/submission.cpp renderer/native11/submission.hpp renderer/native11/scene_capture.hpp renderer/native11/assets.h $(NATIVE11_SOURCES) $(NATIVE11_HEADERS) $(NATIVE11_ASSET_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DA2FO_NATIVE11_FIXTURE -Werror -static -o $@ $< tests/native11_device_slots.cpp renderer/native11/content.cpp $(NATIVE11_GEOMETRY_SOURCES) renderer/native11/submission.cpp $(filter-out renderer/native11/renderer.cpp,$(NATIVE11_SOURCES)) $(NATIVE11_ASSET_LIB) -ldxguid -luuid -lntdll -luserenv -lws2_32 -lbcrypt -ladvapi32

$(BUILD_DIR)/native11_offscreen_native.exe: tests/native11_offscreen_native.cpp tests/native11_device_slots.cpp $(NATIVE11_SOURCES) $(NATIVE11_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DA2FO_NATIVE11_FIXTURE -Werror -static -o $@ $< tests/native11_device_slots.cpp $(filter-out renderer/native11/renderer.cpp,$(NATIVE11_SOURCES)) -ldxguid -luuid
$(BUILD_DIR)/native11_shadows_native.exe: tests/native11_shadows_native.cpp tests/native11_device_slots.cpp $(NATIVE11_SOURCES) $(NATIVE11_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DA2FO_NATIVE11_FIXTURE -Werror -static -o $@ $< tests/native11_device_slots.cpp $(filter-out renderer/native11/renderer.cpp,$(NATIVE11_SOURCES)) -ldxguid -luuid
$(BUILD_DIR)/A2FONative11Renderer.dll: renderer/native11/service.cpp renderer/native11/primitive_scene.cpp renderer/native11/service.def sdk/include/a2fo_native11_api.h renderer/native11/content.cpp $(NATIVE11_GEOMETRY_SOURCES) renderer/native11/content.hpp renderer/native11/submission.cpp renderer/native11/submission.hpp renderer/native11/scene_capture.hpp renderer/native11/assets.h $(NATIVE11_SOURCES) $(NATIVE11_HEADERS) $(NATIVE11_ASSET_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror $(DLLFLAGS) -Wl,--exclude-all-symbols -o $@ $< renderer/native11/primitive_scene.cpp renderer/native11/service.def renderer/native11/content.cpp $(NATIVE11_GEOMETRY_SOURCES) renderer/native11/submission.cpp $(NATIVE11_SOURCES) $(NATIVE11_ASSET_LIB) -ldxguid -luuid -lntdll -luserenv -lws2_32 -lbcrypt -ladvapi32
$(BUILD_DIR)/native11_api_c.o: tests/native11_api_c.c sdk/include/a2fo_native11_api.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror -c -o $@ $<
$(BUILD_DIR)/native11_service_native.exe: tests/native11_service_native.cpp $(BUILD_DIR)/native11_api_c.o sdk/include/a2fo_native11_api.h renderer/native11/capture_service.cpp renderer/native11/capture_service.hpp renderer/native11/scene_capture.cpp renderer/native11/scene_capture.hpp renderer/native11/engine_camera.cpp renderer/native11/engine_camera.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(BUILD_DIR)/native11_api_c.o renderer/native11/capture_service.cpp renderer/native11/scene_capture.cpp renderer/native11/engine_camera.cpp

.PHONY: native11-scene-tests native11-capture-tests-build
native11-scene-tests: $(BUILD_DIR)/native11_camera_test $(BUILD_DIR)/native11_scene_capture_test
	$(BUILD_DIR)/native11_camera_test
	$(BUILD_DIR)/native11_scene_capture_test
$(BUILD_DIR)/native11_camera_test: tests/native11_camera_test.cpp renderer/native11/engine_camera.cpp renderer/native11/engine_camera.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< renderer/native11/engine_camera.cpp
$(BUILD_DIR)/native11_scene_capture_test: tests/native11_scene_capture_test.cpp renderer/native11/scene_capture.cpp renderer/native11/scene_capture.hpp renderer/native11/engine_camera.cpp renderer/native11/engine_camera.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< renderer/native11/scene_capture.cpp renderer/native11/engine_camera.cpp
native11-capture-tests-build: $(BUILD_DIR)/native11_capture_bridge_test.exe $(BUILD_DIR)/native11_capture_guard_test.exe
native11-capture-tests-build: $(BUILD_DIR)/native11_asset_bridge_test.exe $(BUILD_DIR)/native11_asset_guard_test.exe
$(BUILD_DIR)/native11_asset_guard_test.exe: tests/native11_model_fixture.hpp

.PHONY: native11-opened-model-tests
native11-opened-model-tests: $(BUILD_DIR)/native11_opened_file_test $(BUILD_DIR)/native11_model_store_test $(BUILD_DIR)/native11_asset_snapshot_test
	$(BUILD_DIR)/native11_opened_file_test
	$(BUILD_DIR)/native11_model_store_test
	$(BUILD_DIR)/native11_asset_snapshot_test
$(BUILD_DIR)/native11_asset_snapshot_test: tests/native11_asset_snapshot_test.cpp renderer/native11/asset_snapshot.cpp renderer/native11/asset_snapshot.hpp renderer/native11/model_store.hpp renderer/native11/scene_capture.hpp sdk/include/a2fo_native11_api.h | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< renderer/native11/asset_snapshot.cpp
$(BUILD_DIR)/native11_opened_file_test: tests/native11_opened_file_test.cpp renderer/native11/opened_file.cpp renderer/native11/opened_file.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< renderer/native11/opened_file.cpp
$(BUILD_DIR)/native11_model_store_test: tests/native11_model_store_test.cpp renderer/native11/model_store.cpp renderer/native11/model_store.hpp renderer/native11/opened_file.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< renderer/native11/model_store.cpp
$(BUILD_DIR)/native11_asset_bridge_test.exe: tests/native11_asset_bridge_test.cpp tests/native11_asset_bridge_test.S core/native11_asset_bridge.S | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -msse2 -static -o $@ $< tests/native11_asset_bridge_test.S core/native11_asset_bridge.S
$(BUILD_DIR)/native11_asset_guard_test.exe: tests/native11_asset_guard_test.cpp core/native11_assets.cpp core/native11_assets.hpp core/native11_asset_profile.hpp core/native11_capture.cpp core/native11_capture.hpp core/native11_capture_scale_profile.hpp core/native11_capture_bridge.S core/native11_asset_bridge.S renderer/native11/opened_file.cpp renderer/native11/opened_file.hpp renderer/native11/model_store.cpp renderer/native11/model_store.hpp renderer/native11/scene_capture.cpp renderer/native11/scene_capture.hpp renderer/native11/engine_camera.cpp renderer/native11/engine_camera.hpp rust/native_hooks_ffi.h renderer/native11/asset_snapshot.cpp renderer/native11/asset_snapshot.hpp core/native11_textures.cpp core/native11_textures.hpp core/native11_texture_profile.hpp core/native11_texture_bridge.S renderer/native11/texture_store.cpp renderer/native11/texture_store.hpp core/native11_tga_profile.hpp core/native11_tga_bridge.S renderer/native11/open_read_owner.hpp renderer/native11/model_dependencies.cpp renderer/native11/model_dependencies.hpp renderer/native11/dependency_plan.cpp renderer/native11/dependency_plan.hpp rust/native11_model_ffi.h $(RUST_WIN_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Da2fo_rs_windows_patch_call=native11_fixture_patch_call -DA2FO_NATIVE11_CAPTURE_TESTING -Werror -static -o $@ $< core/native11_assets.cpp core/native11_capture.cpp core/native11_capture_bridge.S core/native11_asset_bridge.S renderer/native11/opened_file.cpp renderer/native11/model_store.cpp renderer/native11/scene_capture.cpp renderer/native11/engine_camera.cpp renderer/native11/asset_snapshot.cpp core/native11_textures.cpp core/native11_texture_bridge.S renderer/native11/texture_store.cpp core/native11_tga_bridge.S renderer/native11/model_dependencies.cpp renderer/native11/dependency_plan.cpp $(NATIVE11_MATERIAL_SOURCES) $(RUST_WIN_LINK) -ladvapi32
$(BUILD_DIR)/native11_capture_bridge_test.exe: tests/native11_capture_bridge_test.cpp tests/native11_capture_bridge_test.S core/native11_capture_bridge.S renderer/native11/scene_capture.cpp renderer/native11/scene_capture.hpp renderer/native11/engine_camera.cpp renderer/native11/engine_camera.hpp renderer/native11/asset_snapshot.cpp renderer/native11/asset_snapshot.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -msse2 -static -o $@ $< tests/native11_capture_bridge_test.S core/native11_capture_bridge.S renderer/native11/scene_capture.cpp renderer/native11/engine_camera.cpp


$(BUILD_DIR)/native11_capture_guard_test.exe: tests/native11_capture_guard_test.cpp core/native11_capture.cpp core/native11_capture.hpp core/native11_capture_scale_profile.hpp core/native11_capture_bridge.S renderer/native11/scene_capture.cpp renderer/native11/scene_capture.hpp renderer/native11/engine_camera.cpp renderer/native11/engine_camera.hpp rust/native_hooks_ffi.h core/native11_assets.cpp core/native11_assets.hpp core/native11_asset_profile.hpp core/native11_asset_bridge.S renderer/native11/model_store.cpp renderer/native11/model_store.hpp renderer/native11/opened_file.cpp renderer/native11/opened_file.hpp renderer/native11/asset_snapshot.cpp renderer/native11/asset_snapshot.hpp core/native11_textures.cpp core/native11_textures.hpp core/native11_texture_profile.hpp core/native11_texture_bridge.S renderer/native11/texture_store.cpp renderer/native11/texture_store.hpp core/native11_tga_profile.hpp core/native11_tga_bridge.S renderer/native11/open_read_owner.hpp renderer/native11/model_dependencies.cpp renderer/native11/model_dependencies.hpp renderer/native11/dependency_plan.cpp renderer/native11/dependency_plan.hpp rust/native11_model_ffi.h $(RUST_WIN_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Da2fo_rs_windows_patch_call=native11_fixture_patch_call -Werror -static -o $@ $< core/native11_capture.cpp core/native11_capture_bridge.S renderer/native11/scene_capture.cpp renderer/native11/engine_camera.cpp core/native11_assets.cpp core/native11_asset_bridge.S renderer/native11/model_store.cpp renderer/native11/opened_file.cpp renderer/native11/asset_snapshot.cpp core/native11_textures.cpp core/native11_texture_bridge.S renderer/native11/texture_store.cpp core/native11_tga_bridge.S renderer/native11/model_dependencies.cpp renderer/native11/dependency_plan.cpp $(NATIVE11_MATERIAL_SOURCES) $(RUST_WIN_LINK) -ladvapi32

# Gameplay shadow fixtures are independent of the production native hooks.
.PHONY: ingame-shadow-tests-build ingame-shadow-math-test
ingame-shadow-tests-build: $(BUILD_DIR)/ingame_shadow_math_test $(BUILD_DIR)/ingame_shadow_pass_native.exe $(BUILD_DIR)/ingame_shadow_gpu_fixture.exe $(BUILD_DIR)/ingame_shadow_native_depth_fixture.exe $(BUILD_DIR)/ingame_shadow_depth_preserve_fixture.exe $(BUILD_DIR)/ingame_shadow_depth_msaa_fixture.exe $(BUILD_DIR)/ingame_shadow_bias_stability_fixture.exe
ingame-shadow-math-test: $(BUILD_DIR)/ingame_shadow_math_test
	$<
$(BUILD_DIR)/ingame_shadow_math_test: tests/ingame_shadow_math_test.cpp renderer/shadows/math.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $<
$(BUILD_DIR)/ingame_shadow_pass_native.exe: tests/ingame_shadow_pass_native.cpp renderer/shadows/pass.cpp renderer/shadows/pass.hpp renderer/shadows/math.hpp renderer/shadows/shader.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/shadows/pass.cpp -ld3d9 -ld3dcompiler -lole32
$(BUILD_DIR)/ingame_shadow_gpu_fixture.exe: tests/ingame_shadow_gpu_fixture.cpp renderer/shadows/pass.cpp renderer/shadows/pass.hpp renderer/shadows/math.hpp renderer/shadows/shader.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/shadows/pass.cpp -ld3d9 -ld3dcompiler -lole32
$(BUILD_DIR)/ingame_shadow_native_depth_fixture.exe: tests/ingame_shadow_native_depth_fixture.cpp renderer/shadows/pass.cpp renderer/shadows/pass.hpp renderer/shadows/math.hpp renderer/shadows/shader.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/shadows/pass.cpp -ld3d9 -ld3dcompiler -lole32
$(BUILD_DIR)/ingame_shadow_depth_preserve_fixture.exe: tests/ingame_shadow_depth_preserve_fixture.cpp renderer/shadows/pass.cpp renderer/shadows/pass.hpp renderer/shadows/math.hpp renderer/shadows/shader.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/shadows/pass.cpp -ld3d9 -ld3dcompiler -lole32
$(BUILD_DIR)/ingame_shadow_depth_msaa_fixture.exe: tests/ingame_shadow_depth_msaa_fixture.cpp renderer/shadows/pass.cpp renderer/shadows/pass.hpp renderer/shadows/math.hpp renderer/shadows/shader.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/shadows/pass.cpp -ld3d9 -ld3dcompiler -lole32
$(BUILD_DIR)/ingame_shadow_bias_stability_fixture.exe: tests/ingame_shadow_bias_stability_fixture.cpp renderer/shadows/pass.cpp renderer/shadows/pass.hpp renderer/shadows/math.hpp renderer/shadows/shader.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/shadows/pass.cpp -ld3d9 -ld3dcompiler -lole32
# Rust owns migrated implementations; C++ files at the boundary adapt STL only.
CARGO ?= cargo
RUST_MANIFEST := rust/a2fo_core/Cargo.toml
RUST_SOURCES := $(wildcard rust/a2fo_core/src/*.rs) $(RUST_MANIFEST) rust/a2fo_core/Cargo.lock $(NATIVE11_MODEL_INPUTS)
RUST_FFI_HEADERS := $(wildcard rust/*_ffi.h)
RUST_TARGET_DIR := $(abspath $(BUILD_DIR)/rust)
RUST_HOST_LIB := $(BUILD_DIR)/rust/release/liba2fo_core.a
# no_std + alloc uses only the CRT already linked by MinGW/g++.
# rustc 1.96 --print=native-static-libs is empty for these archives.
RUST_WIN_SYSTEM_LIBS :=
RUST_HOST_SYSTEM_LIBS :=
RUST_WIN_LINK = $(RUST_WIN_LIB) $(RUST_WIN_SYSTEM_LIBS)
RUST_HOST_LINK = $(RUST_HOST_LIB) $(RUST_HOST_SYSTEM_LIBS)
RUST_MEDIA_DLL := $(BUILD_DIR)/A2FOMedia.dll
RUST_MEDIA_SOURCES := rust/a2fo_media/Cargo.toml rust/a2fo_media/Cargo.lock \
	$(shell find rust/a2fo_media/src third_party/rust-media -type d \( -name .git -o -name target \) -prune -o -type f -print)

$(RUST_MEDIA_DLL): $(RUST_MEDIA_SOURCES) tools/build_rust_media.py | $(BUILD_DIR)
	python3 tools/build_rust_media.py --target-dir $(BUILD_DIR)/rust-media-target --output $@

# Each custom i686 ABI family is a separate Rust object. Keeping families
# separate avoids pulling unrelated module callbacks into standalone DLLs.
a2fo_rust_bridge = $(BUILD_DIR)/rust-bridges/$(patsubst %.S,%.o,$(1))
RUST_BRIDGE_SOURCES := $(wildcard rust/a2fo_bridges/core/*.rs rust/a2fo_bridges/modules/*/*.rs)
$(BUILD_DIR)/rust-bridges/%.o: rust/a2fo_bridges/%.rs rust/a2fo_bridges/manifest.json tools/build_rust_bridge.py
	@mkdir -p $(dir $@)
	python3 tools/build_rust_bridge.py --source $< --output $@
.PHONY: rust-bridge-objects
rust-bridge-objects: $(patsubst rust/a2fo_bridges/%.rs,$(BUILD_DIR)/rust-bridges/%.o,$(RUST_BRIDGE_SOURCES))

$(RUST_WIN_LIB): $(RUST_SOURCES)
	$(CARGO) build --offline --locked --manifest-path $(RUST_MANIFEST) --target-dir $(RUST_TARGET_DIR) --release --target i686-pc-windows-gnu
$(RUST_HOST_LIB): $(RUST_SOURCES)
	$(CARGO) build --offline --locked --manifest-path $(RUST_MANIFEST) --target-dir $(RUST_TARGET_DIR) --release
.PHONY: rust-tests rust-checks
rust-tests:
	$(CARGO) test --offline --locked --manifest-path $(RUST_MANIFEST) --target-dir $(RUST_TARGET_DIR)
rust-checks:
	$(CARGO) fmt --manifest-path $(RUST_MANIFEST) -- --check
	$(CARGO) clippy --offline --locked --manifest-path $(RUST_MANIFEST) --target-dir $(RUST_TARGET_DIR) --all-targets -- -D warnings

$(BUILD_DIR)/rust_builtin_state_test.exe: tests/builtin_state_test.cpp core/builtin_state.cpp core/builtin_state.hpp rust/builtin_state_ffi.h $(RUST_WIN_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ tests/builtin_state_test.cpp core/builtin_state.cpp $(RUST_WIN_LINK)
$(BUILD_DIR)/rust_resupply_policy_test.exe: tests/resupply_policy_test.cpp modules/A2FOEnergySystems/resupply_policy.cpp modules/A2FOEnergySystems/resupply_policy.hpp rust/resupply_ffi.h $(RUST_WIN_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ tests/resupply_policy_test.cpp modules/A2FOEnergySystems/resupply_policy.cpp $(RUST_WIN_LINK)
.PHONY: rust-policy-tests
rust-policy-tests: rust-tests rust-checks builtin-tests $(BUILD_DIR)/resupply_policy_test $(BUILD_DIR)/resupply_runtime_host_test
	$(BUILD_DIR)/resupply_policy_test
	$(BUILD_DIR)/resupply_runtime_host_test

D3D11_UI_SOURCES := renderer/d3d11/com_guard.cpp renderer/d3d11/compressed_texture.cpp renderer/textures/dds.cpp renderer/d3d11/ui_renderer.cpp renderer/shaders/bytecode_cache.cpp renderer/state/fvf.cpp renderer/textures/upload.cpp
D3D11_UI_HEADERS := renderer/d3d11/compressed_texture.hpp renderer/textures/dds.hpp renderer/shaders/bc_normals.hpp renderer/d3d11/ui_renderer.hpp renderer/shaders/ui_fixed_function.hpp renderer/shaders/bytecode_cache.hpp renderer/state/fvf.hpp renderer/textures/upload.hpp
D3D8_RESOURCE_SOURCES := renderer/d3d8_compat/resources/compressed_texture8.cpp renderer/d3d11/texture_handle.cpp
D3D8_RESOURCE_HEADERS := renderer/d3d8_compat/resources/compressed_texture8.hpp renderer/d3d11/texture_handle.hpp
D3D8_SHADER_SOURCES := renderer/shaders/d3d8_tokens.cpp
D3D8_SHADER_HEADERS := renderer/shaders/d3d8_tokens.hpp renderer/d3d8_compat/shader_diagnostics.inl
D3D11_GAME_SOURCES := renderer/d3d11/game/backend.cpp renderer/d3d11/game/draw_pipeline.cpp renderer/d3d8_compat/game/factory.cpp renderer/d3d8_compat/game/d3dx_bridge.cpp renderer/d3d8_compat/game/device.cpp renderer/d3d8_compat/game/resources.cpp renderer/d3d8_compat/game/draw.cpp renderer/d3d8_compat/game/state_blocks.cpp renderer/d3d8_compat/game/unsupported.cpp renderer/d3d8_compat/game/bc_catalog.cpp
D3D11_GAME_HEADERS := renderer/d3d11/game/contracts.hpp renderer/d3d8_compat/game/factory.hpp renderer/d3d8_compat/game/private.hpp renderer/d3d8_compat/game/bc_catalog.hpp

# Separately built, opt-in crash reporting; never a release/runtime dependency.
.PHONY: log-mailer log-mailer-tests log-mailer-cloudflare-tests log-mailer-loader-smoke
log-mailer-cloudflare-tests:
	node --test tests/log_mailer_cloudflare_test.mjs
log-mailer: $(BUILD_DIR)/modules/A2FOLogMailer.dll $(BUILD_DIR)/modules/A2FOLogMailerHelper.exe

# Build-only: run this headless fixture explicitly with the DLL/helper paths.
log-mailer-loader-smoke: log-mailer $(BUILD_DIR)/log_mailer_loader_smoke.exe

$(BUILD_DIR)/log_mailer_loader_smoke.exe: tests/log_mailer_loader_smoke.cpp sdk/include/a2fo_module_api.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=gnu++17 -O2 -Wall -Wextra -Wpedantic -static -municode -o $@ $<

$(BUILD_DIR)/modules/A2FOLogMailer.dll: modules/A2FOLogMailer/module.cpp modules/A2FOLogMailer/config.hpp modules/A2FOLogMailer/distribution.hpp sdk/include/a2fo_module_api.h | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/modules
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) -o $@ $< $(RUST_WIN_LINK)

$(BUILD_DIR)/modules/A2FOLogMailerHelper.exe: modules/A2FOLogMailer/helper.cpp modules/A2FOLogMailer/config.hpp modules/A2FOLogMailer/distribution.hpp modules/A2FOLogMailer/report.hpp | $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/modules
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -static -municode -o $@ $< -lwinhttp

log-mailer-tests: log-mailer $(BUILD_DIR)/log_mailer_test $(BUILD_DIR)/log_mailer_native_test.exe
	$(BUILD_DIR)/log_mailer_test
	python3 -m unittest discover -s tests -p log_mailer_relay_test.py -v

$(BUILD_DIR)/log_mailer_test: tests/log_mailer_test.cpp modules/A2FOLogMailer/report.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -o $@ $<

$(BUILD_DIR)/log_mailer_native_test.exe: tests/log_mailer_native_test.cpp modules/A2FOLogMailer/helper.cpp modules/A2FOLogMailer/config.hpp modules/A2FOLogMailer/distribution.hpp modules/A2FOLogMailer/report.hpp modules/A2FOLogMailer/module.cpp sdk/include/a2fo_module_api.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=gnu++17 -O2 -Wall -Wextra -Wpedantic -static -municode -o $@ $< -lwinhttp $(RUST_WIN_LINK)

# Isolated renderer fixtures only; none launches Armada or changes an install.
.PHONY: renderer-modernisation-tests
renderer-modernisation-tests: $(BUILD_DIR)/d3d8_observer_test.exe $(BUILD_DIR)/d3d11_probe_native.exe $(BUILD_DIR)/d3d8_d3d11_window_test.exe

$(BUILD_DIR)/d3d8_observer_test.exe: tests/d3d8_observer_test.cpp renderer/d3d8_compat/observer.cpp renderer/d3d8_compat/generated_thunks.inl renderer/d3d8_compat/observer.hpp renderer/d3d11/probe_bridge.hpp $(D3D8_SHADER_SOURCES) $(D3D8_SHADER_HEADERS) $(D3D11_GAME_SOURCES) $(D3D11_GAME_HEADERS) $(D3D11_UI_SOURCES) $(D3D11_UI_HEADERS) renderer/d3d11/texture_handle.cpp renderer/d3d11/texture_handle.hpp renderer/shaders/vs11.cpp renderer/shaders/vs11.hpp core/extension_roots.cpp core/extension_roots.hpp $(RUST_WIN_LIB) $(RUST_FFI_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -fno-strict-aliasing -static -o $@ $< $(D3D8_SHADER_SOURCES) $(D3D11_GAME_SOURCES) $(D3D11_UI_SOURCES) renderer/d3d11/texture_handle.cpp renderer/shaders/vs11.cpp core/extension_roots.cpp -ldxguid -luuid $(RUST_WIN_LINK)

$(BUILD_DIR)/d3d11_probe_native.exe: tests/d3d11_probe_native.cpp renderer/d3d11/probe_device.cpp renderer/d3d11/probe_device.hpp $(D3D11_UI_SOURCES) $(D3D11_UI_HEADERS) $(RUST_WIN_LIB) $(RUST_FFI_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ tests/d3d11_probe_native.cpp renderer/d3d11/probe_device.cpp $(D3D11_UI_SOURCES) -luuid $(RUST_WIN_LINK)

$(BUILD_DIR)/d3d8_d3d11_window_test.exe: tests/d3d8_d3d11_window_test.cpp renderer/d3d8_compat/observer.cpp renderer/d3d8_compat/generated_thunks.inl renderer/d3d8_compat/observer.hpp renderer/d3d11/probe_bridge.cpp renderer/d3d11/probe_bridge.hpp renderer/d3d11/probe_device.cpp renderer/d3d11/probe_device.hpp $(D3D11_GAME_SOURCES) $(D3D11_GAME_HEADERS) $(D3D11_UI_SOURCES) $(D3D11_UI_HEADERS) $(D3D8_SHADER_SOURCES) $(D3D8_SHADER_HEADERS) renderer/d3d11/texture_handle.cpp renderer/d3d11/texture_handle.hpp renderer/shaders/vs11.cpp renderer/shaders/vs11.hpp core/extension_roots.cpp core/extension_roots.hpp $(RUST_WIN_LIB) $(RUST_FFI_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -fno-strict-aliasing -static -o $@ tests/d3d8_d3d11_window_test.cpp renderer/d3d11/probe_bridge.cpp renderer/d3d11/probe_device.cpp $(D3D11_GAME_SOURCES) $(D3D11_UI_SOURCES) $(D3D8_SHADER_SOURCES) renderer/d3d11/texture_handle.cpp renderer/shaders/vs11.cpp core/extension_roots.cpp -ld3d8 -ldxguid -luuid $(RUST_WIN_LINK)

# Temporary minimap diagnostics, built explicitly rather than in release/all.
.PHONY: radar-diagnostics-tests
radar-diagnostics-tests: $(BUILD_DIR)/modules/A2FORadarDiagnostics.dll $(BUILD_DIR)/radar_clear_native.exe $(BUILD_DIR)/radar_diagnostics_test.exe

$(BUILD_DIR)/radar_clear_native.exe: tests/radar_clear_native.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< -ld3d8 -lgdi32 -luser32

$(BUILD_DIR)/radar_diagnostics_test.exe: tests/radar_diagnostics_test.cpp modules/A2FORadarDiagnostics/module.cpp sdk/include/a2fo_supported_armada.hpp sdk/include/a2fo_module_api.h rust/native_hooks_ffi.h $(RUST_WIN_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(RUST_WIN_LINK)

$(BUILD_DIR)/modules/A2FORadarDiagnostics.dll: modules/A2FORadarDiagnostics/module.cpp sdk/include/a2fo_supported_armada.hpp sdk/include/a2fo_module_api.h | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/modules
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror $(DLLFLAGS) -o $@ $< $(RUST_WIN_LINK)

.PHONY: renderer-compat-tests
renderer-compat-tests: $(BUILD_DIR)/renderer_fvf_test.exe $(BUILD_DIR)/renderer_texture_upload_test.exe $(BUILD_DIR)/renderer_shader_cache_test.exe $(BUILD_DIR)/renderer_shader_tokens_test.exe $(BUILD_DIR)/renderer_ui_native.exe

.PHONY: renderer-vs11-tests
renderer-vs11-tests: $(BUILD_DIR)/renderer_vs11_test.exe $(BUILD_DIR)/renderer_vs11_native.exe

$(BUILD_DIR)/renderer_vs11_test.exe: tests/renderer_vs11_test.cpp renderer/shaders/vs11.cpp renderer/shaders/vs11.hpp $(D3D8_SHADER_SOURCES) renderer/shaders/d3d8_tokens.hpp tests/fixtures/renderer_vs11_capture.inc | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/shaders/vs11.cpp $(D3D8_SHADER_SOURCES) $(RUST_WIN_LINK)

$(BUILD_DIR)/renderer_vs11_native.exe: tests/renderer_vs11_native.cpp tests/renderer_vs11_reference.cpp tests/renderer_vs11_scene.hpp tests/fixtures/renderer_vs11_capture.inc renderer/shaders/vs11.cpp renderer/shaders/vs11.hpp renderer/shaders/bytecode_cache.cpp renderer/shaders/bytecode_cache.hpp $(D3D8_SHADER_SOURCES) renderer/shaders/d3d8_tokens.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ tests/renderer_vs11_native.cpp tests/renderer_vs11_reference.cpp renderer/shaders/vs11.cpp renderer/shaders/bytecode_cache.cpp $(D3D8_SHADER_SOURCES) -ld3d8 -ldxguid -luuid $(RUST_WIN_LINK)

$(BUILD_DIR)/renderer_shader_tokens_test.exe: tests/renderer_shader_tokens_test.cpp $(D3D8_SHADER_SOURCES) renderer/shaders/d3d8_tokens.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(D3D8_SHADER_SOURCES) $(RUST_WIN_LINK)

$(BUILD_DIR)/renderer_fvf_test.exe: tests/renderer_fvf_test.cpp renderer/state/fvf.cpp renderer/state/fvf.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/state/fvf.cpp $(RUST_WIN_LINK)

$(BUILD_DIR)/renderer_texture_upload_test.exe: tests/renderer_texture_upload_test.cpp renderer/textures/upload.cpp renderer/textures/upload.hpp $(RUST_WIN_LIB) $(RUST_FFI_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/textures/upload.cpp $(RUST_WIN_LINK)

$(BUILD_DIR)/renderer_shader_cache_test.exe: tests/renderer_shader_cache_test.cpp renderer/shaders/bytecode_cache.cpp renderer/shaders/bytecode_cache.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/shaders/bytecode_cache.cpp $(RUST_WIN_LINK)

$(BUILD_DIR)/renderer_ui_native.exe: tests/renderer_ui_native.cpp tests/renderer_ui_reference.cpp tests/renderer_ui_scene.hpp $(D3D11_UI_SOURCES) $(D3D11_UI_HEADERS) $(RUST_WIN_LIB) $(RUST_FFI_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ tests/renderer_ui_native.cpp tests/renderer_ui_reference.cpp $(D3D11_UI_SOURCES) -ld3d8 -ldxguid -luuid $(RUST_WIN_LINK)

.PHONY: renderer-bc-tests
renderer-bc-tests: $(BUILD_DIR)/renderer_dds_test.exe $(BUILD_DIR)/renderer_bc_native.exe $(BUILD_DIR)/renderer_bc_inventory.exe

$(BUILD_DIR)/renderer_dds_test.exe: tests/renderer_dds_test.cpp renderer/textures/dds.cpp renderer/textures/dds.hpp $(RUST_WIN_LIB) $(RUST_FFI_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/textures/dds.cpp $(RUST_WIN_LINK)

$(BUILD_DIR)/renderer_bc_native.exe: tests/renderer_bc_native.cpp tests/renderer_bc_blocks.hpp $(D3D11_UI_SOURCES) $(D3D11_UI_HEADERS) tests/renderer_texture8_contract.cpp $(D3D8_RESOURCE_SOURCES) $(D3D8_RESOURCE_HEADERS) $(RUST_WIN_LIB) $(RUST_FFI_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< tests/renderer_texture8_contract.cpp $(D3D11_UI_SOURCES) $(D3D8_RESOURCE_SOURCES) -ld3d8 -ldxguid -luuid $(RUST_WIN_LINK)

$(BUILD_DIR)/renderer_bc_inventory.exe: tests/renderer_bc_inventory.cpp renderer/d3d11/com_guard.cpp renderer/d3d11/compressed_texture.cpp renderer/d3d11/compressed_texture.hpp renderer/textures/dds.cpp renderer/textures/dds.hpp $(RUST_WIN_LIB) $(RUST_FFI_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/d3d11/com_guard.cpp renderer/d3d11/compressed_texture.cpp renderer/textures/dds.cpp -luuid $(RUST_WIN_LINK)

MODULE_DIR := $(BUILD_DIR)/modules
MISSIONS_DIR := $(BUILD_DIR)/missions
STA1_CLASSIC_DIR := $(BUILD_DIR)/sta1-classic
STA1_COMPAT_MODULE := $(MODULE_DIR)/A1Compat.dll
A1_FALLBACKS_MODULE := $(MODULE_DIR)/A1Fallbacks.dll
ALWAYS_SHOW_SHIELDS_MODULE := $(MODULE_DIR)/A2FOAlwaysShowShields.dll
ANIMATIONS_MODULE := $(MODULE_DIR)/A2FOAnimations.dll
ANIMATED_HARDPOINTS_MODULE := $(MODULE_DIR)/A2FOAnimatedHardpoints.dll
BUILD_TOOLTIPS_MODULE := $(MODULE_DIR)/A2FOBuildTooltips.dll
CHEATS_MODULE := $(MODULE_DIR)/A2FOCheats.dll
CRAFT_IDENTITY_MODULE := $(MODULE_DIR)/A2FOCraftIdentity.dll
EDIT_MENU_MODULE := $(MODULE_DIR)/A2FOEditMenu.dll
DIRECTIONAL_SHIELDS_MODULE := $(MODULE_DIR)/A2FODirectionalShields.dll
ENERGY_SYSTEMS_MODULE := $(MODULE_DIR)/A2FOEnergySystems.dll
MUZZLE_FLASHES_MODULE := $(MODULE_DIR)/A2FOMuzzleFlashes.dll
INSTANT_ACTION_SETTINGS_MODULE := $(MODULE_DIR)/A2FOInstantActionSettings.dll
RESOURCES_MODULE := $(MODULE_DIR)/A2FOResources.dll
MISSION_SELECTOR_MODULE := $(MODULE_DIR)/A2FOMissionSelector.dll
FIRE_ARCS_MODULE := $(MODULE_DIR)/A2FOFireArcs.dll
WEAPON_DAMAGE_CONTROLS_MODULE := $(MODULE_DIR)/A2FOWeaponDamageControls.dll
WRECKAGE_MODULE := $(MODULE_DIR)/A2FOWreckage.dll
NORMAL_WEAPON_TECH_MODULE := $(MODULE_DIR)/A2FONormalWeaponTech.dll
NEBULA_RENDERER_MODULE := $(MODULE_DIR)/A2FONebulaRenderer.dll
POINT_DEFENSE_CYCLES_MODULE := $(MODULE_DIR)/A2FOPointDefenseCycles.dll
SWARM_SYSTEM_MODULE := $(MODULE_DIR)/A2FOSwarmSystem.dll
TEXTURE_VARIANTS_MODULE := $(MODULE_DIR)/A2FOTextureVariants.dll
ODF_VARIANTS_MODULE := $(MODULE_DIR)/A2FOODFVariants.dll
TURRETS_MODULE := $(MODULE_DIR)/A2FOTurrets.dll
REFIT_YARDS_MODULE := $(MODULE_DIR)/A2FORefitYards.dll
SQUADRONS_MODULE := $(MODULE_DIR)/A2FOSquadrons.dll
COLLECTIVE_XP_MODULE := $(MODULE_DIR)/A2FOCollectiveXP.dll
COLLECTIVE_XP_SOURCES := modules/A2FOCollectiveXP/collective_xp.cpp
COLLECTIVE_XP_HEADERS := modules/A2FOCollectiveXP/collective_xp.hpp
BREEDERS_MODULE := $(MODULE_DIR)/A2FOBreeders.dll
STATION_ROTATION_MODULE := $(MODULE_DIR)/A2FOStationRotation.dll
TEAM_CHANGE_WEAPONS_MODULE := $(MODULE_DIR)/A2FOTeamChangeWeapons.dll
INFESTATION_MODULE := $(MODULE_DIR)/A2FOInfestation.dll
M3D_MODULE := $(MODULE_DIR)/A2FOM3D.dll
SOL_MAPS_MODULE := $(MODULE_DIR)/A2FOSOLMaps.dll
SHELL_DISPLAY_MODULE := $(MODULE_DIR)/A2FOShellDisplay.dll
VIDEO_MODULE := $(MODULE_DIR)/A2FOVideo.dll
RECOVERY_TESTS_MODULE := $(MODULE_DIR)/A2FORecoveryTests.dll
CAMERA_CONTROLS_MODULE := $(MODULE_DIR)/A2FOCameraControls.dll
VIEW_DISTANCE_MODULE := $(MODULE_DIR)/A2FOViewDistance.dll
MULTI_YARD_MODULE := $(MODULE_DIR)/A2FOMultiYardBuild.dll
SOL_FORMAT_TEST := $(BUILD_DIR)/sol_format_test
MISSIONS_MODULE := $(MODULE_DIR)/A2FOMissions.dll
MMM_LOADER_SHIM := $(MISSIONS_DIR)/mmm_loader.dll
TEAM_CHANGE_WEAPONS_SMOKE := $(BUILD_DIR)/team_change_weapons_smoke.exe
NEBULA_SHADER_ASSETS := \
	$(BUILD_DIR)/Shaders/dot3_amd.nvv \
	$(BUILD_DIR)/Shaders/dot3_amd9.nvv \
	$(BUILD_DIR)/Shaders/dx8/pixel/ps.nvv \
	$(BUILD_DIR)/Shaders/dx8/pixel/ps_specular.nvv \
	$(BUILD_DIR)/Shaders/dx8/vertex/vs_flat_lighting.nvv
NEBULA_LICENSE := $(BUILD_DIR)/licenses/armada-nebula-patch.txt
STA1_CLASSIC_GUI_CFG := \
	mods/STA1Classic/misc/gui_fed.cfg \
	mods/STA1Classic/misc/gui_bor.cfg \
	mods/STA1Classic/misc/gui_kli.cfg \
	mods/STA1Classic/misc/gui_rom.cfg
STA1_CLASSIC_SPRITE_REGISTRY := mods/STA1Classic/Sprites/sprites.spr
STA1_CLASSIC_MISSION_SELECTOR := mods/STA1Classic/mission_selector.ini
SMOKE_TEST := $(BUILD_DIR)/dll_load_smoke.exe
GAME_MONITOR_SMOKE := $(BUILD_DIR)/game_monitor_win32_smoke.exe
FPQ_PATHS_TEST := $(BUILD_DIR)/fpq_paths_test
ODF_PATHS_TEST := $(BUILD_DIR)/odf_paths_test
ODF_MODULE_SMOKE := $(BUILD_DIR)/odf_module_init_smoke.exe
EXTENSION_ROOTS_TEST := $(BUILD_DIR)/extension_roots_test
EXTENSION_ROOT_SMOKE := $(BUILD_DIR)/extension_root_discovery_smoke.exe
MODULE_API_TEST := $(BUILD_DIR)/module_api_test
MODULE_POLICY_TEST := $(BUILD_DIR)/module_policy_test
HYBRID_PRODUCTION_TEST := $(BUILD_DIR)/hybrid_production_test
BUILD_SUBMENU_CONFIG_TEST := $(BUILD_DIR)/build_submenu_config_test
TURRET_MATH_TEST := $(BUILD_DIR)/turret_math_test
CRAFT_IDENTITY_TEST := $(BUILD_DIR)/craft_identity_test
EDIT_MENU_TEST := $(BUILD_DIR)/edit_menu_test
ENERGY_SYSTEMS_TEST := $(BUILD_DIR)/energy_systems_test
DIRECTIONAL_SHIELDS_TEST := $(BUILD_DIR)/directional_shields_test
INSTANT_ACTION_SETTINGS_TEST := $(BUILD_DIR)/instant_action_settings_test
BUILD_TIME_TEXT_TEST := $(BUILD_DIR)/build_time_text_test
ADDITIONAL_RESOURCES_TEST := $(BUILD_DIR)/additional_resources_test
A1_RACE_MENU_TEST := $(BUILD_DIR)/a1_race_menu_policy_test
A1_TEAM_COLOR_TEST := $(BUILD_DIR)/a1_team_color_policy_test
A1_BZN_POLICY_TEST := $(BUILD_DIR)/a1_bzn_policy_test
A1_UI_POLICY_TEST := $(BUILD_DIR)/a1_ui_policy_test
A1_FALLBACK_POLICY_TEST := $(BUILD_DIR)/a1_fallback_policy_test
FIRE_ARC_TEST := $(BUILD_DIR)/fire_arc_test
UPGRADE_POD_CONFIG_TEST := $(BUILD_DIR)/upgrade_pod_config_test
WRECKAGE_POLICY_TEST := $(BUILD_DIR)/wreckage_policy_test
WEAPON_DAMAGE_CONTROLS_TEST := $(BUILD_DIR)/weapon_damage_controls_test
SHIELD_VISIBILITY_TEST := $(BUILD_DIR)/shield_visibility_test
NEBULA_EMISSIVE_TEST := $(BUILD_DIR)/nebula_emissive_test
AMD_DOT3_COMPAT_TEST := $(BUILD_DIR)/amd_dot3_compat_test
COM_OWNER_TEST := $(BUILD_DIR)/com_owner_test
GAME_MONITOR_POLICY_TEST := $(BUILD_DIR)/game_monitor_policy_test
ART_TEXTURE_SUFFIX_CONFIG_TEST := $(BUILD_DIR)/art_texture_suffix_config_test
DECAL_MATH_TEST := $(BUILD_DIR)/rust_stage16_decal_binding
POINT_DEFENSE_CYCLE_TEST := $(BUILD_DIR)/point_defense_cycle_test
SWARM_MOTION_TEST := $(BUILD_DIR)/swarm_motion_test
TEXTURE_VARIANTS_TEST := $(BUILD_DIR)/texture_variants_test
M3D_CONVERSION_TEST := $(BUILD_DIR)/m3d_conversion_test
M3D_RUNTIME_HIERARCHY_TEST := $(BUILD_DIR)/m3d_runtime_hierarchy_test.exe
REFIT_POLICY_TEST := $(BUILD_DIR)/refit_policy_test
SQUADRONS_TEST := $(BUILD_DIR)/squadrons_test
SQUADRONS_X86_TEST := $(BUILD_DIR)/squadrons_test.exe
SQUADRONS_SOURCES := modules/A2FOSquadrons/squadron_config.cpp \
	modules/A2FOSquadrons/squadron_state.cpp
SQUADRONS_HEADERS := modules/A2FOSquadrons/squadron_config.hpp \
	modules/A2FOSquadrons/squadron_state.hpp
STATION_ROTATION_TEST := $(BUILD_DIR)/station_rotation_test
STATION_ROTATION_SMOKE := $(BUILD_DIR)/station_rotation_smoke.exe
A2FO_TELEMETRY := $(BUILD_DIR)/a2fo_telemetry
A2FO_RENDERER_HELPER := $(BUILD_DIR)/A2FORendererHelper.exe
ARCLAB_DIR := tools/A2FOArcLab

# Megadroid's Mission Mod source can either live directly in third_party/mmm
# or in third_party/mmm/source (the layout created by vendor-mmm-source).
MMM_ROOT := third_party/mmm
MMM_SOURCE_ROOT := $(if $(wildcard $(MMM_ROOT)/mmm_armada/mmm_armada.vcxproj),$(MMM_ROOT),$(MMM_ROOT)/source)
MMM_ARMADA_DIR := $(MMM_SOURCE_ROOT)/mmm_armada
MMM_COMMON_DIR := $(MMM_SOURCE_ROOT)/mmm_common
MMM_DATABASE_DIR := $(MMM_SOURCE_ROOT)/mmm_database
MMM_LUAPACK_DIR := $(MMM_SOURCE_ROOT)/mmm_luapack
MMM_LOADER_DIR := $(MMM_SOURCE_ROOT)/mmm_loader
MMM_EXTERNAL_DIR := $(MMM_SOURCE_ROOT)/external
MMM_LUABIND_DIR := $(MMM_EXTERNAL_DIR)/luabind
MMM_LUA_DIR := $(MMM_EXTERNAL_DIR)/lua
MMM_ZLIB_DIR := $(MMM_EXTERNAL_DIR)/zlib
MMM_OBJ_DIR := $(BUILD_DIR)/obj/A2FOMissions
MMM_BOOST_STAMP := $(MMM_OBJ_DIR)/.boost-1.43-complete
MMM_PATCH_STAMP := $(MMM_OBJ_DIR)/.mingw-patched
MMM_SOURCE_PRESENT := $(wildcard $(MMM_ARMADA_DIR)/mmm_armada.vcxproj)

# Keep the project file as the authoritative source list where possible. This
# makes updating the vendored MMM tree much less error-prone than maintaining a
# second hand-written copy of its ~200 Armada binding translation units.
define mmm_project_sources
$(shell sed -n 's@.*<ClCompile Include="\([^"]*\)".*@\1@p' $(1) 2>/dev/null | tr '\\' '/' | sed 's@//*@/@g')
endef

MMM_ARMADA_REL := $(call mmm_project_sources,$(MMM_ARMADA_DIR)/mmm_armada.vcxproj)
MMM_COMMON_REL := $(call mmm_project_sources,$(MMM_COMMON_DIR)/mmm_common.vcxproj)
MMM_DATABASE_REL := $(call mmm_project_sources,$(MMM_DATABASE_DIR)/mmm_database.vcxproj)
MMM_LUAPACK_REL := $(call mmm_project_sources,$(MMM_LUAPACK_DIR)/mmm_luapack.vcxproj)
MMM_LUABIND_REL := $(call mmm_project_sources,$(MMM_LUABIND_DIR)/LuaBind.vcxproj)
MMM_LUA_REL := $(filter-out src/lua.c src/luac.c,$(call mmm_project_sources,$(MMM_LUA_DIR)/lua.vcxproj))

MMM_ARMADA_SOURCES := $(addprefix $(MMM_ARMADA_DIR)/,$(MMM_ARMADA_REL))
MMM_COMMON_SOURCES := $(addprefix $(MMM_COMMON_DIR)/,$(MMM_COMMON_REL))
MMM_DATABASE_SOURCES := $(addprefix $(MMM_DATABASE_DIR)/,$(MMM_DATABASE_REL))
MMM_LUAPACK_SOURCES := $(addprefix $(MMM_LUAPACK_DIR)/,$(MMM_LUAPACK_REL))
MMM_LOADER_SOURCES := $(if $(MMM_SOURCE_PRESENT),$(MMM_LOADER_DIR)/RuleHelper.cpp $(MMM_LOADER_DIR)/Worker.cpp)
MMM_LUABIND_SOURCES := $(addprefix $(MMM_LUABIND_DIR)/,$(MMM_LUABIND_REL))
MMM_LUA_SOURCES := $(addprefix $(MMM_LUA_DIR)/,$(MMM_LUA_REL))
MMM_ZLIB_SOURCES := $(if $(MMM_SOURCE_PRESENT),\
	$(MMM_ZLIB_DIR)/adler32.c \
	$(MMM_ZLIB_DIR)/compress.c \
	$(MMM_ZLIB_DIR)/crc32.c \
	$(MMM_ZLIB_DIR)/deflate.c \
	$(MMM_ZLIB_DIR)/gzclose.c \
	$(MMM_ZLIB_DIR)/gzlib.c \
	$(MMM_ZLIB_DIR)/gzread.c \
	$(MMM_ZLIB_DIR)/gzwrite.c \
	$(MMM_ZLIB_DIR)/infback.c \
	$(MMM_ZLIB_DIR)/inffast.c \
	$(MMM_ZLIB_DIR)/inflate.c \
	$(MMM_ZLIB_DIR)/inftrees.c \
	$(MMM_ZLIB_DIR)/trees.c \
	$(MMM_ZLIB_DIR)/uncompr.c \
	$(MMM_ZLIB_DIR)/zutil.c)
MMM_MINIZIP_SOURCES := $(if $(MMM_SOURCE_PRESENT),\
	$(MMM_ZLIB_DIR)/contrib/minizip/ioapi.c \
	$(MMM_ZLIB_DIR)/contrib/minizip/unzip.c \
	$(MMM_ZLIB_DIR)/contrib/minizip/zip.c)

MMM_ARMADA_OBJECTS := $(patsubst $(MMM_ARMADA_DIR)/%.cpp,$(MMM_OBJ_DIR)/mmm_armada/%.o,$(MMM_ARMADA_SOURCES))
MMM_COMMON_OBJECTS := $(patsubst $(MMM_COMMON_DIR)/%.cpp,$(MMM_OBJ_DIR)/mmm_common/%.o,$(MMM_COMMON_SOURCES))
MMM_DATABASE_OBJECTS := $(patsubst $(MMM_DATABASE_DIR)/%.cpp,$(MMM_OBJ_DIR)/mmm_database/%.o,$(MMM_DATABASE_SOURCES))
MMM_LUAPACK_OBJECTS := $(patsubst $(MMM_LUAPACK_DIR)/%.cpp,$(MMM_OBJ_DIR)/mmm_luapack/%.o,$(MMM_LUAPACK_SOURCES))
MMM_LOADER_OBJECTS := $(patsubst $(MMM_LOADER_DIR)/%.cpp,$(MMM_OBJ_DIR)/mmm_loader/%.o,$(MMM_LOADER_SOURCES))
MMM_LUABIND_OBJECTS := $(patsubst $(MMM_LUABIND_DIR)/src/%.cpp,$(MMM_OBJ_DIR)/luabind/%.o,$(MMM_LUABIND_SOURCES))
MMM_LUA_OBJECTS := $(patsubst $(MMM_LUA_DIR)/src/%.c,$(MMM_OBJ_DIR)/lua/%.o,$(MMM_LUA_SOURCES))
MMM_ZLIB_OBJECTS := $(patsubst $(MMM_ZLIB_DIR)/%.c,$(MMM_OBJ_DIR)/zlib/%.o,$(MMM_ZLIB_SOURCES))
MMM_MINIZIP_OBJECTS := $(patsubst $(MMM_ZLIB_DIR)/contrib/minizip/%.c,$(MMM_OBJ_DIR)/minizip/%.o,$(MMM_MINIZIP_SOURCES))
MMM_OBJECTS := \
	$(MMM_ARMADA_OBJECTS) $(MMM_COMMON_OBJECTS) $(MMM_DATABASE_OBJECTS) \
	$(MMM_LUAPACK_OBJECTS) $(MMM_LOADER_OBJECTS) $(MMM_LUABIND_OBJECTS) \
	$(MMM_LUA_OBJECTS) $(MMM_ZLIB_OBJECTS) $(MMM_MINIZIP_OBJECTS)

# MMM vendors Boost 1.43 but its repository contains a Visual-C++-oriented
# subset. vendor_mmm_boost.py supplements that tree with missing files from the
# official Boost 1.43.0 archive. Keep compiler/stdlib autodetection disabled for
# the 2010-era config layer while building with modern MinGW; the complete
# header tree is still required because generic Boost code includes additional
# preprocessor and smart-pointer backends.
MMM_BOOST_CPPFLAGS ?= -DBOOST_NO_COMPILER_CONFIG -DBOOST_NO_STDLIB_CONFIG -DBOOST_GCC=__GNUC__
MMM_CPPFLAGS := $(CPPFLAGS) -DWIN32 $(MMM_BOOST_CPPFLAGS) -D_HAS_AUTO_PTR_ETC=1 -D_GLIBCXX_USE_DEPRECATED=1 \
	-DBOOST_USE_WINDOWS_H -DLUABIND_CPLUSPLUS_LUA -D_CRT_SECURE_NO_WARNINGS \
	-I$(MMM_SOURCE_ROOT) -I$(MMM_EXTERNAL_DIR) -I$(MMM_LUABIND_DIR) \
	-I$(MMM_LUA_DIR)/src -I$(MMM_ZLIB_DIR)
# The original MMM loader project defines LUABIND_CPLUSPLUS_LUA, but the
# separate LuaBind static-library project does not. Lua itself is compiled from
# .c files as C. Remove the loader-only define when compiling LuaBind or its
# Lua API references acquire C++ linkage and cannot resolve against the C Lua
# objects at the final A2FOMissions link.
MMM_LUABIND_CPPFLAGS := $(filter-out -DLUABIND_CPLUSPLUS_LUA,$(MMM_CPPFLAGS))
MMM_CXXFLAGS := $(filter-out -std=%,$(CXXFLAGS)) -std=gnu++20 \
	-Wno-deprecated-declarations -Wno-unused-parameter
MMM_CFLAGS := -std=gnu99 -O2 -DNDEBUG -DWIN32 -D_CRT_SECURE_NO_WARNINGS \
	-I$(MMM_LUA_DIR)/src -I$(MMM_ZLIB_DIR) -I$(MMM_ZLIB_DIR)/contrib/minizip

BUILTIN_REFIT_SOURCES := core/builtin_camera_controls.cpp core/builtin_view_distance.cpp \
    $(call a2fo_rust_bridge,core/builtin_camera_controls_bridge.S) $(call a2fo_rust_bridge,core/builtin_view_distance_bridge.S)
BUILTIN_REFIT_HEADERS := modules/A2FOCameraControls/module.cpp modules/A2FOViewDistance/module.cpp \
    $(call a2fo_rust_bridge,modules/A2FOCameraControls/native_bridge.S) $(call a2fo_rust_bridge,modules/A2FOViewDistance/native_bridge.S) \
    modules/refit_common/runtime.hpp modules/refit_common/settings.hpp \
    modules/refit_common/bindings.hpp modules/refit_common/status.hpp

BUILTIN_INSTANT_SOURCES := core/builtin_instant_action_settings.cpp \
    modules/A2FOInstantActionSettings/load_button_bounds.cpp \
    modules/A2FOInstantActionSettings/setup_details_line.cpp \
    $(call a2fo_rust_bridge,modules/A2FOInstantActionSettings/thiscall_bridge.S)
BUILTIN_INSTANT_HEADERS := modules/A2FOInstantActionSettings/module.cpp \
    modules/A2FOInstantActionSettings/load_button_bounds.hpp \
    modules/A2FOInstantActionSettings/setup_details_line.hpp

BUILTIN_BATCH7_SOURCES := core/builtin_edit_menu.cpp \
    core/builtin_m3d.cpp \
    core/builtin_breeders.cpp \
    core/builtin_infestation.cpp \
    core/builtin_swarm_system.cpp \
    core/builtin_odf_variants.cpp \
    $(call a2fo_rust_bridge,modules/A2FOEditMenu/thiscall_bridge.S) \
    $(call a2fo_rust_bridge,modules/A2FOM3D/thiscall_bridge.S) \
    $(call a2fo_rust_bridge,modules/A2FOBreeders/thiscall_bridge.S) \
    $(call a2fo_rust_bridge,modules/A2FOInfestation/thiscall_bridge.S) \
    $(call a2fo_rust_bridge,modules/A2FOSwarmSystem/thiscall_bridge.S) \
    $(call a2fo_rust_bridge,modules/A2FOODFVariants/thiscall_bridge.S) \
    modules/A2FOEditMenu/edit_menu_odf.cpp \
    modules/A2FOM3D/m3d_conversion.cpp \
    modules/A2FOSwarmSystem/swarm_motion.cpp
BUILTIN_BATCH7_HEADERS := modules/A2FOEditMenu/module.cpp \
    modules/A2FOM3D/module.cpp \
    modules/A2FOBreeders/module.cpp \
    modules/A2FOInfestation/module.cpp \
    modules/A2FOSwarmSystem/module.cpp \
    modules/A2FOODFVariants/module.cpp \
    modules/A2FOEditMenu/edit_menu_odf.hpp \
    modules/A2FOM3D/m3d_conversion.hpp \
    modules/A2FOSwarmSystem/swarm_motion.hpp

BUILTIN_BATCH8_SOURCES := core/builtin_fire_arcs.cpp \
    core/builtin_animations.cpp \
    core/builtin_always_shields.cpp \
    core/builtin_station_rotation.cpp \
    core/builtin_radar_diagnostics.cpp \
    core/builtin_recovery_tests.cpp \
    modules/A2FOFireArcs/fire_arc.cpp \
    modules/A2FOFireArcs/runtime_config.cpp \
    $(call a2fo_rust_bridge,modules/A2FOFireArcs/thiscall_bridge.S) \
    modules/A2FOAlwaysShowShields/shield_visibility.cpp \
    $(call a2fo_rust_bridge,modules/A2FOAlwaysShowShields/thiscall_bridge.S) \
    $(call a2fo_rust_bridge,modules/A2FOStationRotation/thiscall_bridge.S) \
    $(call a2fo_rust_bridge,modules/A2FORecoveryTests/native_bridge.S)
BUILTIN_BATCH8_HEADERS := modules/A2FOFireArcs/fire_arc.hpp \
    modules/A2FOFireArcs/runtime_config.hpp \
    modules/A2FOAnimations/api.hpp \
    modules/A2FOAnimations/playback.hpp \
    modules/A2FOAlwaysShowShields/shield_visibility.hpp \
    modules/A2FOStationRotation/rotation.hpp \
    modules/A2FOStationRotation/footprint.hpp \
    modules/A2FOStationRotation/footprint_runtime.inl \
    modules/A2FORecoveryTests/recovery_tests.hpp \
    modules/A2FORecoveryTests/signatures.hpp \
    modules/A2FORecoveryTests/armadal/engine_context.hpp \
    modules/A2FORecoveryTests/armadal/node_transform.hpp \
    modules/A2FORecoveryTests/armadal/entity_registry.hpp \
    modules/A2FOFireArcs/module.cpp \
    modules/A2FOAnimations/module.cpp \
    modules/A2FOAlwaysShowShields/module.cpp \
    modules/A2FOStationRotation/module.cpp \
    modules/A2FORadarDiagnostics/module.cpp \
    modules/A2FORecoveryTests/module.cpp
BUILTIN_RECOVERY_FPFLAGS := -frounding-math -ffp-contract=off -fno-fast-math -mfpmath=387
BUILTIN_RECOVERY_OBJECT := $(BUILD_DIR)/builtin-recovery/recovery_tests.o
$(BUILTIN_RECOVERY_OBJECT): modules/A2FORecoveryTests/recovery_tests.cpp $(BUILTIN_BATCH8_HEADERS)
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror $(BUILTIN_RECOVERY_FPFLAGS) -c -o $@ $<

BUILTIN_BATCH9_SOURCES := core/builtin_startup_modules.cpp core/builtin_startup_policy.cpp \
    core/builtin_cheats.cpp \
    core/builtin_collective_xp.cpp \
    core/builtin_directional_shields.cpp \
    core/builtin_energy_systems.cpp \
    core/builtin_hybrid_build.cpp \
    core/builtin_multi_yard_build.cpp \
    core/builtin_nebula_renderer.cpp \
    core/builtin_point_defense_cycles.cpp \
    core/builtin_refit_yards.cpp \
    core/builtin_turrets.cpp \
    core/builtin_rgb_textures.cpp \
    renderer/native11/open_read_owner.cpp \
    core/builtin_texture_variants.cpp \
    $(call a2fo_rust_bridge,modules/A2FOCheats/thiscall_bridge.S) \
    modules/A2FOCollectiveXP/collective_xp.cpp \
    $(call a2fo_rust_bridge,modules/A2FOCollectiveXP/native_bridge.S) \
    modules/A2FODirectionalShields/directional_shields.cpp \
    $(call a2fo_rust_bridge,modules/A2FODirectionalShields/thiscall_bridge.S) \
    modules/A2FOEnergySystems/energy_systems.cpp \
    modules/A2FOEnergySystems/resupply_policy.cpp \
    $(call a2fo_rust_bridge,modules/A2FOEnergySystems/thiscall_bridge.S) \
    modules/A2FOHybridBuild/build_submenu_config.cpp \
    modules/A2FOHybridBuild/hybrid_production.cpp \
    core/builtin_hybrid_runtime.cpp \
    $(call a2fo_rust_bridge,core/builtin_hybrid_bridge.S) \
    $(call a2fo_rust_bridge,modules/A2FOMultiYardBuild/bridges.S) \
    modules/A2FONebulaRenderer/art_texture_suffix_config.cpp \
    $(call a2fo_rust_bridge,modules/A2FONebulaRenderer/thiscall_bridge.S) \
    modules/A2FOPointDefenseCycles/firing_cycle.cpp \
    $(call a2fo_rust_bridge,modules/A2FOPointDefenseCycles/thiscall_bridge.S) \
    modules/A2FORefitYards/refit_policy.cpp \
    $(call a2fo_rust_bridge,modules/A2FORefitYards/thiscall_bridge.S) \
    modules/A2FOTurrets/turret_combat.cpp \
    modules/A2FOTurrets/turret_math.cpp \
    $(call a2fo_rust_bridge,modules/A2FOTurrets/thiscall_bridge.S) \
    $(call a2fo_rust_bridge,modules/A2FORGBTextures/thiscall_bridge.S) \
    modules/A2FOTextureVariants/texture_variants.cpp \
    $(call a2fo_rust_bridge,modules/A2FOTextureVariants/thiscall_bridge.S)
BUILTIN_BATCH9_HEADERS := modules/A2FOHybridBuild/hybrid_production_runtime.cpp $(call a2fo_rust_bridge,modules/A2FOHybridBuild/delphi_bridge.S) \
     core/builtin_startup_modules.hpp \
    sdk/include/a2fo_module_provider.hpp \
    modules/A2FOCollectiveXP/collective_xp.hpp \
    modules/A2FOCollectiveXP/queue_resolver.hpp \
    modules/A2FOCollectiveXP/native_layout.hpp \
    modules/A2FODirectionalShields/directional_shields.hpp \
    modules/A2FODirectionalShields/api.hpp \
    modules/A2FOEnergySystems/energy_systems.hpp \
    modules/A2FOEnergySystems/resupply_policy.hpp \
    modules/A2FOHybridBuild/hybrid_production.hpp \
    modules/A2FOHybridBuild/build_submenu_config.hpp \
    modules/A2FOHybridBuild/refit_ui_bridge_api.hpp \
    modules/A2FOHybridBuild/hybrid_production_runtime.hpp \
    modules/A2FOMultiYardBuild/policy.hpp \
    modules/A2FONebulaRenderer/art_texture_suffix_config.hpp \
    modules/A2FOPointDefenseCycles/firing_cycle.hpp \
    modules/A2FORefitYards/docking_transform.hpp \
    modules/A2FORefitYards/refit_policy.hpp \
    modules/A2FOTurrets/turret_math.hpp \
    modules/A2FOTurrets/turret_combat.hpp \
    modules/A2FOTextureVariants/texture_variants.hpp \
    modules/A2FOCheats/module.cpp \
    modules/A2FOCollectiveXP/module.cpp \
    modules/A2FODirectionalShields/module.cpp \
    modules/A2FOEnergySystems/module.cpp \
    modules/A2FOHybridBuild/module.cpp \
    modules/A2FOMultiYardBuild/module.cpp \
    modules/A2FONebulaRenderer/module.cpp \
    modules/A2FOPointDefenseCycles/module.cpp \
    modules/A2FORefitYards/module.cpp \
    modules/A2FOTurrets/module.cpp \
    modules/A2FORGBTextures/module.cpp \
    modules/A2FOTextureVariants/module.cpp

# Final module batch retains original startup selection and native ABI bodies.
BUILTIN_BATCH10_SOURCES := core/builtin_a1_compat.cpp \
    core/builtin_craft_identity.cpp \
    core/builtin_feature_pack.cpp \
    core/builtin_log_mailer.cpp \
    core/builtin_mission_selector.cpp \
    core/builtin_refit_menus.cpp \
    core/builtin_resources.cpp \
    core/builtin_sol_maps.cpp \
    core/builtin_shell_display.cpp \
    core/builtin_squadrons.cpp \
    core/builtin_video.cpp \
    modules/A1Compat/a1_bzn_policy.cpp \
    modules/A1Compat/race_menu_policy.cpp \
    modules/A1Compat/team_color_policy.cpp \
    $(call a2fo_rust_bridge,modules/A1Compat/thiscall_bridge.S) \
    modules/A2FOCraftIdentity/identity_selection.cpp \
    modules/A2FOCraftIdentity/directional_shield_fill.cpp \
    modules/A2FOCraftIdentity/directional_shield_display_config.cpp \
    modules/A2FOCraftIdentity/system_icon_state.cpp \
    modules/A2FOCraftIdentity/extended_weapon_icons.cpp \
    modules/A2FOCraftIdentity/object_editor_state.cpp \
    modules/A2FOCraftIdentity/object_editor.cpp \
    modules/A2FOCraftIdentity/fleetops_object_editor.cpp \
    $(call a2fo_rust_bridge,modules/A2FOCraftIdentity/thiscall_bridge.S) \
    core/builtin_feature_pack_buildyard.cpp \
    core/builtin_feature_pack_hybrid_client.cpp \
    modules/A2FOFeaturePack/refit_queue_bridge_client.cpp \
    core/builtin_feature_pack_queue.cpp \
    core/builtin_feature_pack_upgrade_pods.cpp \
    modules/A2FOFeaturePack/upgrade_pod_config.cpp \
    $(call a2fo_rust_bridge,core/builtin_feature_pack_bridge.S) \
    core/fpq_paths.cpp \
    core/odf_paths.cpp \
    $(call a2fo_rust_bridge,modules/A2FOMissionSelector/thiscall_bridge.S) \
    modules/A2FORefitMenus/presentation.cpp \
    modules/A2FORefitMenus/skin.cpp \
    modules/A2FORefitMenus/skin_native.cpp \
    modules/A2FORefitMenus/menu_scene.cpp \
    modules/A2FORefitMenus/scene_catalogue.cpp \
    modules/A2FORefitMenus/mission_backdrop.cpp \
    modules/A2FORefitMenus/scene_composition.cpp \
    $(call a2fo_rust_bridge,modules/A2FORefitMenus/native_bridge.S) \
    modules/A2FORefitMenus/setup_native.cpp \
    modules/A2FORefitMenus/setup_presentation.cpp \
    modules/A2FORefitMenus/loading_native.cpp \
    modules/A2FORefitMenus/loading_gpu.cpp \
    modules/A2FORefitMenus/loading_content.cpp \
    modules/A2FORefitMenus/loading_presentation.cpp \
    modules/A2FORefitMenus/loading_theme.cpp \
    modules/A2FORefitMenus/setup_skin.cpp \
    modules/A2FOResources/additional_resources.cpp \
    $(call a2fo_rust_bridge,modules/A2FOResources/thiscall_bridge.S) \
    modules/A2FOSOLMaps/sol_format.cpp \
    $(call a2fo_rust_bridge,modules/A2FOSOLMaps/thiscall_bridge.S) \
    modules/A2FOShellDisplay/bink_video.cpp \
    modules/A2FOShellDisplay/native_intro.cpp \
    modules/A2FOShellDisplay/canvas.cpp \
    $(call a2fo_rust_bridge,modules/A2FOShellDisplay/delphi_bridge.S) \
    modules/A2FOSquadrons/squadron_config.cpp \
    modules/A2FOSquadrons/squadron_state.cpp \
    $(call a2fo_rust_bridge,modules/A2FOSquadrons/thiscall_bridge.S) \
    $(call a2fo_rust_bridge,modules/A2FOVideo/delphi_bridge.S)
BUILTIN_BATCH10_HEADERS := core/builtin_feature_pack_symbols.hpp modules/A2FOFeaturePack/hybrid_bridge_client.cpp \
     sdk/include/a2fo_module_provider.hpp \
    modules/A1Compat/a1_bzn_policy.cpp \
    modules/A1Compat/a1_bzn_policy.hpp \
    modules/A1Compat/a1_ui_policy.hpp \
    modules/A1Compat/race_menu_policy.cpp \
    modules/A1Compat/race_menu_policy.hpp \
    modules/A1Compat/team_color_policy.cpp \
    modules/A1Compat/team_color_policy.hpp \
    $(call a2fo_rust_bridge,modules/A1Compat/thiscall_bridge.S) \
    modules/A1Compat/module.cpp \
    modules/A2FOCraftIdentity/identity_selection.hpp \
    modules/A2FOCraftIdentity/identity_selection.cpp \
    modules/A2FOCraftIdentity/directional_shield_fill.hpp \
    modules/A2FOCraftIdentity/directional_shield_fill.cpp \
    modules/A2FOCraftIdentity/directional_shield_display_config.hpp \
    modules/A2FOCraftIdentity/directional_shield_display_config.cpp \
    modules/A2FOCraftIdentity/system_icon_state.cpp \
    $(call a2fo_rust_bridge,modules/A2FOCraftIdentity/thiscall_bridge.S) \
    modules/A2FOCraftIdentity/extended_weapon_icons.hpp \
    modules/A2FOCraftIdentity/extended_weapon_icons.cpp \
    modules/A2FOCraftIdentity/object_editor_state.hpp \
    modules/A2FOCraftIdentity/object_editor_state.cpp \
    modules/A2FOCraftIdentity/object_editor.hpp \
    modules/A2FOCraftIdentity/object_editor.cpp \
    modules/A2FOCraftIdentity/fleetops_object_editor.hpp \
    modules/A2FOCraftIdentity/fleetops_object_editor.cpp \
    modules/A2FOCraftIdentity/selected_panel_anchor.hpp \
    modules/A2FOCraftIdentity/system_icon_state.hpp \
    modules/A2FOCraftIdentity/module.cpp \
    modules/A2FOFeaturePack/hybrid_bridge_client.hpp \
    modules/A2FOFeaturePack/bink_video.hpp \
    modules/A2FOFeaturePack/upgrade_pods.hpp \
    modules/A2FOFeaturePack/queue_enhancement.hpp \
    modules/A2FOFeaturePack/upgrade_pod_config.hpp \
    modules/A2FOFeaturePack/upgrade_pod_config.cpp \
    modules/A2FOFeaturePack/bink_video.cpp \
    modules/A2FOFeaturePack/buildyard_pseudo_technology.cpp \
    modules/A2FOFeaturePack/buildyard_pseudo_technology.hpp \
    $(call a2fo_rust_bridge,modules/A2FOFeaturePack/delphi_bridge.S) \
    modules/A2FOFeaturePack/odf_recursive.cpp \
    modules/A2FOFeaturePack/queue_enhancement.cpp \
    modules/A2FOFeaturePack/refit_queue_bridge_api.hpp \
    modules/A2FOFeaturePack/refit_queue_bridge_client.cpp \
    modules/A2FOFeaturePack/refit_queue_bridge_client.hpp \
    modules/A2FOFeaturePack/upgrade_pods.cpp \
    modules/A2FOFeaturePack/hybrid_bridge_api.hpp \
    core/builtin_feature_pack_hybrid_client.cpp \
    modules/A2FOFeaturePack/buildyard_tooltip_native.hpp \
    modules/A2FOFeaturePack/buildyard_tree_signatures.hpp \
    modules/A2FOFeaturePack/module_technology_policy.hpp \
    modules/A2FOFeaturePack/buildyard_tree_runtime.inl \
    modules/A2FOLogMailer/report.hpp \
    modules/A2FOLogMailer/config.hpp \
    modules/A2FOLogMailer/module.cpp \
    modules/A2FOLogMailer/helper.cpp \
    modules/A2FOLogMailer/distribution.hpp \
    modules/A2FOMissionSelector/module.cpp \
    $(call a2fo_rust_bridge,modules/A2FOMissionSelector/thiscall_bridge.S) \
    modules/A2FOMissionSelector/standard_buttons.inl \
    modules/A2FOMissions/module.cpp \
    modules/A2FOMissions/fault_diagnostics.hpp \
    modules/A2FORefitMenus/presentation.hpp \
    modules/A2FORefitMenus/presentation.cpp \
    modules/A2FORefitMenus/bindings.hpp \
    $(call a2fo_rust_bridge,modules/A2FORefitMenus/native_bridge.S) \
    modules/A2FORefitMenus/interaction.hpp \
    modules/A2FORefitMenus/status.hpp \
    modules/A2FORefitMenus/native_model.hpp \
    modules/A2FORefitMenus/module.cpp \
    modules/A2FORefitMenus/profile.hpp \
    modules/A2FORefitMenus/skin.hpp \
    modules/A2FORefitMenus/skin.cpp \
    modules/A2FORefitMenus/skin_bindings.hpp \
    modules/A2FORefitMenus/skin_native.hpp \
    modules/A2FORefitMenus/skin_native.cpp \
    modules/A2FORefitMenus/scene_composition.hpp \
    modules/A2FORefitMenus/scene_composition.cpp \
    modules/A2FORefitMenus/menu_scene.hpp \
    modules/A2FORefitMenus/scene_budget.hpp \
    modules/A2FORefitMenus/scene_mailbox.hpp \
    modules/A2FORefitMenus/menu_scene.cpp \
    modules/A2FORefitMenus/scene_settings.hpp \
    modules/A2FORefitMenus/setup_presentation.hpp \
    modules/A2FORefitMenus/setup_presentation.cpp \
    modules/A2FORefitMenus/setup_native.hpp \
    modules/A2FORefitMenus/setup_native.cpp \
    modules/A2FORefitMenus/setup_skin.hpp \
    modules/A2FORefitMenus/setup_skin.cpp \
    modules/A2FORefitMenus/loading_presentation.hpp \
    modules/A2FORefitMenus/loading_presentation.cpp \
    modules/A2FORefitMenus/loading_native.hpp \
    modules/A2FORefitMenus/loading_bindings.hpp \
    modules/A2FORefitMenus/loading_gpu.hpp \
    modules/A2FORefitMenus/loading_gpu.cpp \
    modules/A2FORefitMenus/loading_content.hpp \
    modules/A2FORefitMenus/loading_content.cpp \
    modules/A2FORefitMenus/loading_native.cpp \
    modules/A2FORefitMenus/loading_theme.hpp \
    modules/A2FORefitMenus/loading_theme.cpp \
    modules/A2FORefitMenus/mission_backdrop.hpp \
    modules/A2FORefitMenus/mission_backdrop.cpp \
    modules/A2FORefitMenus/scene_catalogue.hpp \
    modules/A2FORefitMenus/scene_catalogue.cpp \
    modules/A2FORefitMenus/scene_handover.hpp \
    modules/A2FORefitMenus/scene_transition.hpp \
    modules/A2FOResources/additional_resources.hpp \
    modules/A2FOResources/additional_resources.cpp \
    $(call a2fo_rust_bridge,modules/A2FOResources/thiscall_bridge.S) \
    modules/A2FOResources/api.hpp \
    modules/A2FOResources/module.cpp \
    $(call a2fo_rust_bridge,modules/A2FOSOLMaps/thiscall_bridge.S) \
    modules/A2FOSOLMaps/sol_format.cpp \
    modules/A2FOSOLMaps/sol_format.hpp \
    modules/A2FOSOLMaps/module.cpp \
    modules/A2FOShellDisplay/policy.hpp \
    $(call a2fo_rust_bridge,modules/A2FOShellDisplay/delphi_bridge.S) \
    modules/A2FOShellDisplay/module.cpp \
    modules/A2FOShellDisplay/canvas.hpp \
    modules/A2FOShellDisplay/canvas.cpp \
    modules/A2FOShellDisplay/presentation.hpp \
    modules/A2FOShellDisplay/bink_video.cpp \
    modules/A2FOShellDisplay/bink_video.hpp \
    modules/A2FOShellDisplay/native_intro.hpp \
    modules/A2FOShellDisplay/native_intro.cpp \
    modules/A2FOSquadrons/squadron_config.hpp \
    modules/A2FOSquadrons/squadron_config.cpp \
    modules/A2FOSquadrons/squadron_state.hpp \
    modules/A2FOSquadrons/squadron_state.cpp \
    $(call a2fo_rust_bridge,modules/A2FOSquadrons/thiscall_bridge.S) \
    modules/A2FOSquadrons/module.cpp \
    modules/A2FOSquadrons/native_repair.inl \
    modules/A2FOSquadrons/api.hpp \
    modules/A2FOSquadrons/native_economics.inl \
    modules/A2FOSquadrons/native_pod_selection.inl \
    modules/A2FOVideo/module.cpp \
    $(call a2fo_rust_bridge,modules/A2FOVideo/delphi_bridge.S) \
    rust/media_ffi.h
BUILTIN_BATCH10_MISSIONS_OBJECT := $(BUILD_DIR)/builtin-missions/module.o
BUILTIN_BATCH10_RESOURCES := $(BUILD_DIR)/refit_menu_fonts.o $(BUILD_DIR)/refit_menu_ui_skin.o
BUILTIN_BATCH10_SUPPORT_OBJECTS := $(BUILTIN_BATCH10_MISSIONS_OBJECT) $(MMM_OBJECTS) $(BUILTIN_BATCH10_RESOURCES)
BUILTIN_BATCH10_LIBS := -lcomctl32 -ldxguid -lmsimg32 -lgdiplus -lversion -lws2_32 -lshell32 -lwinmm -lole32 -luser32 $(RUST_WIN_SYSTEM_LIBS)
$(BUILTIN_BATCH10_MISSIONS_OBJECT): core/builtin_missions.cpp modules/A2FOMissions/module.cpp modules/A2FOMissions/fault_diagnostics.hpp rust/mmm_runtime_ffi.h sdk/include/a2fo_module_api.h $(MMM_PATCH_STAMP)
	@mkdir -p $(dir $@)
	$(CXX) $(MMM_CPPFLAGS) $(MMM_CXXFLAGS) -include $(MMM_LOADER_DIR)/pch.h -MMD -MP -MF $@.d -c -o $@ $<
-include $(BUILTIN_BATCH10_MISSIONS_OBJECT).d

CORE_SOURCES := \
	core/dllmain.cpp \
	core/builtin_state.cpp \
	core/builtin_modules.cpp \
	core/builtin_wreckage.cpp \
	core/builtin_normal_weapon_tech.cpp \
	core/builtin_build_tooltips.cpp \
	core/builtin_info_ini.cpp \
	core/builtin_muzzle_flashes.cpp \
	core/builtin_team_change_weapons.cpp \
	core/builtin_weapon_damage_controls.cpp \
	core/builtin_a1_fallbacks.cpp \
	core/builtin_animated_hardpoints.cpp \
	$(BUILTIN_REFIT_SOURCES) \
	$(BUILTIN_INSTANT_SOURCES) \
	$(BUILTIN_BATCH7_SOURCES) $(BUILTIN_BATCH8_SOURCES) $(BUILTIN_BATCH9_SOURCES) $(BUILTIN_BATCH10_SOURCES) $(BUILTIN_BATCH10_SUPPORT_OBJECTS) $(BUILTIN_RECOVERY_OBJECT) \
	modules/A1Fallbacks/fallback_policy.cpp \
	$(call a2fo_rust_bridge,modules/A1Fallbacks/thiscall_bridge.S) \
	$(call a2fo_rust_bridge,modules/A2FOAnimatedHardpoints/thiscall_bridge.S) \
	$(call a2fo_rust_bridge,modules/A2FOTeamChangeWeapons/thiscall_bridge.S) \
	$(call a2fo_rust_bridge,modules/A2FOWeaponDamageControls/thiscall_bridge.S) \
	$(call a2fo_rust_bridge,modules/A2FOMuzzleFlashes/thiscall_bridge.S) \
	modules/A2FOWreckage/wreckage_policy.cpp \
	$(call a2fo_rust_bridge,modules/A2FONormalWeaponTech/delphi_bridge.S) \
	modules/A2FOBuildTooltips/build_time_text.cpp \
	$(call a2fo_rust_bridge,modules/A2FOBuildTooltips/thiscall_bridge.S) \
	core/extension_roots.cpp \
	core/module_menu.cpp \
	core/module_loader.cpp \
	core/module_policy.cpp \
	core/renderer_options.cpp \
	core/game_monitor.cpp \
	core/game_monitor_policy.cpp \
	core/nebula_emissive.cpp \
	core/nebula_renderer.cpp \
	core/native11_capture.cpp \
	renderer/native11/world_model_association.cpp \
	core/native11_capture_bridge.S \
	core/native11_sprites.cpp \
	core/native11_sprite_bridge.S \
	renderer/native11/sprite_observation.cpp \
	renderer/native11/sprite_colour_policy.cpp \
	core/native11_lifecycle.cpp \
	core/native11_lifecycle_bridge.S \
	core/native11_assets.cpp \
	core/native11_fofs_stream.cpp \
	core/native11_asset_bridge.S \
	renderer/native11/opened_file.cpp \
	renderer/native11/model_store.cpp \
	renderer/native11/asset_snapshot.cpp \
	renderer/native11/model_dependencies.cpp \
	renderer/native11/dependency_plan.cpp \
	core/native11_textures.cpp \
	core/native11_texture_bridge.S \
	core/native11_tga_bridge.S \
	core/native11_nodes.cpp \
	core/native11_animation_clock.cpp \
	core/native11_template_fleet.cpp \
	renderer/native11/craft_view_policy.cpp \
	renderer/native11/craft_view_capture.cpp \
	renderer/native11/world_view_capture.cpp \
	renderer/native11/world_preparation_capture.cpp \
	renderer/native11/world_prepared_visibility.cpp \
	renderer/native11/craft_visibility_capture.cpp \
	renderer/native11/craft_visibility.cpp \
	renderer/native11/prepared_craft_visibility_capture.cpp \
	core/native11_craft_visibility_base_source.cpp \
	renderer/native11/craft_preparation_capture.cpp \
	renderer/native11/craft_preparation.cpp \
	core/native11_craft_preparation_source.cpp \
	renderer/native11/native_arithmetic.cpp \
	renderer/native11/model_node_template.cpp \
	renderer/native11/model_node_template_cache.cpp \
	renderer/native11/model_material_baseline.cpp \
	renderer/native11/model_material_baseline_cache.cpp \
	renderer/native11/world_borg_materials.cpp \
	renderer/native11/craft_borg_material_capture.cpp \
	renderer/native11/craft_borg_material_policy.cpp \
	core/native11_borg_material_source.cpp \
	renderer/native11/model_lod_template.cpp \
	renderer/native11/model_lod_context.cpp \
	renderer/native11/native_matrix34.cpp \
	renderer/native11/model_pose_snapshot.cpp \
	renderer/native11/owned_animation_pose.cpp \
	renderer/native11/runtime_pose_channels.cpp \
	renderer/native11/animation_channel.cpp \
	renderer/native11/instance_animation_time.cpp \
	renderer/native11/animation_clock_capture.cpp \
	renderer/native11/animation_phase.cpp \
	core/native11_node_bridge.S \
	renderer/native11/node_state.cpp \
	core/native11_materials.cpp \
	core/native11_light_classes.cpp \
	renderer/native11/class_update_barrier.cpp \
	core/native11_world_lights.cpp \
	core/native11_world_pipeline.cpp \
	core/native11_preview_export.cpp \
	renderer/native11/preview_export.cpp \
	renderer/native11/world_light_capture.cpp \
	renderer/native11/world_coverage.cpp \
	renderer/native11/readiness_report.cpp \
	renderer/native11/observed_world.cpp \
	renderer/native11/light_class_store.cpp \
	core/native11_material_bridge.S \
	renderer/native11/material_observation.cpp \
	renderer/native11/material_snapshot.cpp \
	renderer/native11/material_stream.cpp \
	renderer/native11/light_config.cpp \
	renderer/native11/light_event.cpp \
	renderer/native11/world_lighting.cpp \
	renderer/native11/lighting_service.cpp \
	renderer/native11/world_service.cpp \
	renderer/native11/world_session.cpp \
	renderer/native11/capture_service.cpp \
	renderer/native11/texture_store.cpp \
	renderer/native11/scene_capture.cpp \
	renderer/native11/engine_camera.cpp \
	renderer/cloak/refraction.cpp \
	renderer/shadows/pass.cpp \
	renderer/d3d8_compat/observer.cpp \
	renderer/refit/startup.cpp \
	renderer/refit/workspace_bridge.S \
	core/refit_scene.cpp \
	core/refit_mesh_capture.cpp \
	core/refit_mesh_bridge.S \
	core/refit_scene_test.cpp \
	renderer/refit/packet.cpp \
	renderer/refit/resources.cpp \
	renderer/refit/mesh_scope.cpp \
	renderer/refit/geometry_parity.cpp \
	renderer/refit/lights.cpp \
	renderer/native11/model_geometry_policy.cpp \
	$(D3D8_SHADER_SOURCES) \
	renderer/shaders/vs11.cpp \
	renderer/d3d11/probe_device.cpp \
	renderer/d3d11/probe_bridge.cpp \
	$(D3D11_UI_SOURCES) \
	$(D3D8_RESOURCE_SOURCES) \
	$(D3D11_GAME_SOURCES) \
	$(call a2fo_rust_bridge,core/nebula_renderer_bridge.S) \
	$(call a2fo_rust_bridge,core/delphi_bridge.S)

# Keep link order identical to CORE_SOURCES and share the already checked
# builtin objects with headless fixtures. Renderer/core-only TUs compile once.
CORE_SHARED_SOURCES := $(filter core/builtin_%.cpp core/builtin_%.S modules/% core/module_loader.cpp core/module_policy.cpp core/extension_roots.cpp,$(CORE_SOURCES))
CORE_OBJECTS := $(foreach source,$(CORE_SOURCES),$(if $(filter %.o,$(source)),$(source),$(if $(filter $(source),$(CORE_SHARED_SOURCES)),$(BUILD_DIR)/builtin-native/$(source).o,$(BUILD_DIR)/core-objects/$(source).o))) $(RUST_WIN_LIB)
$(BUILD_DIR)/core-objects/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -MF $@.d -c -o $@ $<
$(BUILD_DIR)/core-objects/%.S.o: %.S
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -MF $@.d -c -o $@ $<
-include $(addsuffix .d,$(filter $(BUILD_DIR)/core-objects/%,$(CORE_OBJECTS)))

.PHONY: all release sta1-classic verify-sta1-classic sdk-examples clean \
	verify verify-sdk test smoke odf-module-smoke extension-root-smoke \
	arclab arclab-test telemetry missions vendor-mmm-source vendor-mmm-boost check-mmm-source patch-mmm-source \
	recovery-tests recovery-tests-signatures

all: release

arclab:
	cargo build --release --locked --manifest-path $(ARCLAB_DIR)/Cargo.toml

arclab-test:
	cargo test --locked --manifest-path $(ARCLAB_DIR)/Cargo.toml

telemetry: $(A2FO_TELEMETRY)

ifeq ($(strip $(MMM_SOURCE_PRESENT)),)
# The MMM source lists are read while GNU make parses this file.  If the source
# is missing, vendor it first and recurse so the second make process sees the
# newly populated Visual Studio project files.
missions: vendor-mmm-source
	@$(MAKE) missions
else
missions: $(MISSIONS_MODULE) $(MMM_LOADER_SHIM)
endif

vendor-mmm-source:
	python3 tools/vendor_mmm_source.py
	python3 tools/vendor_mmm_boost.py "$(MMM_ROOT)/source"
	python3 tools/patch_mmm_source.py "$(MMM_ROOT)/source"
	@echo "MMM source is ready under $(MMM_ROOT)/source."


check-mmm-source:
	@test -f "$(MMM_ARMADA_DIR)/mmm_armada.vcxproj" || { \
		echo "MMM source not found under third_party/mmm or third_party/mmm/source."; \
		echo "Run 'make vendor-mmm-source' once, then rerun ./build.sh."; \
		exit 1; \
	}
	@test -f "$(MMM_COMMON_DIR)/mmm_common.vcxproj" \
		-a -f "$(MMM_DATABASE_DIR)/mmm_database.vcxproj" \
		-a -f "$(MMM_LUAPACK_DIR)/mmm_luapack.vcxproj" \
		-a -f "$(MMM_LUAPACK_DIR)/LuaPack.h" \
		-a -f "$(MMM_LUAPACK_DIR)/Pack.cpp" \
		-a -f "$(MMM_COMMON_DIR)/Externals.h" \
		-a -f "$(MMM_COMMON_DIR)/mmm_common.cpp" \
		-a -f "$(MMM_COMMON_DIR)/ModFolder.cpp" \
		-a -f "$(MMM_ARMADA_DIR)/Type_ST3D_GraphicsEngine_Configuration.h" \
		-a -f "$(MMM_ARMADA_DIR)/Type_ST3D_DisplayMode.h" \
		-a -f "$(MMM_ARMADA_DIR)/Type_ST3D_AnimationChannel.h" \
		-a -f "$(MMM_ARMADA_DIR)/Externals.h" \
		-a -f "$(MMM_ARMADA_DIR)/Type_Entity.cpp" \
		-a -f "$(MMM_ARMADA_DIR)/Type_GameObjectClass.cpp" \
		-a -f "$(MMM_ARMADA_DIR)/Type_DebriefingData.h" \
		-a -f "$(MMM_LOADER_DIR)/Worker.cpp" \
		-a -f "$(MMM_LOADER_DIR)/RuleHelper.cpp" \
		-a -f "$(MMM_LUABIND_DIR)/LuaBind.vcxproj" \
		-a -f "$(MMM_LUA_DIR)/lua.vcxproj" \
		-a -f "$(MMM_ZLIB_DIR)/zlib.h" \
		-a -f "$(MMM_EXTERNAL_DIR)/boost/smart_ptr/detail/sp_counted_base.hpp" \
		-a -f "$(MMM_EXTERNAL_DIR)/boost/smart_ptr/detail/sp_counted_base_w32.hpp" \
		-a -f "$(MMM_EXTERNAL_DIR)/boost/smart_ptr/detail/spinlock.hpp" \
		-a -f "$(MMM_EXTERNAL_DIR)/boost/smart_ptr/detail/spinlock_w32.hpp" || { \
		echo "MMM source tree is incomplete: $(MMM_SOURCE_ROOT)"; \
		echo "Re-run 'make vendor-mmm-source' after moving/removing the incomplete source tree."; \
		exit 1; \
	}

vendor-mmm-boost: check-mmm-source
	python3 tools/vendor_mmm_boost.py "$(MMM_SOURCE_ROOT)"

$(MMM_BOOST_STAMP): tools/vendor_mmm_boost.py \
		$(MMM_EXTERNAL_DIR)/boost/version.hpp | check-mmm-source
	mkdir -p $(dir $@)
	python3 tools/vendor_mmm_boost.py "$(MMM_SOURCE_ROOT)"
	touch $@

$(MMM_PATCH_STAMP): tools/patch_mmm_source.py $(MMM_BOOST_STAMP) \
		$(MMM_LUAPACK_DIR)/Node.h \
		$(MMM_LUAPACK_DIR)/LuaPack.h \
		$(MMM_LUAPACK_DIR)/Pack.cpp \
		$(MMM_COMMON_DIR)/Externals.h \
		$(MMM_COMMON_DIR)/mmm_common.cpp \
		$(MMM_COMMON_DIR)/ModFolder.cpp \
		$(MMM_ARMADA_DIR)/Race_Internal.cpp \
		$(MMM_ARMADA_DIR)/Type_ST3D_GraphicsEngine_Configuration.h \
		$(MMM_ARMADA_DIR)/Type_ST3D_DisplayMode.h \
		$(MMM_ARMADA_DIR)/Type_ST3D_AnimationChannel.h \
		$(MMM_ARMADA_DIR)/Externals.h \
		$(MMM_ARMADA_DIR)/Type_Entity.cpp \
		$(MMM_ARMADA_DIR)/Type_GameObjectClass.cpp \
		$(MMM_ARMADA_DIR)/Type_DebriefingData.h \
		$(MMM_LOADER_DIR)/Worker.cpp \
		$(MMM_EXTERNAL_DIR)/boost/smart_ptr/detail/sp_counted_base.hpp \
		$(MMM_EXTERNAL_DIR)/boost/smart_ptr/detail/spinlock.hpp | check-mmm-source
	mkdir -p $(dir $@)
	python3 tools/patch_mmm_source.py "$(MMM_SOURCE_ROOT)"
	touch $@

patch-mmm-source: $(MMM_PATCH_STAMP)

$(A2FO_TELEMETRY): tools/a2fo_telemetry.cpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-o $@ tools/a2fo_telemetry.cpp -ldl

$(A2FO_RENDERER_HELPER): tools/a2fo_renderer_helper.cpp \
		core/build_identity.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -static -static-libgcc -static-libstdc++ \
		-o $@ tools/a2fo_renderer_helper.cpp

release: \
	$(BUILD_DIR)/A2FOExtensions.dll \
	$(RUST_MEDIA_DLL) \
	$(BUILD_DIR)/Win2kDisableTaskSwitch.dll \
	$(A2FO_RENDERER_HELPER) \
	$(MODULE_DIR)/A2FOFeaturePack.dll \
	$(MODULE_DIR)/A2FOHybridBuild.dll \
	$(CHEATS_MODULE) \
	$(CRAFT_IDENTITY_MODULE) \
	$(DIRECTIONAL_SHIELDS_MODULE) \
	$(ENERGY_SYSTEMS_MODULE) \
	$(RESOURCES_MODULE) \
	$(MISSION_SELECTOR_MODULE) \
	$(NEBULA_RENDERER_MODULE) \
	$(POINT_DEFENSE_CYCLES_MODULE) \
	$(TEXTURE_VARIANTS_MODULE) \
	$(STA1_COMPAT_MODULE) \
	$(NEBULA_SHADER_ASSETS) \
	$(NEBULA_LICENSE) \
	$(TURRETS_MODULE) \
	$(REFIT_YARDS_MODULE) \
	$(SQUADRONS_MODULE) \
	$(MULTI_YARD_MODULE) \
	$(COLLECTIVE_XP_MODULE) \
	$(SOL_MAPS_MODULE) \
	$(SHELL_DISPLAY_MODULE) \
	$(VIDEO_MODULE) \
	$(BUILD_DIR)/licenses/armada2-refit.txt \
	missions \
	$(MODULE_DIR)/A2FORGBTextures.dll

sta1-classic: release $(STA1_COMPAT_MODULE) $(STA1_CLASSIC_GUI_CFG) \
		$(STA1_CLASSIC_SPRITE_REGISTRY) $(STA1_CLASSIC_MISSION_SELECTOR)
	rm -rf $(STA1_CLASSIC_DIR)/modules
	mkdir -p $(STA1_CLASSIC_DIR)/AI $(STA1_CLASSIC_DIR)/bzn \
		$(STA1_CLASSIC_DIR)/misc $(STA1_CLASSIC_DIR)/odf \
		$(STA1_CLASSIC_DIR)/sod $(STA1_CLASSIC_DIR)/sounds \
		$(STA1_CLASSIC_DIR)/sprites $(STA1_CLASSIC_DIR)/techtree \
		$(STA1_CLASSIC_DIR)/textures
	cp mods/STA1Classic/info.ini $(STA1_CLASSIC_DIR)/info.ini
	cp mods/STA1Classic/a1compat.ini $(STA1_CLASSIC_DIR)/a1compat.ini
	cp $(STA1_CLASSIC_MISSION_SELECTOR) $(STA1_CLASSIC_DIR)/mission_selector.ini
	cp mods/STA1Classic/README.md $(STA1_CLASSIC_DIR)/README.md
	cp $(STA1_CLASSIC_GUI_CFG) $(STA1_CLASSIC_DIR)/misc/
	cp $(STA1_CLASSIC_SPRITE_REGISTRY) $(STA1_CLASSIC_DIR)/sprites/

sdk-examples: $(MODULE_DIR)/ExampleModule.dll

$(BUILD_DIR):
	mkdir -p $@

$(MODULE_DIR):
	mkdir -p $@

$(MISSIONS_DIR):
	mkdir -p $@

# A2FOMissions compiles the MMM runtime into the ordinary A2FO module DLL.
# mmm_loader/Main.cpp is intentionally excluded; modules/A2FOMissions/module.cpp
# supplies its entry points with lazy Worker ownership instead.
$(MMM_OBJ_DIR)/mmm_armada/%.o: $(MMM_ARMADA_DIR)/%.cpp $(MMM_PATCH_STAMP)
	mkdir -p $(dir $@)
	$(CXX) $(MMM_CPPFLAGS) $(MMM_CXXFLAGS) -include $(MMM_ARMADA_DIR)/pch.h -c -o $@ $<

$(MMM_OBJ_DIR)/mmm_common/%.o: $(MMM_COMMON_DIR)/%.cpp $(MMM_PATCH_STAMP)
	mkdir -p $(dir $@)
	$(CXX) $(MMM_CPPFLAGS) $(MMM_CXXFLAGS) -include $(MMM_COMMON_DIR)/pch.h -c -o $@ $<

$(MMM_OBJ_DIR)/mmm_database/%.o: $(MMM_DATABASE_DIR)/%.cpp $(MMM_PATCH_STAMP)
	mkdir -p $(dir $@)
	$(CXX) $(MMM_CPPFLAGS) $(MMM_CXXFLAGS) -c -o $@ $<

$(MMM_OBJ_DIR)/mmm_luapack/%.o: $(MMM_LUAPACK_DIR)/%.cpp $(MMM_PATCH_STAMP)
	mkdir -p $(dir $@)
	$(CXX) $(MMM_CPPFLAGS) $(MMM_CXXFLAGS) -c -o $@ $<

$(MMM_OBJ_DIR)/mmm_loader/%.o: $(MMM_LOADER_DIR)/%.cpp $(MMM_PATCH_STAMP)
	mkdir -p $(dir $@)
	$(CXX) $(MMM_CPPFLAGS) $(MMM_CXXFLAGS) -include $(MMM_LOADER_DIR)/pch.h -c -o $@ $<

$(MMM_OBJ_DIR)/luabind/%.o: $(MMM_LUABIND_DIR)/src/%.cpp $(MMM_PATCH_STAMP) Makefile
	mkdir -p $(dir $@)
	$(CXX) $(MMM_LUABIND_CPPFLAGS) $(MMM_CXXFLAGS) -c -o $@ $<

$(MMM_OBJ_DIR)/lua/%.o: $(MMM_LUA_DIR)/src/%.c $(MMM_PATCH_STAMP)
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(MMM_CFLAGS) -c -o $@ $<

$(MMM_OBJ_DIR)/zlib/%.o: $(MMM_ZLIB_DIR)/%.c $(MMM_PATCH_STAMP)
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(MMM_CFLAGS) -c -o $@ $<

$(MMM_OBJ_DIR)/minizip/%.o: $(MMM_ZLIB_DIR)/contrib/minizip/%.c $(MMM_PATCH_STAMP)
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(MMM_CFLAGS) -c -o $@ $<

$(MISSIONS_MODULE): $(MMM_PATCH_STAMP) modules/A2FOMissions/module.cpp \
		modules/A2FOMissions/fault_diagnostics.hpp \
		modules/A2FOMissions/README.md sdk/include/a2fo_module_api.h \
		$(MMM_OBJECTS) $(RUST_WIN_LIB) rust/mmm_runtime_ffi.h | $(MODULE_DIR)
	$(CXX) $(MMM_CPPFLAGS) $(MMM_CXXFLAGS) $(DLLFLAGS) \
		-include $(MMM_LOADER_DIR)/pch.h -o $@ modules/A2FOMissions/module.cpp \
		$(MMM_OBJECTS) $(RUST_WIN_LINK) -lversion -lws2_32 -lshell32 -luser32

$(MMM_LOADER_SHIM): $(wildcard rust/a2fo_mmm_loader/*.rs) \
		tools/build_rust_mmm_loader.py missions/mmm_loader/mmm_loader.def | $(MISSIONS_DIR)
	python3 tools/build_rust_mmm_loader.py --output $@

$(BUILD_DIR)/A2FOExtensions.dll: $(CORE_OBJECTS) $(RUST_MEDIA_DLL) $(BUILTIN_REFIT_HEADERS) $(BUILTIN_INSTANT_HEADERS) $(BUILTIN_BATCH7_HEADERS) $(BUILTIN_BATCH8_HEADERS) $(BUILTIN_BATCH9_HEADERS) $(BUILTIN_BATCH10_HEADERS) \
		core/builtin_state.hpp core/builtin_modules.hpp core/initialize_boundary.hpp \
		modules/A2FOWreckage/module.cpp modules/A2FOWreckage/wreckage_policy.hpp \
		modules/A2FONormalWeaponTech/module.cpp \
		modules/A2FOBuildTooltips/module.cpp modules/A2FOBuildTooltips/build_time_text.hpp \
		modules/A2FOInfoIni/module.cpp core/extension_roots.hpp \
		modules/A2FOMuzzleFlashes/module.cpp modules/A2FOAnimations/api.hpp \
		modules/A2FOTeamChangeWeapons/module.cpp \
		modules/A2FOWeaponDamageControls/module.cpp modules/A2FOWeaponDamageControls/damage_controls.hpp \
		modules/A2FODirectionalShields/api.hpp \
		modules/A1Fallbacks/module.cpp modules/A1Fallbacks/fallback_policy.hpp \
		modules/A2FOAnimatedHardpoints/module.cpp \
		modules/A2FOResources/api.hpp \
		modules/A2FOLogMailer/config.hpp \
		modules/A2FOLogMailer/distribution.hpp \
		core/build_identity.hpp \
		core/game_monitor.hpp \
		core/game_monitor_policy.hpp \
		core/renderer_draw_policy.hpp \
		core/renderer_cloak_refraction.inl core/renderer_cloak_policy.hpp renderer/cloak/refraction.hpp renderer/cloak/refraction_shader.hpp \
		core/renderer_ingame_shadows.inl renderer/shadows/pass.hpp renderer/shadows/math.hpp renderer/shadows/shader.hpp \
		renderer/d3d8_compat/observer.hpp \
		renderer/d3d8_compat/generated_thunks.inl \
		$(D3D8_SHADER_HEADERS) $(D3D11_UI_HEADERS) \
		$(D3D8_RESOURCE_HEADERS) \
		$(D3D11_GAME_HEADERS) \
		renderer/shaders/vs11.hpp \
		renderer/d3d11/probe_device.hpp \
		renderer/d3d11/probe_bridge.hpp \
		sdk/include/a2fo_supported_armada.hpp | $(BUILD_DIR)
	A2FO_DLL_NATIVE_OBJECTS="$(filter-out $(RUST_WIN_LIB),$(CORE_OBJECTS))" \
	A2FO_DLL_NATIVE_LIBS="-lcomctl32 -lgdi32 -ldxguid -ladvapi32 $(BUILTIN_BATCH10_LIBS)" \
	python3 tools/build_rust_owned_dll.py --output $@

$(BUILD_DIR)/Win2kDisableTaskSwitch.dll: \
		rust/a2fo_startup_proxy/lib.rs rust/a2fo_startup_proxy/tests.rs \
		rust/a2fo_startup_proxy/exports.def tools/build_rust_startup_proxy.py | $(BUILD_DIR)
	python3 tools/build_rust_startup_proxy.py --output $@

$(ALWAYS_SHOW_SHIELDS_MODULE): \
		modules/A2FOAlwaysShowShields/module.cpp \
		modules/A2FOAlwaysShowShields/shield_visibility.cpp \
		modules/A2FOAlwaysShowShields/shield_visibility.hpp \
		$(call a2fo_rust_bridge,modules/A2FOAlwaysShowShields/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h $(RUST_WIN_LIB) $(RUST_FFI_HEADERS) | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOAlwaysShowShields/module.cpp \
		modules/A2FOAlwaysShowShields/shield_visibility.cpp \
		$(call a2fo_rust_bridge,modules/A2FOAlwaysShowShields/thiscall_bridge.S) $(RUST_WIN_LINK)

$(ANIMATIONS_MODULE): modules/A2FOAnimations/module.cpp \
		modules/A2FOAnimations/playback.hpp modules/A2FOAnimations/api.hpp \
		sdk/include/a2fo_module_api.h sdk/include/a2fo_supported_armada.hpp | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) -o $@ modules/A2FOAnimations/module.cpp $(RUST_WIN_LINK)

$(BUILD_DIR)/animations_test: tests/animations_test.cpp modules/A2FOAnimations/playback.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -o $@ tests/animations_test.cpp

$(BUILD_DIR)/animations_native_test.exe: tests/animations_native_test.cpp \
		modules/A2FOAnimations/module.cpp modules/A2FOAnimations/playback.hpp \
		modules/A2FOAnimations/api.hpp sdk/include/a2fo_module_api.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -Wall -Wextra -static -o $@ tests/animations_native_test.cpp $(RUST_WIN_LINK)

$(BUILD_DIR)/animations_runtime_init_smoke.exe: tests/animations_runtime_init_smoke.cpp \
		modules/A2FOAnimations/module.cpp modules/A2FOAnimations/playback.hpp \
		modules/A2FOAnimations/api.hpp sdk/include/a2fo_supported_armada.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -Wall -Wextra -static -o $@ tests/animations_runtime_init_smoke.cpp $(RUST_WIN_LINK)

$(BUILD_DIR)/muzzle_animation_bridge_test.exe: tests/muzzle_animation_bridge_test.cpp \
		modules/A2FOMuzzleFlashes/module.cpp $(call a2fo_rust_bridge,modules/A2FOMuzzleFlashes/thiscall_bridge.S) \
		modules/A2FOAnimations/api.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -Wall -Wextra -static -o $@ \
		tests/muzzle_animation_bridge_test.cpp $(call a2fo_rust_bridge,modules/A2FOMuzzleFlashes/thiscall_bridge.S) $(RUST_WIN_LINK)

.PHONY: animations-test
animations-test: $(BUILD_DIR)/animations_test
	./$(BUILD_DIR)/animations_test

$(ANIMATED_HARDPOINTS_MODULE): modules/A2FOAnimations/api.hpp \
		modules/A2FOAnimatedHardpoints/module.cpp \
		$(call a2fo_rust_bridge,modules/A2FOAnimatedHardpoints/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h \
		sdk/include/a2fo_supported_armada.hpp | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOAnimatedHardpoints/module.cpp \
		$(call a2fo_rust_bridge,modules/A2FOAnimatedHardpoints/thiscall_bridge.S) $(RUST_WIN_LINK)

$(BUILD_TOOLTIPS_MODULE): \
		modules/A2FOBuildTooltips/module.cpp \
		modules/A2FOBuildTooltips/build_time_text.cpp \
		modules/A2FOBuildTooltips/build_time_text.hpp \
		modules/A2FOResources/api.hpp \
		$(call a2fo_rust_bridge,modules/A2FOBuildTooltips/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h \
		sdk/include/a2fo_supported_armada.hpp | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOBuildTooltips/module.cpp \
		modules/A2FOBuildTooltips/build_time_text.cpp \
		$(call a2fo_rust_bridge,modules/A2FOBuildTooltips/thiscall_bridge.S) $(RUST_WIN_LINK)

$(MODULE_DIR)/ExampleModule.dll: \
		sdk/examples/ExampleModule/example_module.cpp \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-Isdk/include \
		-o $@ sdk/examples/ExampleModule/example_module.cpp

$(MODULE_DIR)/A2FOFeaturePack.dll: \
		modules/A2FOFeaturePack/odf_recursive.cpp \
		modules/A2FOFeaturePack/buildyard_pseudo_technology.cpp \
		modules/A2FOFeaturePack/buildyard_pseudo_technology.hpp \
		modules/A2FOFeaturePack/buildyard_tooltip_native.hpp \
		modules/A2FOFeaturePack/module_technology_policy.hpp \
		modules/A2FOFeaturePack/buildyard_tree_runtime.inl \
		modules/A2FOFeaturePack/buildyard_tree_signatures.hpp \
		modules/A2FOFeaturePack/hybrid_bridge_api.hpp \
		modules/A2FOFeaturePack/hybrid_bridge_client.cpp \
		modules/A2FOFeaturePack/hybrid_bridge_client.hpp \
		modules/A2FOFeaturePack/refit_queue_bridge_api.hpp \
		modules/A2FOFeaturePack/refit_queue_bridge_client.cpp \
		modules/A2FOFeaturePack/refit_queue_bridge_client.hpp \
		modules/A2FOFeaturePack/queue_enhancement.cpp \
		modules/A2FOFeaturePack/queue_enhancement.hpp \
		modules/A2FOCollectiveXP/queue_resolver.hpp \
		modules/A2FOFeaturePack/upgrade_pods.cpp \
		modules/A2FOFeaturePack/upgrade_pods.hpp \
		modules/A2FOFeaturePack/upgrade_pod_config.cpp \
		modules/A2FOFeaturePack/upgrade_pod_config.hpp \
		$(call a2fo_rust_bridge,modules/A2FOFeaturePack/delphi_bridge.S) \
		core/fpq_paths.cpp core/fpq_paths.hpp \
		core/odf_paths.cpp core/odf_paths.hpp \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ \
		modules/A2FOFeaturePack/odf_recursive.cpp \
		modules/A2FOFeaturePack/buildyard_pseudo_technology.cpp \
		modules/A2FOFeaturePack/hybrid_bridge_client.cpp \
		modules/A2FOFeaturePack/refit_queue_bridge_client.cpp \
		modules/A2FOFeaturePack/queue_enhancement.cpp \
		modules/A2FOFeaturePack/upgrade_pods.cpp \
		modules/A2FOFeaturePack/upgrade_pod_config.cpp \
		$(call a2fo_rust_bridge,modules/A2FOFeaturePack/delphi_bridge.S) \
		core/fpq_paths.cpp core/odf_paths.cpp \
		-lgdi32 $(RUST_WIN_LINK)

$(MODULE_DIR)/A2FOHybridBuild.dll: \
		modules/A2FOHybridBuild/module.cpp \
		$(call a2fo_rust_bridge,modules/A2FOHybridBuild/delphi_bridge.S) \
		modules/A2FOFeaturePack/hybrid_bridge_api.hpp \
		modules/A2FOHybridBuild/refit_ui_bridge_api.hpp \
		modules/A2FOHybridBuild/build_submenu_config.cpp \
		modules/A2FOHybridBuild/build_submenu_config.hpp \
		modules/A2FOHybridBuild/hybrid_production.cpp \
		modules/A2FOHybridBuild/hybrid_production.hpp \
		modules/A2FOHybridBuild/hybrid_production_runtime.cpp \
		modules/A2FOHybridBuild/hybrid_production_runtime.hpp \
		sdk/include/a2fo_module_api.h $(RUST_WIN_LIB) $(RUST_FFI_HEADERS) | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ \
		modules/A2FOHybridBuild/module.cpp \
		modules/A2FOHybridBuild/build_submenu_config.cpp \
		modules/A2FOHybridBuild/hybrid_production.cpp \
		modules/A2FOHybridBuild/hybrid_production_runtime.cpp \
		$(call a2fo_rust_bridge,modules/A2FOHybridBuild/delphi_bridge.S) $(RUST_WIN_LINK)

$(REFIT_YARDS_MODULE): \
		modules/A2FORefitYards/module.cpp \
		modules/A2FORefitYards/docking_transform.hpp \
		modules/A2FORefitYards/refit_policy.cpp \
		modules/A2FORefitYards/refit_policy.hpp \
		$(call a2fo_rust_bridge,modules/A2FORefitYards/thiscall_bridge.S) \
		modules/A2FOFeaturePack/refit_queue_bridge_api.hpp \
		modules/A2FOHybridBuild/refit_ui_bridge_api.hpp \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FORefitYards/module.cpp \
		modules/A2FORefitYards/refit_policy.cpp \
		$(call a2fo_rust_bridge,modules/A2FORefitYards/thiscall_bridge.S) $(RUST_WIN_LINK)

$(SQUADRONS_MODULE): modules/A2FOSquadrons/native_repair.inl \
		modules/A2FOSquadrons/native_pod_selection.inl \
		modules/A2FOSquadrons/native_economics.inl \
		modules/A2FOSquadrons/api.hpp \
		modules/A2FOFeaturePack/refit_queue_bridge_api.hpp \
		modules/A2FOSquadrons/module.cpp \
		modules/A2FOSquadrons/squadron_config.cpp \
		modules/A2FOSquadrons/squadron_config.hpp \
		modules/A2FOSquadrons/squadron_state.cpp \
		modules/A2FOSquadrons/squadron_state.hpp \
		$(call a2fo_rust_bridge,modules/A2FOSquadrons/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h \
		sdk/include/a2fo_supported_armada.hpp | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ \
		modules/A2FOSquadrons/module.cpp \
		modules/A2FOSquadrons/squadron_config.cpp \
		modules/A2FOSquadrons/squadron_state.cpp \
		$(call a2fo_rust_bridge,modules/A2FOSquadrons/thiscall_bridge.S) $(RUST_WIN_LINK)

$(MULTI_YARD_MODULE): modules/A2FOMultiYardBuild/module.cpp \
		modules/A2FOMultiYardBuild/policy.hpp $(call a2fo_rust_bridge,modules/A2FOMultiYardBuild/bridges.S) \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) -o $@ \
		modules/A2FOMultiYardBuild/module.cpp $(call a2fo_rust_bridge,modules/A2FOMultiYardBuild/bridges.S) $(RUST_WIN_LINK)

$(BUILD_DIR)/multi_yard_policy_test: tests/multi_yard_policy_test.cpp \
		modules/A2FOMultiYardBuild/policy.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $<

$(BUILD_DIR)/multi_yard_native_test.exe: tests/multi_yard_native_test.cpp \
		tests/multi_yard_bridge_fixture.S modules/A2FOMultiYardBuild/module.cpp \
		modules/A2FOMultiYardBuild/policy.hpp $(call a2fo_rust_bridge,modules/A2FOMultiYardBuild/bridges.S) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=gnu++17 -O2 -Wall -Wextra -Wpedantic \
		-static -static-libgcc -static-libstdc++ -o $@ tests/multi_yard_native_test.cpp \
		$(call a2fo_rust_bridge,modules/A2FOMultiYardBuild/bridges.S) tests/multi_yard_bridge_fixture.S $(RUST_WIN_LINK)

$(BUILD_DIR)/multi_yard_queue_load_test.exe: tests/multi_yard_queue_load_test.cpp \
		modules/A2FOFeaturePack/queue_enhancement.cpp \
		modules/A2FOFeaturePack/hybrid_bridge_client.cpp \
		modules/A2FOFeaturePack/refit_queue_bridge_client.cpp $(RUST_WIN_LIB) $(RUST_FFI_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=gnu++17 -O2 -Wall -Wextra -Wpedantic \
		-static -static-libgcc -static-libstdc++ -o $@ tests/multi_yard_queue_load_test.cpp \
		modules/A2FOFeaturePack/hybrid_bridge_client.cpp \
		modules/A2FOFeaturePack/refit_queue_bridge_client.cpp -lgdi32 $(RUST_WIN_LINK)

$(COLLECTIVE_XP_MODULE): modules/A2FOCollectiveXP/module.cpp \
		$(call a2fo_rust_bridge,modules/A2FOCollectiveXP/native_bridge.S) modules/A2FOCollectiveXP/native_layout.hpp \
		$(COLLECTIVE_XP_SOURCES) $(COLLECTIVE_XP_HEADERS) \
		sdk/include/a2fo_module_api.h sdk/include/a2fo_supported_armada.hpp | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) -o $@ \
		modules/A2FOCollectiveXP/module.cpp $(COLLECTIVE_XP_SOURCES) \
		$(call a2fo_rust_bridge,modules/A2FOCollectiveXP/native_bridge.S) $(RUST_WIN_LINK)

.PHONY: collective-xp-test
collective-xp-test: $(BUILD_DIR)/collective_xp_test $(BUILD_DIR)/collective_xp_queue_test
	$(BUILD_DIR)/collective_xp_test
	$(BUILD_DIR)/collective_xp_queue_test

.PHONY: collective-xp-native-test
collective-xp-native-test: $(BUILD_DIR)/collective_xp_bridge_test.exe $(BUILD_DIR)/collective_xp_runtime_test.exe $(BUILD_DIR)/collective_xp_queue_runtime_test.exe
	env -u DISPLAY -u WAYLAND_DISPLAY WINEDEBUG=-all wine $(BUILD_DIR)/collective_xp_bridge_test.exe
	env -u DISPLAY -u WAYLAND_DISPLAY WINEDEBUG=-all wine $(BUILD_DIR)/collective_xp_runtime_test.exe
	env -u DISPLAY -u WAYLAND_DISPLAY WINEDEBUG=-all wine $(BUILD_DIR)/collective_xp_queue_runtime_test.exe

$(BUILD_DIR)/collective_xp_queue_runtime_test.exe: tests/collective_xp_queue_runtime_test.cpp \
		tests/collective_xp_queue_fixture.S modules/A2FOFeaturePack/queue_enhancement.cpp \
		modules/A2FOCollectiveXP/queue_resolver.hpp modules/A2FOFeaturePack/queue_enhancement.hpp \
		modules/A2FOFeaturePack/refit_queue_bridge_client.cpp modules/A2FOFeaturePack/hybrid_bridge_client.cpp $(RUST_WIN_LIB) $(RUST_FFI_HEADERS) | $(BUILD_DIR)
	$(CXX) -std=c++17 -O2 -Wall -Wextra -static -o $@ \
		tests/collective_xp_queue_runtime_test.cpp tests/collective_xp_queue_fixture.S \
		modules/A2FOFeaturePack/refit_queue_bridge_client.cpp modules/A2FOFeaturePack/hybrid_bridge_client.cpp $(RUST_WIN_LINK)

$(BUILD_DIR)/collective_xp_bridge_test.exe: tests/collective_xp_bridge_test.cpp \
		tests/collective_xp_bridge_fixture.S $(call a2fo_rust_bridge,modules/A2FOCollectiveXP/native_bridge.S) | $(BUILD_DIR)
	$(CXX) -std=c++17 -O2 -Wall -Wextra -static -o $@ $^

$(BUILD_DIR)/collective_xp_runtime_test.exe: tests/collective_xp_runtime_test.cpp \
		modules/A2FOCollectiveXP/module.cpp $(call a2fo_rust_bridge,modules/A2FOCollectiveXP/native_bridge.S) \
		modules/A2FOCollectiveXP/native_layout.hpp $(COLLECTIVE_XP_SOURCES) $(COLLECTIVE_XP_HEADERS) \
		sdk/include/a2fo_module_api.h sdk/include/a2fo_supported_armada.hpp | $(BUILD_DIR)
	$(CXX) -std=c++17 -O2 -Wall -Wextra -static -o $@ \
		tests/collective_xp_runtime_test.cpp $(COLLECTIVE_XP_SOURCES) $(call a2fo_rust_bridge,modules/A2FOCollectiveXP/native_bridge.S) $(RUST_WIN_LINK)

$(BUILD_DIR)/collective_xp_test: tests/collective_xp_test.cpp \
		$(COLLECTIVE_XP_SOURCES) $(COLLECTIVE_XP_HEADERS) | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -o $@ \
		tests/collective_xp_test.cpp $(COLLECTIVE_XP_SOURCES) $(RUST_HOST_LINK)

$(BUILD_DIR)/collective_xp_queue_test: tests/collective_xp_queue_test.cpp \
		modules/A2FOCollectiveXP/queue_resolver.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -o $@ $<

$(BREEDERS_MODULE): \
		modules/A2FOBreeders/module.cpp \
		$(call a2fo_rust_bridge,modules/A2FOBreeders/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h $(RUST_WIN_LIB) $(RUST_FFI_HEADERS) | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ \
		modules/A2FOBreeders/module.cpp \
		$(call a2fo_rust_bridge,modules/A2FOBreeders/thiscall_bridge.S) $(RUST_WIN_LINK)

$(INFESTATION_MODULE): \
		modules/A2FOInfestation/module.cpp \
		$(call a2fo_rust_bridge,modules/A2FOInfestation/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h $(RUST_WIN_LIB) $(RUST_FFI_HEADERS) | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOInfestation/module.cpp \
		$(call a2fo_rust_bridge,modules/A2FOInfestation/thiscall_bridge.S) $(RUST_WIN_LINK)


$(SHELL_DISPLAY_MODULE): modules/A2FOShellDisplay/module.cpp \
		modules/refit_common/fleetops_profile.hpp \
		modules/A2FOShellDisplay/policy.hpp \
		modules/A2FOShellDisplay/presentation.hpp \
		modules/A2FOShellDisplay/bink_video.cpp modules/A2FOShellDisplay/bink_video.hpp \
		modules/A2FOShellDisplay/native_intro.cpp modules/A2FOShellDisplay/native_intro.hpp \
		modules/A2FOShellDisplay/canvas.hpp modules/A2FOShellDisplay/canvas.cpp \
		sdk/include/a2fo_shell_canvas.h \
		$(call a2fo_rust_bridge,modules/A2FOShellDisplay/delphi_bridge.S) \
		sdk/include/a2fo_module_api.h sdk/include/a2fo_supported_armada.hpp | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOShellDisplay/module.cpp \
		modules/A2FOShellDisplay/bink_video.cpp modules/A2FOShellDisplay/native_intro.cpp \
		modules/A2FOShellDisplay/canvas.cpp \
		$(call a2fo_rust_bridge,modules/A2FOShellDisplay/delphi_bridge.S) -lgdi32 -luser32 -ladvapi32 -lgdi32 $(RUST_WIN_LINK)

.PHONY: shell-display shell-display-test shell-canvas-test shell-canvas-lifecycle-test shell-canvas-dialog-test shell-canvas-scroll-test shell-intro-test shell-native-intro-test
$(VIDEO_MODULE): modules/A2FOVideo/module.cpp rust/media_ffi.h $(call a2fo_rust_bridge,modules/A2FOVideo/delphi_bridge.S) \
		sdk/include/a2fo_module_api.h sdk/include/a2fo_supported_armada.hpp \
		$(RUST_WIN_LIB) $(RUST_MEDIA_DLL) $(RUST_FFI_HEADERS) | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOVideo/module.cpp $(call a2fo_rust_bridge,modules/A2FOVideo/delphi_bridge.S) \
		$(RUST_WIN_LINK) -lgdi32 -luser32

.PHONY: video video-test
video: $(VIDEO_MODULE)

$(BUILD_DIR)/video_module_test.exe: tests/video_module_test.cpp modules/A2FOVideo/module.cpp rust/media_ffi.h \
		sdk/include/a2fo_module_api.h sdk/include/a2fo_supported_armada.hpp \
		$(call a2fo_rust_bridge,modules/A2FOVideo/delphi_bridge.S) $(RUST_WIN_LIB) $(RUST_MEDIA_DLL) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -UNDEBUG \
		-static -static-libgcc -static-libstdc++ -o $@ $< \
		$(call a2fo_rust_bridge,modules/A2FOVideo/delphi_bridge.S) $(RUST_WIN_LINK) -luser32 -lgdi32

# Standalone dynamic-backend throughput probe; run with wine and a local movie.
$(BUILD_DIR)/video_perf.exe: tests/video_perf.cpp rust/media_ffi.h $(RUST_MEDIA_DLL) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -UNDEBUG \
		-static -static-libgcc -static-libstdc++ -o $@ $< -lkernel32

# Memory-host fixtures never create windows, read input, or emit sound.
# ffmpeg CLI generates independent test fixtures; it is not a runtime dependency.
VIDEO_REFERENCE_MOVIE ?=
.PHONY: video-media-test video-rust-test
video-test: $(BUILD_DIR)/video_module_test.exe $(BUILD_DIR)/video_perf.exe
	python3 tests/video_fixtures.py $(if $(VIDEO_REFERENCE_MOVIE),--reference-movie "$(VIDEO_REFERENCE_MOVIE)")
	env -u DISPLAY -u WAYLAND_DISPLAY WINEDEBUG=-all wine $(BUILD_DIR)/video_module_test.exe
	env -u DISPLAY -u WAYLAND_DISPLAY WINEDEBUG=-all wine $(BUILD_DIR)/video_perf.exe $(RUST_MEDIA_DLL) build/video-fixtures

video-media-test: $(BUILD_DIR)/video_perf.exe
	env -u DISPLAY -u WAYLAND_DISPLAY WINEDEBUG=-all wine $< $(RUST_MEDIA_DLL) build/video-fixtures

video-rust-test:
	A2FO_MEDIA_FIXTURES=$(abspath build/video-fixtures) cargo test --offline --locked --manifest-path rust/a2fo_media/Cargo.toml --target-dir $(BUILD_DIR)/rust-media-host

shell-display: $(SHELL_DISPLAY_MODULE)

$(BUILD_DIR)/shell_display_native_smoke.exe: tests/shell_display_native_smoke.cpp \
		sdk/include/a2fo_module_api.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -static-libgcc -static-libstdc++ -UNDEBUG -o $@ $<

$(BUILD_DIR)/shell_canvas_test.exe: tests/shell_canvas_test.cpp \
		modules/A2FOShellDisplay/canvas.hpp modules/A2FOShellDisplay/policy.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -static-libgcc -static-libstdc++ -UNDEBUG -o $@ $< -lgdi32

shell-canvas-test: $(BUILD_DIR)/shell_canvas_test.exe
	WINEDEBUG=-all wine $(BUILD_DIR)/shell_canvas_test.exe

$(BUILD_DIR)/shell_canvas_lifecycle_test.exe: tests/shell_canvas_lifecycle_test.cpp \
		modules/A2FOShellDisplay/canvas.cpp modules/A2FOShellDisplay/canvas.hpp \
		$(call a2fo_rust_bridge,modules/A2FOShellDisplay/delphi_bridge.S) modules/A2FOShellDisplay/policy.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -static-libgcc -static-libstdc++ \
		-UNDEBUG -o $@ tests/shell_canvas_lifecycle_test.cpp \
		$(call a2fo_rust_bridge,modules/A2FOShellDisplay/delphi_bridge.S) -lgdi32 -luser32

shell-canvas-lifecycle-test: $(BUILD_DIR)/shell_canvas_lifecycle_test.exe
	WINEDEBUG=-all wine $(BUILD_DIR)/shell_canvas_lifecycle_test.exe

$(BUILD_DIR)/shell_canvas_dialog_test.exe: tests/shell_canvas_dialog_test.cpp \
		modules/A2FOShellDisplay/canvas.cpp modules/A2FOShellDisplay/canvas.hpp \
		$(call a2fo_rust_bridge,modules/A2FOShellDisplay/delphi_bridge.S) modules/A2FOShellDisplay/policy.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -static-libgcc -static-libstdc++ \
		-UNDEBUG -o $@ tests/shell_canvas_dialog_test.cpp \
		$(call a2fo_rust_bridge,modules/A2FOShellDisplay/delphi_bridge.S) -lgdi32 -luser32

shell-canvas-dialog-test: $(BUILD_DIR)/shell_canvas_dialog_test.exe
	WINEDEBUG=-all wine $(BUILD_DIR)/shell_canvas_dialog_test.exe

$(BUILD_DIR)/shell_canvas_scroll_test.exe: tests/shell_canvas_scroll_test.cpp \
		modules/A2FOShellDisplay/canvas.cpp modules/A2FOShellDisplay/canvas.hpp \
		$(call a2fo_rust_bridge,modules/A2FOShellDisplay/delphi_bridge.S) modules/A2FOShellDisplay/policy.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -static-libgcc -static-libstdc++ \
		-UNDEBUG -o $@ tests/shell_canvas_scroll_test.cpp \
		$(call a2fo_rust_bridge,modules/A2FOShellDisplay/delphi_bridge.S) -lgdi32 -luser32

shell-canvas-scroll-test: $(BUILD_DIR)/shell_canvas_scroll_test.exe
	WINEDEBUG=-all wine $(BUILD_DIR)/shell_canvas_scroll_test.exe

$(BUILD_DIR)/shell_intro_test.exe: tests/shell_intro_test.cpp \
		modules/A2FOShellDisplay/bink_video.cpp modules/A2FOShellDisplay/bink_video.hpp \
		modules/A2FOShellDisplay/native_intro.cpp modules/A2FOShellDisplay/native_intro.hpp \
		modules/A2FOShellDisplay/presentation.hpp modules/A2FOShellDisplay/policy.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -static-libgcc -static-libstdc++ \
		-UNDEBUG -o $@ tests/shell_intro_test.cpp modules/A2FOShellDisplay/native_intro.cpp -lgdi32 -luser32 $(RUST_WIN_LINK)

shell-intro-test: $(BUILD_DIR)/shell_intro_test.exe
	WINEDEBUG=-all wine $(BUILD_DIR)/shell_intro_test.exe ArmadaL.exe FleetOpsHook.dll

$(BUILD_DIR)/shell_native_intro_test.exe: tests/shell_native_intro_test.cpp \
		modules/A2FOShellDisplay/native_intro.cpp modules/A2FOShellDisplay/native_intro.hpp \
		sdk/include/a2fo_module_api.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -static-libgcc -static-libstdc++ \
		-UNDEBUG -o $@ tests/shell_native_intro_test.cpp -lgdi32 -luser32 $(RUST_WIN_LINK)

shell-native-intro-test: $(BUILD_DIR)/shell_native_intro_test.exe
	WINEDEBUG=-all wine $(BUILD_DIR)/shell_native_intro_test.exe

# Supply Windows paths to mapped reference inputs. This suite does not run
# game startup or original engine methods. See the module README.
shell-display-test: $(SHELL_DISPLAY_MODULE) $(BUILD_DIR)/shell_display_native_smoke.exe \
		shell-canvas-test shell-canvas-lifecycle-test shell-canvas-dialog-test shell-canvas-scroll-test shell-intro-test shell-native-intro-test
	WINEDEBUG=-all wine $(BUILD_DIR)/shell_display_native_smoke.exe \
		$(SHELL_DISPLAY_MODULE) ArmadaL.exe FleetOpsHook.dll

$(SOL_MAPS_MODULE): \
		modules/A2FOSOLMaps/module.cpp \
		modules/A2FOSOLMaps/sol_format.cpp \
		modules/A2FOSOLMaps/sol_format.hpp \
		$(call a2fo_rust_bridge,modules/A2FOSOLMaps/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOSOLMaps/module.cpp \
		modules/A2FOSOLMaps/sol_format.cpp \
		$(call a2fo_rust_bridge,modules/A2FOSOLMaps/thiscall_bridge.S) $(RUST_WIN_LINK)

$(SOL_FORMAT_TEST): tests/sol_format_test.cpp \
		modules/A2FOSOLMaps/sol_format.cpp \
		modules/A2FOSOLMaps/sol_format.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-I. -o $@ tests/sol_format_test.cpp modules/A2FOSOLMaps/sol_format.cpp $(RUST_HOST_LINK)

# A2FORecoveryTests is disabled at runtime unless explicitly enabled in its INI.
# The core provides it in releases; this standalone SDK target remains available.
recovery-tests: $(RECOVERY_TESTS_MODULE)

recovery-tests-signatures:
	python3 tools/verify_recovery_test_signatures.py --exe ArmadaL.exe

$(RECOVERY_TESTS_MODULE): modules/A2FORecoveryTests/module.cpp \
		modules/A2FORecoveryTests/recovery_tests.cpp \
		modules/A2FORecoveryTests/recovery_tests.hpp \
		modules/A2FORecoveryTests/signatures.hpp \
		modules/A2FORecoveryTests/armadal/entity_registry.hpp \
		modules/A2FORecoveryTests/armadal/engine_context.hpp \
		modules/A2FORecoveryTests/armadal/node_transform.hpp \
		$(call a2fo_rust_bridge,modules/A2FORecoveryTests/native_bridge.S) \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -frounding-math \
		-ffp-contract=off -fno-fast-math -mfpmath=387 $(DLLFLAGS) \
		-o $@ modules/A2FORecoveryTests/module.cpp \
		modules/A2FORecoveryTests/recovery_tests.cpp \
		$(call a2fo_rust_bridge,modules/A2FORecoveryTests/native_bridge.S) -ladvapi32 -lgdi32 $(RUST_WIN_LINK)

$(M3D_MODULE): \
		modules/A2FOM3D/module.cpp \
		modules/A2FOM3D/m3d_conversion.cpp \
		modules/A2FOM3D/m3d_conversion.hpp \
		$(call a2fo_rust_bridge,modules/A2FOM3D/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOM3D/module.cpp \
		modules/A2FOM3D/m3d_conversion.cpp \
		$(call a2fo_rust_bridge,modules/A2FOM3D/thiscall_bridge.S) $(RUST_WIN_LINK)

$(M3D_CONVERSION_TEST): tests/m3d_conversion_test.cpp \
		modules/A2FOM3D/m3d_conversion.cpp \
		modules/A2FOM3D/m3d_conversion.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -o $@ \
		tests/m3d_conversion_test.cpp modules/A2FOM3D/m3d_conversion.cpp $(RUST_HOST_LINK)

$(M3D_RUNTIME_HIERARCHY_TEST): tests/m3d_runtime_hierarchy_test.cpp \
		modules/A2FOM3D/module.cpp \
		modules/A2FOM3D/m3d_conversion.cpp \
		modules/A2FOM3D/m3d_conversion.hpp \
		$(call a2fo_rust_bridge,modules/A2FOM3D/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=gnu++17 -O2 -Wall -Wextra -Wpedantic \
		-static -static-libgcc -static-libstdc++ -o $@ \
		tests/m3d_runtime_hierarchy_test.cpp \
		modules/A2FOM3D/m3d_conversion.cpp \
		$(call a2fo_rust_bridge,modules/A2FOM3D/thiscall_bridge.S) $(RUST_WIN_LINK)

$(TEAM_CHANGE_WEAPONS_MODULE): modules/A2FOTeamChangeWeapons/module.cpp \
		$(call a2fo_rust_bridge,modules/A2FOTeamChangeWeapons/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) -o $@ \
		modules/A2FOTeamChangeWeapons/module.cpp \
		$(call a2fo_rust_bridge,modules/A2FOTeamChangeWeapons/thiscall_bridge.S) $(RUST_WIN_LINK)

$(TEAM_CHANGE_WEAPONS_SMOKE): tests/team_change_weapons_smoke.cpp \
		modules/A2FOTeamChangeWeapons/module.cpp \
		$(call a2fo_rust_bridge,modules/A2FOTeamChangeWeapons/thiscall_bridge.S) \
		rust/native_hooks_ffi.h sdk/include/a2fo_module_api.h $(RUST_WIN_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-static -static-libgcc -static-libstdc++ -Wl,--image-base,0x10000000 -o $@ $< \
		$(call a2fo_rust_bridge,modules/A2FOTeamChangeWeapons/thiscall_bridge.S) $(RUST_WIN_LINK)

$(STATION_ROTATION_MODULE): modules/A2FOStationRotation/module.cpp \
		modules/A2FOStationRotation/rotation.hpp \
		modules/A2FOStationRotation/footprint.hpp \
		modules/A2FOStationRotation/footprint_runtime.inl \
		$(call a2fo_rust_bridge,modules/A2FOStationRotation/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h sdk/include/a2fo_supported_armada.hpp | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) -o $@ \
		modules/A2FOStationRotation/module.cpp \
		$(call a2fo_rust_bridge,modules/A2FOStationRotation/thiscall_bridge.S) $(RUST_WIN_LINK)

$(STATION_ROTATION_TEST): tests/station_rotation_test.cpp \
		modules/A2FOStationRotation/rotation.hpp \
		modules/A2FOStationRotation/footprint.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -o $@ $<

$(STATION_ROTATION_SMOKE): tests/station_rotation_smoke.cpp \
		tests/station_rotation_smoke_bridge.S \
		modules/A2FOStationRotation/module.cpp \
		modules/A2FOStationRotation/rotation.hpp \
		modules/A2FOStationRotation/footprint.hpp \
		modules/A2FOStationRotation/footprint_runtime.inl \
		$(call a2fo_rust_bridge,modules/A2FOStationRotation/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-static -static-libgcc -static-libstdc++ -Wl,--image-base,0x10000000 -o $@ $< \
		tests/station_rotation_smoke_bridge.S \
		$(call a2fo_rust_bridge,modules/A2FOStationRotation/thiscall_bridge.S) $(RUST_WIN_LINK)

$(MODULE_DIR)/A2FOInfoIni.dll: \
		modules/A2FOInfoIni/module.cpp \
		core/extension_roots.cpp core/extension_roots.hpp \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOInfoIni/module.cpp core/extension_roots.cpp $(RUST_WIN_LINK)

$(CHEATS_MODULE): \
		modules/A2FOCheats/module.cpp \
		$(call a2fo_rust_bridge,modules/A2FOCheats/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOCheats/module.cpp \
		$(call a2fo_rust_bridge,modules/A2FOCheats/thiscall_bridge.S) $(RUST_WIN_LINK)

$(CRAFT_IDENTITY_MODULE): \
		modules/A2FOCraftIdentity/module.cpp \
		modules/A2FOCraftIdentity/selected_panel_anchor.hpp \
		modules/A2FOCraftIdentity/object_editor.cpp \
		modules/A2FOCraftIdentity/object_editor.hpp \
		modules/A2FOCraftIdentity/fleetops_object_editor.cpp \
		modules/A2FOCraftIdentity/fleetops_object_editor.hpp \
		modules/A2FOCraftIdentity/object_editor_state.cpp \
		modules/A2FOCraftIdentity/object_editor_state.hpp \
		sdk/include/a2fo_shield_values.hpp \
		sdk/include/a2fo_supported_armada.hpp \
		modules/A2FOCraftIdentity/directional_shield_display_config.cpp \
		modules/A2FOCraftIdentity/directional_shield_display_config.hpp \
		modules/A2FOCraftIdentity/directional_shield_fill.cpp \
		modules/A2FOCraftIdentity/directional_shield_fill.hpp \
		modules/A2FOCraftIdentity/extended_weapon_icons.cpp \
		modules/A2FOCraftIdentity/extended_weapon_icons.hpp \
		modules/A2FOCraftIdentity/identity_selection.cpp \
		modules/A2FOCraftIdentity/identity_selection.hpp \
		modules/A2FOCraftIdentity/system_icon_state.cpp \
		modules/A2FOCraftIdentity/system_icon_state.hpp \
		modules/A2FODirectionalShields/api.hpp \
		$(call a2fo_rust_bridge,modules/A2FOCraftIdentity/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOCraftIdentity/module.cpp \
		modules/A2FOCraftIdentity/object_editor.cpp \
		modules/A2FOCraftIdentity/object_editor_state.cpp \
		modules/A2FOCraftIdentity/fleetops_object_editor.cpp \
		modules/A2FOCraftIdentity/directional_shield_display_config.cpp \
		modules/A2FOCraftIdentity/directional_shield_fill.cpp \
		modules/A2FOCraftIdentity/extended_weapon_icons.cpp \
		modules/A2FOCraftIdentity/identity_selection.cpp \
		modules/A2FOCraftIdentity/system_icon_state.cpp \
		$(call a2fo_rust_bridge,modules/A2FOCraftIdentity/thiscall_bridge.S) $(RUST_WIN_LINK)

$(BUILD_DIR)/craft_identity_panel_smoke.exe: tests/craft_identity_panel_smoke.cpp \
		$(CRAFT_IDENTITY_MODULE) rust/native_hooks_ffi.h $(RUST_WIN_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-static -static-libgcc -static-libstdc++ -Wl,--image-base,0x10000000 -o $@ $< \
		$(filter-out modules/A2FOCraftIdentity/module.cpp,\
		$(wildcard modules/A2FOCraftIdentity/*.cpp)) \
		$(call a2fo_rust_bridge,modules/A2FOCraftIdentity/thiscall_bridge.S) $(RUST_WIN_LINK)

$(BUILD_DIR)/craft_identity_ammunition_hover_test.exe: tests/craft_identity_ammunition_hover_test.cpp \
		$(CRAFT_IDENTITY_MODULE) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-static -static-libgcc -static-libstdc++ -Wl,--image-base,0x10000000 -o $@ $< \
		$(filter-out modules/A2FOCraftIdentity/module.cpp,\
		$(wildcard modules/A2FOCraftIdentity/*.cpp)) \
		$(call a2fo_rust_bridge,modules/A2FOCraftIdentity/thiscall_bridge.S)

$(EDIT_MENU_MODULE): \
		modules/A2FOEditMenu/module.cpp \
		modules/A2FOEditMenu/edit_menu_odf.cpp \
		modules/A2FOEditMenu/edit_menu_odf.hpp \
		$(call a2fo_rust_bridge,modules/A2FOEditMenu/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOEditMenu/module.cpp \
		modules/A2FOEditMenu/edit_menu_odf.cpp \
		$(call a2fo_rust_bridge,modules/A2FOEditMenu/thiscall_bridge.S) $(RUST_WIN_LINK)

$(INSTANT_ACTION_SETTINGS_MODULE): \
		modules/A2FOInstantActionSettings/module.cpp \
		modules/A2FOInstantActionSettings/load_button_bounds.cpp \
		modules/A2FOInstantActionSettings/load_button_bounds.hpp \
		modules/A2FOInstantActionSettings/setup_details_line.cpp \
		modules/A2FOInstantActionSettings/setup_details_line.hpp \
		$(call a2fo_rust_bridge,modules/A2FOInstantActionSettings/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h \
		sdk/include/a2fo_supported_armada.hpp | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOInstantActionSettings/module.cpp \
		modules/A2FOInstantActionSettings/load_button_bounds.cpp \
		modules/A2FOInstantActionSettings/setup_details_line.cpp \
		$(call a2fo_rust_bridge,modules/A2FOInstantActionSettings/thiscall_bridge.S) $(RUST_WIN_LINK)

$(MISSION_SELECTOR_MODULE): \
		modules/A2FOMissionSelector/module.cpp \
		$(call a2fo_rust_bridge,modules/A2FOMissionSelector/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h $(RUST_WIN_LIB) $(RUST_FFI_HEADERS) | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOMissionSelector/module.cpp \
		$(call a2fo_rust_bridge,modules/A2FOMissionSelector/thiscall_bridge.S) \
		-lcomctl32 -lgdi32 -lgdiplus $(RUST_WIN_LINK)

$(FIRE_ARCS_MODULE): \
		modules/A2FOFireArcs/module.cpp \
		modules/A2FOFireArcs/fire_arc.cpp \
		modules/A2FOFireArcs/fire_arc.hpp \
		modules/A2FOFireArcs/runtime_config.cpp \
		modules/A2FOFireArcs/runtime_config.hpp \
		$(call a2fo_rust_bridge,modules/A2FOFireArcs/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOFireArcs/module.cpp \
		modules/A2FOFireArcs/fire_arc.cpp \
		modules/A2FOFireArcs/runtime_config.cpp \
		$(call a2fo_rust_bridge,modules/A2FOFireArcs/thiscall_bridge.S) $(RUST_WIN_LINK)

$(ENERGY_SYSTEMS_MODULE): \
		modules/A2FOEnergySystems/module.cpp \
		modules/A2FOEnergySystems/energy_systems.cpp \
		modules/A2FOEnergySystems/energy_systems.hpp \
		modules/A2FOEnergySystems/resupply_policy.cpp \
		modules/A2FOEnergySystems/resupply_policy.hpp \
		$(call a2fo_rust_bridge,modules/A2FOEnergySystems/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h $(RUST_WIN_LIB) $(RUST_FFI_HEADERS) | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOEnergySystems/module.cpp \
		modules/A2FOEnergySystems/energy_systems.cpp \
		modules/A2FOEnergySystems/resupply_policy.cpp \
		$(call a2fo_rust_bridge,modules/A2FOEnergySystems/thiscall_bridge.S) $(RUST_WIN_LINK)

$(BUILD_DIR)/resupply_runtime_test.exe: tests/resupply_runtime_test.cpp \
		$(ENERGY_SYSTEMS_MODULE) $(RUST_WIN_LIB) $(RUST_FFI_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-static -static-libgcc -static-libstdc++ -o $@ $< \
		modules/A2FOEnergySystems/energy_systems.cpp \
		modules/A2FOEnergySystems/resupply_policy.cpp \
		$(call a2fo_rust_bridge,modules/A2FOEnergySystems/thiscall_bridge.S) $(RUST_WIN_LINK)

$(BUILD_DIR)/resupply_runtime_host_test: tests/resupply_runtime_test.cpp \
		modules/A2FOEnergySystems/module.cpp \
		modules/A2FOEnergySystems/energy_systems.cpp \
		modules/A2FOEnergySystems/energy_systems.hpp \
		modules/A2FOEnergySystems/resupply_policy.cpp \
		modules/A2FOEnergySystems/resupply_policy.hpp \
		tests/support/energy_systems_host/windows.h \
		sdk/include/a2fo_module_api.h $(RUST_HOST_LIB) $(RUST_FFI_HEADERS) | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Wno-attributes \
		-Itests/support/energy_systems_host -ffunction-sections -fdata-sections \
		-Wl,--gc-sections -o $@ $< \
		modules/A2FOEnergySystems/energy_systems.cpp \
		modules/A2FOEnergySystems/resupply_policy.cpp $(RUST_HOST_LINK)

$(MUZZLE_FLASHES_MODULE): modules/A2FOAnimations/api.hpp \
		modules/A2FOMuzzleFlashes/module.cpp \
		$(call a2fo_rust_bridge,modules/A2FOMuzzleFlashes/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOMuzzleFlashes/module.cpp \
		$(call a2fo_rust_bridge,modules/A2FOMuzzleFlashes/thiscall_bridge.S) $(RUST_WIN_LINK)

$(DIRECTIONAL_SHIELDS_MODULE): \
		modules/A2FODirectionalShields/module.cpp \
		sdk/include/a2fo_shield_values.hpp \
		modules/A2FODirectionalShields/directional_shields.cpp \
		modules/A2FODirectionalShields/directional_shields.hpp \
		modules/A2FODirectionalShields/api.hpp \
		$(call a2fo_rust_bridge,modules/A2FODirectionalShields/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h $(RUST_WIN_LIB) $(RUST_FFI_HEADERS) | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FODirectionalShields/module.cpp \
		modules/A2FODirectionalShields/directional_shields.cpp \
		$(call a2fo_rust_bridge,modules/A2FODirectionalShields/thiscall_bridge.S) $(RUST_WIN_LINK)

$(WEAPON_DAMAGE_CONTROLS_MODULE): \
		modules/A2FOWeaponDamageControls/module.cpp \
		modules/A2FOWeaponDamageControls/damage_controls.hpp \
		modules/A2FODirectionalShields/api.hpp \
		$(call a2fo_rust_bridge,modules/A2FOWeaponDamageControls/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOWeaponDamageControls/module.cpp \
		$(call a2fo_rust_bridge,modules/A2FOWeaponDamageControls/thiscall_bridge.S) $(RUST_WIN_LINK)

$(NORMAL_WEAPON_TECH_MODULE): \
		modules/A2FONormalWeaponTech/module.cpp \
		$(call a2fo_rust_bridge,modules/A2FONormalWeaponTech/delphi_bridge.S) \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FONormalWeaponTech/module.cpp \
		$(call a2fo_rust_bridge,modules/A2FONormalWeaponTech/delphi_bridge.S) $(RUST_WIN_LINK)

$(NEBULA_RENDERER_MODULE): \
		modules/A2FONebulaRenderer/module.cpp \
		modules/A2FONebulaRenderer/art_texture_suffix_config.cpp \
		modules/A2FONebulaRenderer/art_texture_suffix_config.hpp \
		$(call a2fo_rust_bridge,modules/A2FONebulaRenderer/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FONebulaRenderer/module.cpp \
		modules/A2FONebulaRenderer/art_texture_suffix_config.cpp \
		$(call a2fo_rust_bridge,modules/A2FONebulaRenderer/thiscall_bridge.S) $(RUST_WIN_LINK)

$(POINT_DEFENSE_CYCLES_MODULE): \
		modules/A2FOPointDefenseCycles/module.cpp \
		modules/A2FOPointDefenseCycles/firing_cycle.cpp \
		modules/A2FOPointDefenseCycles/firing_cycle.hpp \
		$(call a2fo_rust_bridge,modules/A2FOPointDefenseCycles/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOPointDefenseCycles/module.cpp \
		modules/A2FOPointDefenseCycles/firing_cycle.cpp \
		$(call a2fo_rust_bridge,modules/A2FOPointDefenseCycles/thiscall_bridge.S) $(RUST_WIN_LINK)

$(SWARM_SYSTEM_MODULE): \
		modules/A2FOSwarmSystem/module.cpp \
		modules/A2FOSwarmSystem/swarm_motion.cpp \
		modules/A2FOSwarmSystem/swarm_motion.hpp \
		$(call a2fo_rust_bridge,modules/A2FOSwarmSystem/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOSwarmSystem/module.cpp \
		modules/A2FOSwarmSystem/swarm_motion.cpp \
		$(call a2fo_rust_bridge,modules/A2FOSwarmSystem/thiscall_bridge.S) $(RUST_WIN_LINK)

$(BUILD_DIR)/Shaders/dx8/pixel/%.nvv: \
		modules/A2FONebulaRenderer/Shaders/dx8/pixel/%.nvv | $(BUILD_DIR)
	mkdir -p $(dir $@)
	cp $< $@

$(BUILD_DIR)/Shaders/dx8/vertex/%.nvv: \
		modules/A2FONebulaRenderer/Shaders/dx8/vertex/%.nvv | $(BUILD_DIR)
	mkdir -p $(dir $@)
	cp $< $@

$(BUILD_DIR)/Shaders/dot3_amd.nvv: \
		modules/A2FONebulaRenderer/Shaders/dot3_amd.nvv | $(BUILD_DIR)
	mkdir -p $(dir $@)
	cp $< $@

$(BUILD_DIR)/Shaders/dot3_amd9.nvv: \
		modules/A2FONebulaRenderer/Shaders/dot3_amd9.nvv | $(BUILD_DIR)
	mkdir -p $(dir $@)
	cp $< $@

$(NEBULA_LICENSE): third_party/armada-nebula-patch/LICENSE.txt | $(BUILD_DIR)
	mkdir -p $(dir $@)
	cp $< $@

$(TURRETS_MODULE): \
		modules/A2FOTurrets/module.cpp \
		modules/A2FOTurrets/turret_combat.cpp \
		modules/A2FOTurrets/turret_combat.hpp \
		modules/A2FOTurrets/turret_math.cpp \
		modules/A2FOTurrets/turret_math.hpp \
		$(call a2fo_rust_bridge,modules/A2FOTurrets/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOTurrets/module.cpp \
		modules/A2FOTurrets/turret_combat.cpp \
		modules/A2FOTurrets/turret_math.cpp \
		$(call a2fo_rust_bridge,modules/A2FOTurrets/thiscall_bridge.S) $(RUST_WIN_LINK)

$(MODULE_DIR)/A2FORGBTextures.dll: \
		modules/A2FORGBTextures/module.cpp \
		$(call a2fo_rust_bridge,modules/A2FORGBTextures/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h \
		sdk/include/a2fo_supported_armada.hpp $(RUST_WIN_LIB) $(RUST_FFI_HEADERS) | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FORGBTextures/module.cpp \
		$(call a2fo_rust_bridge,modules/A2FORGBTextures/thiscall_bridge.S) $(RUST_WIN_LINK)

$(TEXTURE_VARIANTS_MODULE): \
		modules/A2FOTextureVariants/module.cpp \
		modules/A2FOTextureVariants/texture_variants.cpp \
		modules/A2FOTextureVariants/texture_variants.hpp \
		$(call a2fo_rust_bridge,modules/A2FOTextureVariants/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOTextureVariants/module.cpp \
		modules/A2FOTextureVariants/texture_variants.cpp \
		$(call a2fo_rust_bridge,modules/A2FOTextureVariants/thiscall_bridge.S) $(RUST_WIN_LINK)

$(ODF_VARIANTS_MODULE): \
		modules/A2FOODFVariants/module.cpp \
		$(call a2fo_rust_bridge,modules/A2FOODFVariants/thiscall_bridge.S) \
		sdk/include/a2fo_faction_suffix.hpp \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOODFVariants/module.cpp \
		$(call a2fo_rust_bridge,modules/A2FOODFVariants/thiscall_bridge.S) $(RUST_WIN_LINK)

$(RESOURCES_MODULE): \
		modules/A2FOSquadrons/api.hpp \
		modules/A2FOResources/module.cpp \
		modules/A2FOResources/additional_resources.cpp \
		modules/A2FOResources/additional_resources.hpp \
		modules/A2FOResources/api.hpp \
		$(call a2fo_rust_bridge,modules/A2FOResources/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOResources/module.cpp \
		modules/A2FOResources/additional_resources.cpp \
		$(call a2fo_rust_bridge,modules/A2FOResources/thiscall_bridge.S) $(RUST_WIN_LINK)

$(WRECKAGE_MODULE): \
		modules/A2FOWreckage/module.cpp \
		modules/A2FOWreckage/wreckage_policy.cpp \
		modules/A2FOWreckage/wreckage_policy.hpp \
		sdk/include/a2fo_module_api.h $(RUST_WIN_LIB) $(RUST_FFI_HEADERS) | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A2FOWreckage/module.cpp \
		modules/A2FOWreckage/wreckage_policy.cpp $(RUST_WIN_LINK)

$(STA1_COMPAT_MODULE): \
		modules/A1Compat/module.cpp \
		modules/A1Compat/a1_bzn_policy.cpp \
		modules/A1Compat/a1_bzn_policy.hpp \
		modules/A1Compat/a1_ui_policy.hpp \
		modules/A1Compat/race_menu_policy.cpp \
		modules/A1Compat/race_menu_policy.hpp \
		modules/A1Compat/team_color_policy.cpp \
		modules/A1Compat/team_color_policy.hpp \
		$(call a2fo_rust_bridge,modules/A1Compat/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A1Compat/module.cpp \
		modules/A1Compat/a1_bzn_policy.cpp \
		modules/A1Compat/race_menu_policy.cpp \
		modules/A1Compat/team_color_policy.cpp \
		$(call a2fo_rust_bridge,modules/A1Compat/thiscall_bridge.S) $(RUST_WIN_LINK)

$(A1_FALLBACKS_MODULE): \
		modules/A1Fallbacks/module.cpp \
		modules/A1Fallbacks/fallback_policy.cpp \
		modules/A1Fallbacks/fallback_policy.hpp \
		$(call a2fo_rust_bridge,modules/A1Fallbacks/thiscall_bridge.S) \
		sdk/include/a2fo_module_api.h | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) \
		-o $@ modules/A1Fallbacks/module.cpp \
		modules/A1Fallbacks/fallback_policy.cpp \
		$(call a2fo_rust_bridge,modules/A1Fallbacks/thiscall_bridge.S) $(RUST_WIN_LINK)

$(SMOKE_TEST): tests/dll_load_smoke.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -static -static-libgcc -static-libstdc++ \
		-o $@ $<

$(BUILD_DIR)/supported_armada_smoke.exe: tests/supported_armada_smoke.cpp \
		sdk/include/a2fo_supported_armada.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -static -static-libgcc -static-libstdc++ \
		-o $@ $<

$(BUILD_DIR)/roots_engine_identity_smoke.exe: tests/roots_engine_identity_smoke.cpp \
		sdk/include/a2fo_supported_armada.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -static -o $@ $<

$(BUILD_DIR)/roots_profile_smoke.exe: tests/roots_profile_smoke.cpp \
		sdk/include/a2fo_supported_armada.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $<

$(BUILD_DIR)/roots_native_rotation_smoke.exe: tests/roots_native_rotation_smoke.cpp \
		sdk/include/a2fo_module_api.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -static -o $@ $<

$(BUILD_DIR)/roots_collective_xp_smoke.exe: tests/roots_collective_xp_smoke.cpp \
		sdk/include/a2fo_module_api.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -static -o $@ $<

$(BUILD_DIR)/roots_dll_load_smoke.exe: tests/roots_dll_load_smoke.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -static -o $@ $<

$(GAME_MONITOR_SMOKE): tests/game_monitor_win32_smoke.cpp \
		core/game_monitor.cpp core/game_monitor.hpp \
		core/game_monitor_policy.cpp core/game_monitor_policy.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -static -static-libgcc -static-libstdc++ \
		-Icore -o $@ tests/game_monitor_win32_smoke.cpp \
		core/game_monitor.cpp core/game_monitor_policy.cpp $(RUST_WIN_LINK)

$(FPQ_PATHS_TEST): tests/fpq_paths_test.cpp \
		core/fpq_paths.cpp core/fpq_paths.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Icore \
		-o $@ tests/fpq_paths_test.cpp core/fpq_paths.cpp $(RUST_HOST_LINK)

$(ADDITIONAL_RESOURCES_TEST): \
		tests/additional_resources_test.cpp \
		modules/A2FOResources/additional_resources.cpp \
		modules/A2FOResources/additional_resources.hpp \
		modules/A2FOResources/api.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOResources -o $@ \
		tests/additional_resources_test.cpp \
		modules/A2FOResources/additional_resources.cpp $(RUST_HOST_LINK)

$(A1_RACE_MENU_TEST): \
		tests/a1_race_menu_policy_test.cpp \
		modules/A1Compat/race_menu_policy.cpp \
		modules/A1Compat/race_menu_policy.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A1Compat -o $@ \
		tests/a1_race_menu_policy_test.cpp \
		modules/A1Compat/race_menu_policy.cpp $(RUST_HOST_LINK)

$(A1_TEAM_COLOR_TEST): \
		tests/a1_team_color_policy_test.cpp \
		modules/A1Compat/team_color_policy.cpp \
		modules/A1Compat/team_color_policy.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A1Compat -o $@ \
		tests/a1_team_color_policy_test.cpp \
		modules/A1Compat/team_color_policy.cpp $(RUST_HOST_LINK)

$(A1_BZN_POLICY_TEST): \
		tests/a1_bzn_policy_test.cpp \
		modules/A1Compat/a1_bzn_policy.cpp \
		modules/A1Compat/a1_bzn_policy.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A1Compat -o $@ \
		tests/a1_bzn_policy_test.cpp \
		modules/A1Compat/a1_bzn_policy.cpp $(RUST_HOST_LINK)

$(A1_UI_POLICY_TEST): \
		tests/a1_ui_policy_test.cpp \
		modules/A1Compat/a1_ui_policy.hpp rust/stage9_a1_ui_ffi.h $(RUST_HOST_LIB) | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A1Compat -o $@ tests/a1_ui_policy_test.cpp $(RUST_HOST_LINK)

$(A1_FALLBACK_POLICY_TEST): \
		tests/a1_fallback_policy_test.cpp \
		modules/A1Fallbacks/fallback_policy.cpp \
		modules/A1Fallbacks/fallback_policy.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A1Fallbacks -o $@ \
		tests/a1_fallback_policy_test.cpp \
		modules/A1Fallbacks/fallback_policy.cpp $(RUST_HOST_LINK)

$(ODF_PATHS_TEST): tests/odf_paths_test.cpp \
		core/odf_paths.cpp core/odf_paths.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Icore \
		-o $@ tests/odf_paths_test.cpp core/odf_paths.cpp $(RUST_HOST_LINK)

$(EXTENSION_ROOTS_TEST): tests/extension_roots_test.cpp \
		core/extension_roots.cpp core/extension_roots.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Icore \
		-o $@ tests/extension_roots_test.cpp core/extension_roots.cpp $(RUST_HOST_LINK)

$(EXTENSION_ROOT_SMOKE): tests/extension_root_discovery_smoke.cpp \
		core/extension_roots.cpp core/extension_roots.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -static -static-libgcc -static-libstdc++ -Icore \
		-o $@ tests/extension_root_discovery_smoke.cpp \
		core/extension_roots.cpp $(RUST_WIN_LINK)

$(ODF_MODULE_SMOKE): tests/odf_module_init_smoke.cpp \
		sdk/include/a2fo_module_api.h | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -static -static-libgcc -static-libstdc++ \
		-Isdk/include -o $@ $<

$(MODULE_API_TEST): tests/module_api_test.cpp \
		sdk/include/a2fo_module_api.h | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Isdk/include -o $@ $<

$(MODULE_POLICY_TEST): tests/module_policy_test.cpp \
		core/module_policy.cpp core/module_policy.hpp $(RUST_HOST_LIB) | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Icore \
		-o $@ tests/module_policy_test.cpp core/module_policy.cpp $(RUST_HOST_LINK)

$(HYBRID_PRODUCTION_TEST): tests/hybrid_production_test.cpp \
		modules/A2FOHybridBuild/hybrid_production.cpp \
		modules/A2FOHybridBuild/hybrid_production.hpp $(RUST_HOST_LIB) $(RUST_FFI_HEADERS) | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOHybridBuild -o $@ \
		tests/hybrid_production_test.cpp \
		modules/A2FOHybridBuild/hybrid_production.cpp $(RUST_HOST_LINK)

$(BUILD_SUBMENU_CONFIG_TEST): tests/build_submenu_config_test.cpp \
		modules/A2FOHybridBuild/build_submenu_config.cpp \
		modules/A2FOHybridBuild/build_submenu_config.hpp $(RUST_HOST_LIB) $(RUST_FFI_HEADERS) | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOHybridBuild -o $@ \
		tests/build_submenu_config_test.cpp \
		modules/A2FOHybridBuild/build_submenu_config.cpp $(RUST_HOST_LINK)

$(TURRET_MATH_TEST): tests/turret_math_test.cpp \
		modules/A2FOTurrets/turret_combat.cpp \
		modules/A2FOTurrets/turret_combat.hpp \
		modules/A2FOTurrets/turret_math.cpp \
		modules/A2FOTurrets/turret_math.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOTurrets -o $@ \
		tests/turret_math_test.cpp \
		modules/A2FOTurrets/turret_combat.cpp \
		modules/A2FOTurrets/turret_math.cpp $(RUST_HOST_LINK)

$(BUILD_DIR)/directional_shield_layout_test: tests/directional_shield_layout_test.cpp \
		modules/A2FOCraftIdentity/directional_shield_display_config.cpp \
		modules/A2FOCraftIdentity/directional_shield_display_config.hpp \
		modules/A2FOCraftIdentity/directional_shield_fill.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-o $@ tests/directional_shield_layout_test.cpp \
		modules/A2FOCraftIdentity/directional_shield_display_config.cpp $(RUST_HOST_LINK)

$(CRAFT_IDENTITY_TEST): tests/craft_identity_test.cpp \
		modules/A2FOCraftIdentity/selected_panel_anchor.hpp \
		modules/A2FOCraftIdentity/directional_shield_display_config.cpp \
		modules/A2FOCraftIdentity/directional_shield_display_config.hpp \
		modules/A2FOCraftIdentity/directional_shield_fill.cpp \
		modules/A2FOCraftIdentity/directional_shield_fill.hpp \
		modules/A2FOCraftIdentity/extended_weapon_icons.hpp \
		modules/A2FOCraftIdentity/identity_selection.cpp \
		modules/A2FOCraftIdentity/identity_selection.hpp \
		modules/A2FOCraftIdentity/system_icon_state.cpp \
		modules/A2FOCraftIdentity/system_icon_state.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOCraftIdentity -o $@ \
		tests/craft_identity_test.cpp \
		modules/A2FOCraftIdentity/directional_shield_display_config.cpp \
		modules/A2FOCraftIdentity/directional_shield_fill.cpp \
		modules/A2FOCraftIdentity/identity_selection.cpp \
		modules/A2FOCraftIdentity/system_icon_state.cpp $(RUST_HOST_LINK)

$(EDIT_MENU_TEST): tests/edit_menu_test.cpp \
		modules/A2FOEditMenu/edit_menu_odf.cpp \
		modules/A2FOEditMenu/edit_menu_odf.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOEditMenu -o $@ \
		tests/edit_menu_test.cpp modules/A2FOEditMenu/edit_menu_odf.cpp $(RUST_HOST_LINK)

$(ENERGY_SYSTEMS_TEST): tests/energy_systems_test.cpp \
		modules/A2FOEnergySystems/energy_systems.cpp \
		modules/A2FOEnergySystems/energy_systems.hpp $(RUST_HOST_LIB) $(RUST_FFI_HEADERS) | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOEnergySystems -o $@ \
		tests/energy_systems_test.cpp \
		modules/A2FOEnergySystems/energy_systems.cpp $(RUST_HOST_LINK)

$(BUILD_DIR)/resupply_policy_test: tests/resupply_policy_test.cpp \
		modules/A2FOEnergySystems/resupply_policy.cpp \
		modules/A2FOEnergySystems/resupply_policy.hpp $(RUST_HOST_LIB) | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOEnergySystems -o $@ $< \
		modules/A2FOEnergySystems/resupply_policy.cpp $(RUST_HOST_LINK)

$(DIRECTIONAL_SHIELDS_TEST): tests/directional_shields_test.cpp \
		modules/A2FODirectionalShields/directional_shields.cpp \
		modules/A2FODirectionalShields/directional_shields.hpp $(RUST_HOST_LIB) $(RUST_FFI_HEADERS) | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FODirectionalShields -o $@ \
		tests/directional_shields_test.cpp \
		modules/A2FODirectionalShields/directional_shields.cpp $(RUST_HOST_LINK)

$(INSTANT_ACTION_SETTINGS_TEST): \
		tests/instant_action_settings_test.cpp \
		modules/A2FOInstantActionSettings/load_button_bounds.cpp \
		modules/A2FOInstantActionSettings/load_button_bounds.hpp \
		modules/A2FOInstantActionSettings/setup_details_line.cpp \
		modules/A2FOInstantActionSettings/setup_details_line.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOInstantActionSettings -o $@ \
		tests/instant_action_settings_test.cpp \
		modules/A2FOInstantActionSettings/load_button_bounds.cpp \
		modules/A2FOInstantActionSettings/setup_details_line.cpp $(RUST_HOST_LINK)

$(BUILD_TIME_TEXT_TEST): tests/build_time_text_test.cpp \
		modules/A2FOBuildTooltips/build_time_text.cpp \
		modules/A2FOBuildTooltips/build_time_text.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOBuildTooltips -o $@ \
		tests/build_time_text_test.cpp \
		modules/A2FOBuildTooltips/build_time_text.cpp $(RUST_HOST_LINK)

$(FIRE_ARC_TEST): tests/fire_arc_test.cpp \
		modules/A2FOFireArcs/fire_arc.cpp \
		modules/A2FOFireArcs/fire_arc.hpp \
		modules/A2FOFireArcs/runtime_config.cpp \
		modules/A2FOFireArcs/runtime_config.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOFireArcs -o $@ \
		tests/fire_arc_test.cpp modules/A2FOFireArcs/fire_arc.cpp \
		modules/A2FOFireArcs/runtime_config.cpp $(RUST_HOST_LINK)

$(UPGRADE_POD_CONFIG_TEST): tests/upgrade_pod_config_test.cpp \
		modules/A2FOFeaturePack/upgrade_pod_config.cpp \
		modules/A2FOFeaturePack/upgrade_pod_config.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOFeaturePack -o $@ \
		tests/upgrade_pod_config_test.cpp \
		modules/A2FOFeaturePack/upgrade_pod_config.cpp $(RUST_HOST_LINK)

$(WRECKAGE_POLICY_TEST): tests/wreckage_policy_test.cpp \
		modules/A2FOWreckage/wreckage_policy.cpp \
		modules/A2FOWreckage/wreckage_policy.hpp $(RUST_HOST_LIB) $(RUST_FFI_HEADERS) | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOWreckage -o $@ \
		tests/wreckage_policy_test.cpp \
		modules/A2FOWreckage/wreckage_policy.cpp $(RUST_HOST_LINK)

$(REFIT_POLICY_TEST): tests/refit_policy_test.cpp \
		modules/A2FORefitYards/docking_transform.hpp \
		modules/A2FORefitYards/refit_policy.cpp \
		modules/A2FORefitYards/refit_policy.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FORefitYards -o $@ \
		tests/refit_policy_test.cpp \
		modules/A2FORefitYards/refit_policy.cpp $(RUST_HOST_LINK)

# Policy tests are host-only. The separate x86 harness exercises the native
# adapter against mocked gateways; in-game acceptance remains a manual step.
.PHONY: squadrons-test
squadrons-test: $(SQUADRONS_TEST)
	$(SQUADRONS_TEST)

$(SQUADRONS_TEST): tests/squadrons_test.cpp $(SQUADRONS_SOURCES) \
		$(SQUADRONS_HEADERS) | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOSquadrons -o $@ tests/squadrons_test.cpp $(SQUADRONS_SOURCES) $(RUST_HOST_LINK)

$(SQUADRONS_X86_TEST): tests/squadrons_test.cpp $(SQUADRONS_SOURCES) \
		$(SQUADRONS_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=gnu++17 -O2 -Wall -Wextra -Wpedantic \
		-static -static-libgcc -static-libstdc++ -Imodules/A2FOSquadrons \
		-o $@ tests/squadrons_test.cpp $(SQUADRONS_SOURCES) $(RUST_WIN_LINK)

$(BUILD_DIR)/squadrons_native_test.exe: tests/squadrons_native_test.cpp \
		tests/squadrons_native_economics_test.inl \
		modules/A2FOSquadrons/native_economics.inl \
		modules/A2FOSquadrons/api.hpp \
		modules/A2FOFeaturePack/refit_queue_bridge_api.hpp \
		tests/squadrons_native_repair_test.inl \
		modules/A2FOSquadrons/module.cpp $(call a2fo_rust_bridge,modules/A2FOSquadrons/thiscall_bridge.S) \
		modules/A2FOSquadrons/native_repair.inl \
		$(SQUADRONS_SOURCES) $(SQUADRONS_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=gnu++17 -O2 -Wall -Wextra -Wpedantic \
		-static -static-libgcc -static-libstdc++ -o $@ \
		tests/squadrons_native_test.cpp $(SQUADRONS_SOURCES) \
		$(call a2fo_rust_bridge,modules/A2FOSquadrons/thiscall_bridge.S) $(RUST_WIN_LINK)

$(BUILD_DIR)/pod_group_selection_test.exe: tests/pod_group_selection_test.cpp \
		modules/A2FOSquadrons/module.cpp modules/A2FOSquadrons/native_pod_selection.inl \
		modules/A2FOSquadrons/native_repair.inl modules/A2FOSquadrons/native_economics.inl \
		$(SQUADRONS_SOURCES) $(SQUADRONS_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=gnu++17 -O2 -Wall -Wextra -Wpedantic \
		-static -static-libgcc -static-libstdc++ -o $@ \
		tests/pod_group_selection_test.cpp $(SQUADRONS_SOURCES) \
		$(call a2fo_rust_bridge,modules/A2FOSquadrons/thiscall_bridge.S) $(RUST_WIN_LINK)

$(BUILD_DIR)/squadrons_resources_test.exe: tests/squadrons_resources_test.cpp \
		modules/A2FOResources/module.cpp modules/A2FOResources/api.hpp \
		modules/A2FOResources/additional_resources.cpp modules/A2FOResources/additional_resources.hpp \
		modules/A2FOSquadrons/api.hpp $(call a2fo_rust_bridge,modules/A2FOResources/thiscall_bridge.S) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=gnu++17 -O2 -Wall -Wextra -Wpedantic \
		-static -static-libgcc -static-libstdc++ -o $@ tests/squadrons_resources_test.cpp \
		modules/A2FOResources/additional_resources.cpp $(call a2fo_rust_bridge,modules/A2FOResources/thiscall_bridge.S) $(RUST_WIN_LINK)

$(BUILD_DIR)/squadrons_queue_bridge_test.exe: tests/squadrons_queue_bridge_test.cpp \
		modules/A2FOFeaturePack/buildyard_pseudo_technology.cpp \
		modules/A2FOFeaturePack/queue_enhancement.cpp modules/A2FOFeaturePack/queue_enhancement.hpp \
		modules/A2FOFeaturePack/refit_queue_bridge_api.hpp \
		modules/A2FOFeaturePack/refit_queue_bridge_client.cpp \
		modules/A2FOFeaturePack/hybrid_bridge_client.cpp $(call a2fo_rust_bridge,modules/A2FOFeaturePack/delphi_bridge.S) $(RUST_WIN_LIB) $(RUST_FFI_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=gnu++17 -O2 -Wall -Wextra -Wpedantic \
		-static -static-libgcc -static-libstdc++ -o $@ tests/squadrons_queue_bridge_test.cpp \
		modules/A2FOFeaturePack/buildyard_pseudo_technology.cpp \
		modules/A2FOFeaturePack/refit_queue_bridge_client.cpp \
		modules/A2FOFeaturePack/hybrid_bridge_client.cpp $(call a2fo_rust_bridge,modules/A2FOFeaturePack/delphi_bridge.S) $(RUST_WIN_LINK)

$(WEAPON_DAMAGE_CONTROLS_TEST): tests/weapon_damage_controls_test.cpp \
		modules/A2FOWeaponDamageControls/damage_controls.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOWeaponDamageControls -o $@ \
		tests/weapon_damage_controls_test.cpp

$(SHIELD_VISIBILITY_TEST): tests/shield_visibility_test.cpp \
		modules/A2FOAlwaysShowShields/shield_visibility.cpp \
		modules/A2FOAlwaysShowShields/shield_visibility.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOAlwaysShowShields -o $@ \
		tests/shield_visibility_test.cpp \
		modules/A2FOAlwaysShowShields/shield_visibility.cpp $(RUST_HOST_LINK)

$(NEBULA_EMISSIVE_TEST): tests/nebula_emissive_test.cpp \
		core/nebula_emissive.cpp core/nebula_emissive.hpp \
		core/renderer_draw_policy.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Icore -o $@ tests/nebula_emissive_test.cpp \
		core/nebula_emissive.cpp $(RUST_HOST_LINK)

$(AMD_DOT3_COMPAT_TEST): tests/amd_dot3_compat_test.cpp \
		core/amd_dot3_compat.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Icore -o $@ tests/amd_dot3_compat_test.cpp

$(COM_OWNER_TEST): tests/com_owner_test.cpp core/com_owner.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Icore -o $@ tests/com_owner_test.cpp

$(GAME_MONITOR_POLICY_TEST): tests/game_monitor_policy_test.cpp \
		core/game_monitor_policy.cpp core/game_monitor_policy.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Icore -o $@ tests/game_monitor_policy_test.cpp \
		core/game_monitor_policy.cpp $(RUST_HOST_LINK)

$(ART_TEXTURE_SUFFIX_CONFIG_TEST): tests/art_texture_suffix_config_test.cpp \
		modules/A2FONebulaRenderer/art_texture_suffix_config.cpp \
		modules/A2FONebulaRenderer/art_texture_suffix_config.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FONebulaRenderer -o $@ \
		tests/art_texture_suffix_config_test.cpp \
		modules/A2FONebulaRenderer/art_texture_suffix_config.cpp $(RUST_HOST_LINK)

$(DECAL_MATH_TEST): tests/rust_stage16_decal_binding.cpp rust/text_geometry_ffi.h $(RUST_HOST_LIB) | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-o $@ tests/rust_stage16_decal_binding.cpp $(RUST_HOST_LINK)

$(POINT_DEFENSE_CYCLE_TEST): tests/point_defense_cycle_test.cpp \
		modules/A2FOPointDefenseCycles/firing_cycle.cpp \
		modules/A2FOPointDefenseCycles/firing_cycle.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOPointDefenseCycles -o $@ \
		tests/point_defense_cycle_test.cpp \
		modules/A2FOPointDefenseCycles/firing_cycle.cpp $(RUST_HOST_LINK)

$(SWARM_MOTION_TEST): tests/swarm_motion_test.cpp \
		modules/A2FOSwarmSystem/swarm_motion.cpp \
		modules/A2FOSwarmSystem/swarm_motion.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOSwarmSystem -o $@ \
		tests/swarm_motion_test.cpp \
		modules/A2FOSwarmSystem/swarm_motion.cpp $(RUST_HOST_LINK)

$(TEXTURE_VARIANTS_TEST): tests/texture_variants_test.cpp \
		modules/A2FOTextureVariants/texture_variants.cpp \
		modules/A2FOTextureVariants/texture_variants.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic \
		-Imodules/A2FOTextureVariants -o $@ \
		tests/texture_variants_test.cpp \
		modules/A2FOTextureVariants/texture_variants.cpp $(RUST_HOST_LINK)

verify: release verify-builtins
	@echo "A2FOExtensions exports:"
	@$(OBJDUMP) -p $(BUILD_DIR)/A2FOExtensions.dll | \
		grep -E "A2FO_Initialize|A2FO_NebulaRendererStatus|A2FO_NebulaSet(EmissiveBumpMultiplier|BumpLightBias|EmissiveDiffuseRestore|FastNonBumpEnabled)|A2FO_NebulaRegister(Emissive(Class|Materials)|SpecularMaterials)|A2FO_NebulaBeginCraftRender|A2FO_NebulaEndCraftRender|DLL Name" || true
	@echo
	@echo "Proxy exports:"
	@$(OBJDUMP) -p $(BUILD_DIR)/Win2kDisableTaskSwitch.dll | \
		grep -E "LowLevelKeyboardProc|SetHookID|DLL Name" || true
	@echo
	@echo "Renderer helper dependencies:"
	@$(OBJDUMP) -p $(A2FO_RENDERER_HELPER) | \
		grep -E "DLL Name" || true
	@echo
	@echo "A2FOAlwaysShowShields module exports:"
	@$(OBJDUMP) -p $(BUILD_DIR)/A2FOExtensions.dll | \
		grep -E "A2FOBuiltinAlwaysShowShields_Init|A2FOBuiltinAlwaysShowShields_Shutdown|A2FOAlwaysShowShields_RegisterClass|A2FOAlwaysShowShields_UpdateCraft|A2FOAlwaysShowShields_CleanupCraft|DLL Name" || true
	@echo
	@echo "A2FOFeaturePack module exports:"
	@$(OBJDUMP) -p $(MODULE_DIR)/A2FOFeaturePack.dll | \
		grep -E "A2FO_ModuleInit|A2FO_(RegisterRefitQueueBridge|ProducerPushRefit)|DLL Name" || true
	@echo
	@echo "A2FOHybridBuild module exports:"
	@$(OBJDUMP) -p $(MODULE_DIR)/A2FOHybridBuild.dll | \
		grep -E "A2FO_ModuleInit|A2FO_RegisterRefitUiBridge|DLL Name" || true
	@echo
	@echo "A2FOCheats module exports:"
	@$(OBJDUMP) -p $(CHEATS_MODULE) | \
		grep -E "A2FO_ModuleInit|DLL Name" || true
	@echo
	@echo "A2FOCraftIdentity module exports:"
	@$(OBJDUMP) -p $(CRAFT_IDENTITY_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true
	@echo
	@echo "A2FOEditMenu built-in exports:"
	@$(OBJDUMP) -p $(BUILD_DIR)/A2FOExtensions.dll | \
		grep -E "A2FOBuiltinEditMenu_(Init|Shutdown)|DLL Name" || true
	@echo
	@echo "A2FODirectionalShields module exports:"
	@$(OBJDUMP) -p $(DIRECTIONAL_SHIELDS_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|A2FODirectionalShields_(ConnectDamageBridge|BeginDamage|EndDamage|IsEnabled|GetCurrent|GetMaximum)|DLL Name" || true
	@echo
	@echo "A2FOEnergySystems module exports:"
	@$(OBJDUMP) -p $(ENERGY_SYSTEMS_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|A2FOEnergySystems_(Get|Set|Add)|DLL Name" || true
	@echo
	@echo "A2FOInstantActionSettings built-in exports:"
	@$(OBJDUMP) -p $(BUILD_DIR)/A2FOExtensions.dll | \
		grep -E "A2FOBuiltinInstantActionSettings_(Init|Shutdown)|DLL Name" || true
	@echo
	@echo "A2FOResources module exports:"
	@$(OBJDUMP) -p $(RESOURCES_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|A2FOResources_(Get|Set|Add|GetCost|GetPresentationText)|DLL Name" || true
	@echo
	@echo "A2FOMissionSelector module exports:"
	@$(OBJDUMP) -p $(MISSION_SELECTOR_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true
	@echo
	@echo "A2FOMissions module exports:"
	@$(OBJDUMP) -p $(MISSIONS_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|A2FOMissions_(ScriptInit|ScriptExtendInterface|MarkMissionUnloaded)|DLL Name" || true
	@echo
	@echo "MMM loader compatibility shim exports:"
	@$(OBJDUMP) -p $(MMM_LOADER_SHIM) | \
		grep -E "ScriptInit|ScriptExtendInterface|DLL Name" || true
	@echo
	@echo "A2FOFireArcs module exports:"
	@$(OBJDUMP) -p $(BUILD_DIR)/A2FOExtensions.dll | \
		grep -E "A2FOBuiltinFireArcs_Init|A2FOBuiltinFireArcs_Shutdown|A2FOFireArcs_AllowWeaponTrigger|DLL Name" || true
	@echo
	@echo "A2FONebulaRenderer module exports:"
	@$(OBJDUMP) -p $(NEBULA_RENDERER_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|A2FONebulaRenderer_RegisterClass|DLL Name" || true
	@echo
	@echo "A2FOPointDefenseCycles module exports:"
	@$(OBJDUMP) -p $(POINT_DEFENSE_CYCLES_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true
	@echo
	@echo "A2FOSwarmSystem built-in exports:"
	@$(OBJDUMP) -p $(BUILD_DIR)/A2FOExtensions.dll | \
		grep -E "A2FOBuiltinSwarmSystem_(Init|Shutdown)|DLL Name" || true
	@echo
	@echo "A2FOTextureVariants module exports:"
	@$(OBJDUMP) -p $(TEXTURE_VARIANTS_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|A2FOTextureVariants_RegisterClass|DLL Name" || true
	@echo
	@echo "A2FOODFVariants built-in exports:"
	@$(OBJDUMP) -p $(BUILD_DIR)/A2FOExtensions.dll | \
		grep -E "A2FOBuiltinODFVariants_(Init|Shutdown)|DLL Name" || true
	@echo
	@echo "A2FOTurrets module exports:"
	@$(OBJDUMP) -p $(TURRETS_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true
	@echo
	@echo "A2FORefitYards module exports:"
	@$(OBJDUMP) -p $(REFIT_YARDS_MODULE) | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true
	@echo
	@echo "A2FOInfestation built-in exports:"
	@$(OBJDUMP) -p $(BUILD_DIR)/A2FOExtensions.dll | \
		grep -E "A2FOBuiltinInfestation_(Init|Shutdown)|DLL Name" || true
	@echo
	@echo "A2FOM3D built-in exports:"
	@$(OBJDUMP) -p $(BUILD_DIR)/A2FOExtensions.dll | \
		grep -E "A2FOBuiltinM3D_(Init|Shutdown)|DLL Name" || true
	@echo
	@echo "A2FOBreeders built-in exports:"
	@$(OBJDUMP) -p $(BUILD_DIR)/A2FOExtensions.dll | \
		grep -E "A2FOBuiltinBreeders_(Init|Shutdown)|DLL Name" || true
	@echo
	@echo "A2FOStationRotation module exports:"
	@$(OBJDUMP) -p $(BUILD_DIR)/A2FOExtensions.dll | \
		grep -E "A2FOBuiltinStationRotation_Init|A2FOBuiltinStationRotation_Shutdown|DLL Name" || true
	@echo
	@echo "A2FORGBTextures module exports:"
	@$(OBJDUMP) -p $(MODULE_DIR)/A2FORGBTextures.dll | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true
	@echo
	@echo "Checking for non-system MinGW runtime dependencies:"
	@for dll in \
		$(BUILD_DIR)/A2FOExtensions.dll \
		$(BUILD_DIR)/Win2kDisableTaskSwitch.dll \
		$(A2FO_RENDERER_HELPER) \
		$(MODULE_DIR)/A2FOFeaturePack.dll \
		$(MODULE_DIR)/A2FOHybridBuild.dll \
		$(CHEATS_MODULE) \
		$(CRAFT_IDENTITY_MODULE) \
		$(DIRECTIONAL_SHIELDS_MODULE) \
		$(ENERGY_SYSTEMS_MODULE) \
		$(RESOURCES_MODULE) \
		$(MISSION_SELECTOR_MODULE) \
		$(NEBULA_RENDERER_MODULE) \
		$(POINT_DEFENSE_CYCLES_MODULE) \
		$(TEXTURE_VARIANTS_MODULE) \
		$(STA1_COMPAT_MODULE) \
		$(TURRETS_MODULE) \
		$(REFIT_YARDS_MODULE) \
		$(SQUADRONS_MODULE) \
		$(COLLECTIVE_XP_MODULE) \
		$(MODULE_DIR)/A2FORGBTextures.dll; do \
		if $(OBJDUMP) -p "$$dll" | \
			grep -Eiq 'DLL Name: (libgcc|libstdc\+\+|libwinpthread)'; then \
			echo "Unexpected MinGW runtime dependency in $$dll" >&2; \
			$(OBJDUMP) -p "$$dll" | grep -Ei 'DLL Name:' >&2; \
			exit 1; \
		fi; \
	done
	@echo "No external MinGW runtime DLLs required."

verify-sta1-classic: sta1-classic verify-builtins
	@test ! -d "$(STA1_CLASSIC_DIR)/modules" || \
		(echo "Mod package must not contain a native modules directory" >&2; exit 1)
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
	@echo "STA1 Classic selects its centrally installed modules through info.ini."

verify-sdk: sdk-examples
	@$(OBJDUMP) -p $(MODULE_DIR)/ExampleModule.dll | \
		grep -E "A2FO_ModuleInit|A2FO_ModuleShutdown|DLL Name" || true

test: builtin-tests collective-xp-test $(FPQ_PATHS_TEST) $(ODF_PATHS_TEST) $(EXTENSION_ROOTS_TEST) \
	$(MODULE_API_TEST) $(MODULE_POLICY_TEST) $(HYBRID_PRODUCTION_TEST) \
	$(BUILD_SUBMENU_CONFIG_TEST) $(TURRET_MATH_TEST) \
	$(CRAFT_IDENTITY_TEST) $(EDIT_MENU_TEST) $(INSTANT_ACTION_SETTINGS_TEST) \
	$(ENERGY_SYSTEMS_TEST) $(DIRECTIONAL_SHIELDS_TEST) \
	$(BUILD_TIME_TEXT_TEST) $(ADDITIONAL_RESOURCES_TEST) $(FIRE_ARC_TEST) \
	$(A1_RACE_MENU_TEST) $(A1_TEAM_COLOR_TEST) $(A1_BZN_POLICY_TEST) \
	$(A1_UI_POLICY_TEST) $(A1_FALLBACK_POLICY_TEST) \
	$(UPGRADE_POD_CONFIG_TEST) \
	$(WRECKAGE_POLICY_TEST) \
	$(REFIT_POLICY_TEST) \
	$(SQUADRONS_TEST) \
	$(STATION_ROTATION_TEST) \
	$(WEAPON_DAMAGE_CONTROLS_TEST) \
	$(SHIELD_VISIBILITY_TEST) $(NEBULA_EMISSIVE_TEST) \
	$(AMD_DOT3_COMPAT_TEST) $(COM_OWNER_TEST) \
	$(GAME_MONITOR_POLICY_TEST) \
	$(ART_TEXTURE_SUFFIX_CONFIG_TEST) $(DECAL_MATH_TEST) \
	$(POINT_DEFENSE_CYCLE_TEST) $(SWARM_MOTION_TEST) \
	$(TEXTURE_VARIANTS_TEST) \
	$(M3D_CONVERSION_TEST)
	$(FPQ_PATHS_TEST)
	$(ODF_PATHS_TEST)
	$(EXTENSION_ROOTS_TEST)
	$(MODULE_API_TEST)
	$(MODULE_POLICY_TEST)
	$(HYBRID_PRODUCTION_TEST)
	$(BUILD_SUBMENU_CONFIG_TEST)
	$(TURRET_MATH_TEST)
	$(CRAFT_IDENTITY_TEST)
	$(EDIT_MENU_TEST)
	$(ENERGY_SYSTEMS_TEST)
	$(DIRECTIONAL_SHIELDS_TEST)
	$(INSTANT_ACTION_SETTINGS_TEST)
	$(BUILD_TIME_TEXT_TEST)
	$(ADDITIONAL_RESOURCES_TEST)
	$(A1_RACE_MENU_TEST)
	$(A1_TEAM_COLOR_TEST)
	$(A1_BZN_POLICY_TEST)
	$(A1_UI_POLICY_TEST)
	$(A1_FALLBACK_POLICY_TEST)
	$(FIRE_ARC_TEST)
	$(UPGRADE_POD_CONFIG_TEST)
	$(WRECKAGE_POLICY_TEST)
	$(REFIT_POLICY_TEST)
	$(SQUADRONS_TEST)
	$(STATION_ROTATION_TEST)
	$(WEAPON_DAMAGE_CONTROLS_TEST)
	$(SHIELD_VISIBILITY_TEST)
	$(NEBULA_EMISSIVE_TEST)
	$(AMD_DOT3_COMPAT_TEST)
	$(COM_OWNER_TEST)
	$(GAME_MONITOR_POLICY_TEST)
	$(ART_TEXTURE_SUFFIX_CONFIG_TEST)
	$(DECAL_MATH_TEST)
	$(POINT_DEFENSE_CYCLE_TEST)
	$(SWARM_MOTION_TEST)
	$(TEXTURE_VARIANTS_TEST)
	$(M3D_CONVERSION_TEST)
	python3 -m unittest tests/test_odf_formatter.py \
		tests/test_modder_documentation.py

smoke: release $(SMOKE_TEST) $(GAME_MONITOR_SMOKE)
	cd $(BUILD_DIR) && wine dll_load_smoke.exe
	cd $(BUILD_DIR) && wine game_monitor_win32_smoke.exe

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

clean:
	rm -rf $(BUILD_DIR)

# Included implementation for automatic non-bump MeshVB preparation.
build/A2FOExtensions.dll: core/renderer_unmapped_meshvb.inl

# Hardware-lit Phong support shares native MeshVB buffer ownership.
build/A2FOExtensions.dll: core/renderer_phong_meshvb.inl

# Cloak lighting is composited once, without opaque framebuffer accumulation.
build/A2FOExtensions.dll: core/renderer_cloak_composite.inl

# Opt-in Refit refraction; isolated fixtures never launch or alter a game.
.PHONY: renderer-cloak-tests
renderer-cloak-tests: $(BUILD_DIR)/renderer_cloak_policy_test $(BUILD_DIR)/renderer_cloak_native.exe $(BUILD_DIR)/renderer_cloak_dispatch_native.exe

$(BUILD_DIR)/renderer_cloak_policy_test: tests/renderer_cloak_policy_test.cpp core/renderer_cloak_policy.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $<

$(BUILD_DIR)/renderer_cloak_dispatch_native.exe: tests/renderer_cloak_dispatch_native.cpp core/renderer_cloak_refraction.inl core/renderer_cloak_policy.hpp renderer/cloak/refraction.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $<

$(BUILD_DIR)/renderer_cloak_native.exe: tests/renderer_cloak_native.cpp tests/renderer_cloak_native_d3d9.cpp renderer/cloak/refraction.cpp renderer/cloak/refraction.hpp renderer/cloak/refraction_shader.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ tests/renderer_cloak_native.cpp tests/renderer_cloak_native_d3d9.cpp renderer/cloak/refraction.cpp -ld3d8 -ldxguid -luuid $(RUST_WIN_LINK)

# Standard shell button artwork and interaction in the mission selector.
$(MISSION_SELECTOR_MODULE): modules/A2FOMissionSelector/standard_buttons.inl

# Shared visible/cloaked mesh compositor integration.
build/A2FOExtensions.dll: core/renderer_general_meshvb.inl

# Alpha MeshVB dynamic index-stream sorting.
build/A2FOExtensions.dll: core/renderer_transparent_indices.inl

# Focused BuildYard native ABI checks. Run the EXEs manually with the real
# MSVCP60.DLL path and an isolated Wine prefix; see the BuildYard test guide.
$(BUILD_DIR)/buildyard_tooltip_native_test.exe: tests/buildyard_tooltip_native_test.cpp \
		modules/A2FOFeaturePack/buildyard_tooltip_native.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -static -o $@ $<

$(BUILD_DIR)/buildyard_tooltip_provider_test.exe: tests/buildyard_tooltip_provider_test.cpp \
		tests/buildyard_tooltip_native_test.cpp \
		modules/A2FOFeaturePack/buildyard_pseudo_technology.cpp \
		modules/A2FOFeaturePack/buildyard_pseudo_technology.hpp \
		modules/A2FOFeaturePack/buildyard_tooltip_native.hpp \
		modules/A2FOFeaturePack/module_technology_policy.hpp \
		modules/A2FOFeaturePack/buildyard_tree_runtime.inl \
		modules/A2FOFeaturePack/buildyard_tree_signatures.hpp \
		$(call a2fo_rust_bridge,modules/A2FOFeaturePack/delphi_bridge.S) sdk/include/a2fo_module_api.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -static -o $@ \
		tests/buildyard_tooltip_provider_test.cpp $(call a2fo_rust_bridge,modules/A2FOFeaturePack/delphi_bridge.S)

# Portable XML/predicate regression suite for symbolic MODULE gates.
$(BUILD_DIR)/module_technology_policy_test: tests/module_technology_policy_test.cpp \
        modules/A2FOFeaturePack/module_technology_policy.hpp | $(BUILD_DIR)
	g++ -std=c++17 -O2 -Wall -Wextra -pedantic -o $@ $<

.PHONY: module-technology-test
module-technology-test: $(BUILD_DIR)/module_technology_policy_test
	$(BUILD_DIR)/module_technology_policy_test

$(BUILD_DIR)/renderer_options_test.exe: tests/renderer_options_test.cpp \
		core/renderer_options.cpp core/renderer_options.hpp \
		core/game_monitor.hpp rust/native_hooks_ffi.h $(RUST_WIN_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -static -o $@ $< -luser32 $(RUST_WIN_LINK)

$(BUILD_DIR)/buildyard_tree_runtime_test.exe: tests/buildyard_tree_runtime_test.cpp \
        tests/buildyard_tree_fixture.S modules/A2FOFeaturePack/buildyard_pseudo_technology.cpp \
        modules/A2FOFeaturePack/buildyard_tree_runtime.inl modules/A2FOFeaturePack/buildyard_tree_signatures.hpp \
        modules/A2FOFeaturePack/module_technology_policy.hpp $(call a2fo_rust_bridge,modules/A2FOFeaturePack/delphi_bridge.S) \
        modules/A2FOFeaturePack/buildyard_pseudo_technology.hpp \
        modules/A2FOFeaturePack/buildyard_tooltip_native.hpp sdk/include/a2fo_module_api.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -static -Wl,--image-base,0x10000000 -o $@ \
		tests/buildyard_tree_runtime_test.cpp tests/buildyard_tree_fixture.S $(call a2fo_rust_bridge,modules/A2FOFeaturePack/delphi_bridge.S)

# Refit-derived optional modules. Their INIs default to passive; building a
# release never installs or enables them in a game.
REFIT_COMMON_HEADERS := modules/refit_common/runtime.hpp \
    modules/refit_common/settings.hpp modules/refit_common/bindings.hpp \
    modules/refit_common/status.hpp sdk/include/a2fo_module_api.h \
    sdk/include/a2fo_supported_armada.hpp

# Optional main-menu presentation; enabling/deployment is separate from build.
REFIT_MENUS_MODULE := $(MODULE_DIR)/A2FORefitMenus.dll
REFIT_MENU_FONT_DIR := third_party/armada2-refit/fonts
REFIT_MENU_FONT_FILES := $(REFIT_MENU_FONT_DIR)/NotoSans-CondensedMedium.ttf \
    $(REFIT_MENU_FONT_DIR)/NotoSans-CondensedSemiBold.ttf
REFIT_MENU_FONT_RESOURCE := $(BUILD_DIR)/refit_menu_fonts.o
REFIT_MENU_SKIN_PARTS := panel header button_normal button_hot button_pressed button_disabled popup popup_selected
REFIT_MENU_SKIN_FILES := $(addprefix assets/refit-menus/ui/,$(addsuffix .png,$(REFIT_MENU_SKIN_PARTS)))
REFIT_MENU_SKIN_BINARY := $(BUILD_DIR)/refit-menu-ui/skin.bgra
REFIT_MENU_SKIN_RESOURCE := $(BUILD_DIR)/refit_menu_ui_skin.o
REFIT_MENU_SKIN_SOURCE := modules/A2FORefitMenus/setup_skin.cpp
REFIT_LOADING_SOURCE := modules/A2FORefitMenus/loading_native.cpp \
    modules/A2FORefitMenus/loading_gpu.cpp modules/A2FORefitMenus/loading_content.cpp \
    modules/A2FORefitMenus/loading_presentation.cpp modules/A2FORefitMenus/loading_theme.cpp
REFIT_LOADING_HEADERS := modules/A2FORefitMenus/loading_native.hpp \
    modules/A2FORefitMenus/loading_bindings.hpp modules/A2FORefitMenus/loading_gpu.hpp \
    modules/A2FORefitMenus/loading_content.hpp modules/A2FORefitMenus/loading_presentation.hpp \
    modules/A2FORefitMenus/loading_theme.hpp modules/A2FORefitMenus/scene_budget.hpp
REFIT_MENU_HEADERS := modules/A2FORefitMenus/presentation.hpp \
    modules/A2FORefitMenus/setup_native.hpp modules/A2FORefitMenus/setup_presentation.hpp \
    modules/A2FORefitMenus/setup_skin.hpp \
    modules/A2FORefitMenus/skin.hpp modules/A2FORefitMenus/skin_native.hpp \
    modules/A2FORefitMenus/skin_bindings.hpp modules/A2FORefitMenus/menu_scene.hpp \
    modules/A2FORefitMenus/mission_backdrop.hpp \
    modules/A2FORefitMenus/scene_composition.hpp modules/A2FORefitMenus/scene_budget.hpp \
    modules/A2FORefitMenus/scene_mailbox.hpp modules/A2FORefitMenus/scene_handover.hpp modules/A2FORefitMenus/scene_transition.hpp modules/A2FORefitMenus/scene_settings.hpp \
    modules/A2FORefitMenus/scene_catalogue.hpp sdk/include/a2fo_menu_scene.h \
    modules/A2FORefitMenus/profile.hpp \
    modules/refit_common/fleetops_profile.hpp \
    modules/A2FORefitMenus/bindings.hpp modules/A2FORefitMenus/native_model.hpp \
    modules/A2FORefitMenus/interaction.hpp modules/A2FORefitMenus/status.hpp \
    modules/refit_common/settings.hpp sdk/include/a2fo_module_api.h \
    sdk/include/a2fo_supported_armada.hpp sdk/include/a2fo_shell_canvas.h

release: $(REFIT_MENUS_MODULE)

$(REFIT_MENU_FONT_RESOURCE): modules/A2FORefitMenus/fonts.rc $(REFIT_MENU_FONT_FILES) | $(BUILD_DIR)
	i686-w64-mingw32-windres -O coff -o $@ $<

$(REFIT_MENU_SKIN_BINARY): tools/generate_refit_menu_skin.py $(REFIT_MENU_SKIN_FILES) | $(BUILD_DIR)
	python3 tools/generate_refit_menu_skin.py --pack --output-build $(BUILD_DIR)/refit-menu-ui

$(REFIT_MENU_SKIN_RESOURCE): assets/refit-menus/ui_skin.rc $(REFIT_MENU_SKIN_BINARY) | $(BUILD_DIR)
	i686-w64-mingw32-windres -I$(BUILD_DIR)/refit-menu-ui -O coff -o $@ $<

$(REFIT_MENUS_MODULE): modules/A2FORefitMenus/module.cpp \
        modules/A2FORefitMenus/setup_native.cpp modules/A2FORefitMenus/setup_presentation.cpp \
        $(REFIT_MENU_SKIN_SOURCE) \
        modules/A2FORefitMenus/skin.cpp modules/A2FORefitMenus/skin_native.cpp \
        modules/A2FORefitMenus/menu_scene.cpp modules/A2FORefitMenus/scene_catalogue.cpp modules/A2FORefitMenus/mission_backdrop.cpp modules/A2FORefitMenus/scene_composition.cpp \
        modules/A2FORefitMenus/presentation.cpp $(call a2fo_rust_bridge,modules/A2FORefitMenus/native_bridge.S) \
        $(REFIT_MENU_HEADERS) $(REFIT_LOADING_HEADERS) $(REFIT_LOADING_SOURCE) $(REFIT_MENU_FONT_RESOURCE) $(REFIT_MENU_SKIN_RESOURCE) | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DNOMINMAX -Werror $(DLLFLAGS) -o $@ $< \
		modules/A2FORefitMenus/presentation.cpp modules/A2FORefitMenus/skin.cpp \
		modules/A2FORefitMenus/skin_native.cpp modules/A2FORefitMenus/menu_scene.cpp modules/A2FORefitMenus/scene_catalogue.cpp modules/A2FORefitMenus/mission_backdrop.cpp \
		modules/A2FORefitMenus/scene_composition.cpp $(call a2fo_rust_bridge,modules/A2FORefitMenus/native_bridge.S) \
		modules/A2FORefitMenus/setup_native.cpp modules/A2FORefitMenus/setup_presentation.cpp \
		$(REFIT_LOADING_SOURCE) \
		$(REFIT_MENU_SKIN_SOURCE) $(REFIT_MENU_SKIN_RESOURCE) \
		$(REFIT_MENU_FONT_RESOURCE) -lcomctl32 -lgdi32 -lmsimg32 -ladvapi32 -lgdiplus $(RUST_WIN_LINK)

$(BUILD_DIR)/refit_loading_presentation_test: tests/refit_loading_presentation_test.cpp modules/A2FORefitMenus/loading_presentation.cpp $(REFIT_LOADING_HEADERS) | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< modules/A2FORefitMenus/loading_presentation.cpp modules/A2FORefitMenus/loading_theme.cpp $(RUST_HOST_LINK)

$(BUILD_DIR)/refit_loading_content_test: tests/refit_loading_content_test.cpp modules/A2FORefitMenus/loading_content.cpp $(REFIT_LOADING_HEADERS) | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< modules/A2FORefitMenus/loading_content.cpp modules/A2FORefitMenus/loading_theme.cpp $(RUST_HOST_LINK)

$(BUILD_DIR)/refit_loading_render.exe: tests/refit_loading_render.cpp modules/A2FORefitMenus/loading_presentation.cpp modules/A2FORefitMenus/presentation.cpp $(REFIT_LOADING_HEADERS) $(REFIT_MENU_FONT_RESOURCE) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DNOMINMAX -Werror -static -o $@ $< modules/A2FORefitMenus/loading_presentation.cpp modules/A2FORefitMenus/presentation.cpp modules/A2FORefitMenus/loading_theme.cpp $(REFIT_MENU_FONT_RESOURCE) -lgdi32 -lmsimg32 $(RUST_WIN_LINK)

$(BUILD_DIR)/refit_loading_native.exe: tests/refit_loading_native.cpp $(REFIT_LOADING_SOURCE) $(REFIT_LOADING_HEADERS) modules/A2FORefitMenus/presentation.cpp $(REFIT_MENU_FONT_RESOURCE) $(BUILD_DIR)/A2FOExtensions.dll $(INSTANT_ACTION_SETTINGS_MODULE) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DNOMINMAX -Werror -static -Wl,--image-base,0x10000000 -o $@ $< modules/A2FORefitMenus/loading_gpu.cpp modules/A2FORefitMenus/loading_content.cpp modules/A2FORefitMenus/loading_presentation.cpp modules/A2FORefitMenus/presentation.cpp modules/A2FORefitMenus/loading_theme.cpp $(REFIT_MENU_FONT_RESOURCE) -lgdi32 -lmsimg32 -lgdiplus $(RUST_WIN_LINK)

$(BUILD_DIR)/refit_loading_gpu.exe: tests/refit_loading_gpu.cpp modules/A2FORefitMenus/mission_backdrop.cpp modules/A2FORefitMenus/mission_backdrop.hpp $(REFIT_LOADING_SOURCE) $(REFIT_LOADING_HEADERS) modules/A2FORefitMenus/presentation.cpp $(REFIT_MENU_FONT_RESOURCE) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DNOMINMAX -Werror -static -o $@ $< modules/A2FORefitMenus/loading_gpu.cpp modules/A2FORefitMenus/loading_content.cpp modules/A2FORefitMenus/loading_presentation.cpp modules/A2FORefitMenus/presentation.cpp modules/A2FORefitMenus/loading_theme.cpp $(REFIT_MENU_FONT_RESOURCE) -lgdi32 -lmsimg32 -lgdiplus $(RUST_WIN_LINK)

$(BUILD_DIR)/refit_loading_images.exe: tests/refit_loading_images.cpp modules/A2FORefitMenus/loading_content.cpp $(REFIT_LOADING_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DNOMINMAX -Werror -static -o $@ $< modules/A2FORefitMenus/loading_content.cpp -lgdiplus -lgdi32 $(RUST_WIN_LINK)

$(BUILD_DIR)/refit_loading_theme_test: tests/refit_loading_theme_test.cpp modules/A2FORefitMenus/loading_theme.cpp modules/A2FORefitMenus/loading_theme.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< modules/A2FORefitMenus/loading_theme.cpp $(RUST_HOST_LINK)

.PHONY: refit-loading-test
refit-loading-test: $(BUILD_DIR)/refit_loading_presentation_test $(BUILD_DIR)/refit_loading_content_test $(BUILD_DIR)/refit_loading_theme_test
	$(BUILD_DIR)/refit_loading_presentation_test
	$(BUILD_DIR)/refit_loading_content_test
	$(BUILD_DIR)/refit_loading_theme_test

# Setup fixtures use synthetic state, memory DCs, or hidden NOACTIVATE windows.
$(BUILD_DIR)/refit_setup_layout_test: tests/refit_setup_layout_test.cpp modules/A2FORefitMenus/setup_presentation.cpp modules/A2FORefitMenus/presentation.cpp modules/A2FORefitMenus/setup_presentation.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< modules/A2FORefitMenus/setup_presentation.cpp modules/A2FORefitMenus/presentation.cpp $(RUST_HOST_LINK)

$(BUILD_DIR)/refit_setup_render.exe: tests/refit_setup_render.cpp modules/A2FORefitMenus/setup_presentation.cpp modules/A2FORefitMenus/presentation.cpp $(REFIT_MENU_SKIN_SOURCE) $(REFIT_MENU_HEADERS) $(REFIT_MENU_FONT_RESOURCE) $(REFIT_MENU_SKIN_RESOURCE) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -DNOMINMAX -static -static-libgcc -static-libstdc++ -o $@ $< modules/A2FORefitMenus/setup_presentation.cpp modules/A2FORefitMenus/presentation.cpp $(REFIT_MENU_SKIN_SOURCE) $(REFIT_MENU_FONT_RESOURCE) $(REFIT_MENU_SKIN_RESOURCE) -lgdi32 -lmsimg32 $(RUST_WIN_LINK)

$(BUILD_DIR)/refit_setup_native.exe: tests/refit_setup_native.cpp modules/A2FORefitMenus/setup_native.cpp modules/A2FORefitMenus/setup_presentation.cpp modules/A2FORefitMenus/presentation.cpp modules/A2FORefitMenus/loading_native.hpp $(REFIT_MENU_SKIN_SOURCE) $(REFIT_MENU_HEADERS) $(REFIT_MENU_FONT_RESOURCE) $(REFIT_MENU_SKIN_RESOURCE) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -DNOMINMAX -static -static-libgcc -static-libstdc++ -Wl,--image-base,0x10000000 -o $@ $< modules/A2FORefitMenus/setup_presentation.cpp modules/A2FORefitMenus/presentation.cpp $(REFIT_MENU_SKIN_SOURCE) $(REFIT_MENU_FONT_RESOURCE) $(REFIT_MENU_SKIN_RESOURCE) -lcomctl32 -lgdi32 -lmsimg32 -ladvapi32 -lgdi32 $(RUST_WIN_LINK)

$(BUILD_DIR)/refit_skin_blend_native.exe: tests/refit_skin_blend_native.cpp modules/A2FORefitMenus/setup_skin.cpp modules/A2FORefitMenus/setup_skin.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -DNOMINMAX -static -static-libgcc -static-libstdc++ -o $@ $< -lgdi32 -lmsimg32 $(RUST_WIN_LINK)

$(BUILD_DIR)/shell_canvas_copy_test.exe: tests/shell_canvas_copy_test.cpp modules/A2FOShellDisplay/canvas.cpp modules/A2FOShellDisplay/canvas.hpp sdk/include/a2fo_shell_canvas.h $(call a2fo_rust_bridge,modules/A2FOShellDisplay/delphi_bridge.S) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -UNDEBUG -static -static-libgcc -static-libstdc++ -o $@ $< $(call a2fo_rust_bridge,modules/A2FOShellDisplay/delphi_bridge.S) -lgdi32 -luser32

$(BUILD_DIR)/refit_menu_skin_test: tests/refit_menu_skin_test.cpp \
        modules/A2FORefitMenus/skin.cpp modules/A2FORefitMenus/skin.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< modules/A2FORefitMenus/skin.cpp $(RUST_HOST_LINK)

$(BUILD_DIR)/refit_menu_presentation_test: tests/refit_menu_presentation_test.cpp \
        modules/A2FORefitMenus/presentation.cpp $(REFIT_MENU_HEADERS) | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -DNDEBUG -o $@ $< modules/A2FORefitMenus/presentation.cpp $(RUST_HOST_LINK)

$(BUILD_DIR)/refit_scene_mailbox_test: tests/refit_scene_mailbox_test.cpp \
        modules/A2FORefitMenus/scene_mailbox.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -pthread -o $@ $<

$(BUILD_DIR)/refit_scene_handover_test: tests/refit_scene_handover_test.cpp modules/A2FORefitMenus/scene_handover.hpp modules/A2FORefitMenus/scene_transition.hpp modules/A2FORefitMenus/presentation.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -pthread -o $@ $<

$(BUILD_DIR)/refit_scene_settings_test: tests/refit_scene_settings_test.cpp \
        modules/A2FORefitMenus/scene_settings.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $<

# Full scene integration fixture; execute only inside an isolated headless compositor.
$(BUILD_DIR)/refit_scene_worker_native.exe: tests/refit_scene_worker_native.cpp \
        modules/A2FORefitMenus/menu_scene.cpp modules/A2FORefitMenus/scene_catalogue.cpp modules/A2FORefitMenus/mission_backdrop.cpp modules/A2FORefitMenus/presentation.cpp \
        $(REFIT_MENU_HEADERS) $(REFIT_MENU_FONT_RESOURCE) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -DNOMINMAX -static \
		-static-libgcc -static-libstdc++ -o $@ $< modules/A2FORefitMenus/scene_catalogue.cpp modules/A2FORefitMenus/mission_backdrop.cpp modules/A2FORefitMenus/presentation.cpp \
		$(REFIT_MENU_FONT_RESOURCE) -lcomctl32 -lgdi32 -lmsimg32 $(RUST_WIN_LINK)

$(BUILD_DIR)/refit_mission_backdrop_native.exe: tests/refit_mission_backdrop_native.cpp \
        tests/support/refit_fo_chat_fixture.hpp \
        modules/A2FORefitMenus/mission_backdrop.cpp modules/A2FORefitMenus/mission_backdrop.hpp \
        modules/A2FORefitMenus/loading_bindings.hpp rust/native_hooks_ffi.h $(RUST_WIN_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -DNOMINMAX -static \
		-static-libgcc -static-libstdc++ -Wl,--image-base,0x10000000 -o $@ $< -lgdi32 $(RUST_WIN_LINK)

$(BUILD_DIR)/refit_menu_model_test: tests/refit_menu_model_test.cpp \
        modules/A2FORefitMenus/presentation.cpp $(REFIT_MENU_HEADERS) | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -DNDEBUG -o $@ $< modules/A2FORefitMenus/presentation.cpp $(RUST_HOST_LINK)

$(BUILD_DIR)/refit_menu_native.exe: tests/refit_menu_native.cpp tests/refit_menu_native_bridge.S \
        tests/support/refit_fo_chat_fixture.hpp \
        rust/native_hooks_ffi.h $(RUST_WIN_LIB) $(REFIT_MENU_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -static \
		-static-libgcc -static-libstdc++ -Wl,--image-base,0x10000000 -o $@ $< tests/refit_menu_native_bridge.S -lgdi32 $(RUST_WIN_LINK)

$(BUILD_DIR)/refit_menu_render.exe: tests/refit_menu_render.cpp modules/A2FORefitMenus/presentation.cpp \
        modules/A2FORefitMenus/presentation.hpp $(REFIT_MENU_FONT_RESOURCE) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -static \
		-static-libgcc -static-libstdc++ -o $@ $< modules/A2FORefitMenus/presentation.cpp \
		$(REFIT_MENU_FONT_RESOURCE) -lgdi32 -lmsimg32 $(RUST_WIN_LINK)

$(BUILD_DIR)/refit_menu_artwork_perf.exe: tests/refit_menu_artwork_perf.cpp modules/A2FORefitMenus/presentation.cpp \
        $(REFIT_MENU_HEADERS) $(REFIT_MENU_FONT_RESOURCE) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -DNOMINMAX -static \
		-static-libgcc -static-libstdc++ -o $@ $< modules/A2FORefitMenus/presentation.cpp \
		$(REFIT_MENU_FONT_RESOURCE) -lgdi32 -lmsimg32 $(RUST_WIN_LINK)

.PHONY: refit-menus refit-menus-test refit-menus-native-test
refit-menus: $(REFIT_MENUS_MODULE) $(BUILD_DIR)/licenses/armada2-refit.txt \
    $(BUILD_DIR)/licenses/noto-sans-condensed-OFL.txt \
    $(BUILD_DIR)/licenses/noto-sans-condensed-SOURCE.txt
refit-menus-test: $(BUILD_DIR)/refit_scene_handover_test $(BUILD_DIR)/refit_scene_catalogue_test $(BUILD_DIR)/refit_setup_layout_test $(BUILD_DIR)/refit_scene_budget_test $(BUILD_DIR)/refit_scene_composition_test $(BUILD_DIR)/refit_menu_presentation_test $(BUILD_DIR)/refit_menu_model_test $(BUILD_DIR)/refit_menu_skin_test $(BUILD_DIR)/refit_scene_mailbox_test $(BUILD_DIR)/refit_scene_settings_test
	$(BUILD_DIR)/refit_menu_presentation_test
	$(BUILD_DIR)/refit_menu_model_test
	$(BUILD_DIR)/refit_menu_skin_test
	$(BUILD_DIR)/refit_scene_composition_test
	$(BUILD_DIR)/refit_scene_budget_test
	$(BUILD_DIR)/refit_scene_mailbox_test
	$(BUILD_DIR)/refit_scene_handover_test
	$(BUILD_DIR)/refit_scene_settings_test
	$(BUILD_DIR)/refit_scene_catalogue_test
	$(BUILD_DIR)/refit_setup_layout_test
refit-menus-native-test: refit-menus $(BUILD_DIR)/refit_menu_native.exe
	python3 tools/run_refit_menu_tests.py --prefix "$(REFIT_WINEPREFIX)"

$(CAMERA_CONTROLS_MODULE): modules/A2FOCameraControls/module.cpp \
        $(call a2fo_rust_bridge,modules/A2FOCameraControls/native_bridge.S) $(REFIT_COMMON_HEADERS) | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror $(DLLFLAGS) -o $@ $< \
		$(call a2fo_rust_bridge,modules/A2FOCameraControls/native_bridge.S) -ladvapi32 -lgdi32 $(RUST_WIN_LINK)

$(VIEW_DISTANCE_MODULE): modules/A2FOViewDistance/module.cpp \
        $(call a2fo_rust_bridge,modules/A2FOViewDistance/native_bridge.S) $(REFIT_COMMON_HEADERS) | $(MODULE_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror $(DLLFLAGS) -o $@ $< \
		$(call a2fo_rust_bridge,modules/A2FOViewDistance/native_bridge.S) -ladvapi32 -lgdi32 $(RUST_WIN_LINK)

$(BUILD_DIR)/refit_settings_test: tests/refit_settings_test.cpp modules/refit_common/settings.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -DNDEBUG -o $@ $<

.PHONY: refit-modules refit-modules-test refit-modules-signatures refit-modules-native-test
refit-modules: $(CAMERA_CONTROLS_MODULE) $(VIEW_DISTANCE_MODULE) $(BUILD_DIR)/licenses/armada2-refit.txt

$(BUILD_DIR)/licenses/armada2-refit.txt: third_party/armada2-refit/LICENSE.txt
	mkdir -p $(dir $@)
	cp $< $@

$(BUILD_DIR)/licenses/noto-sans-condensed-OFL.txt: $(REFIT_MENU_FONT_DIR)/LICENSE.txt
	mkdir -p $(dir $@)
	cp $< $@

$(BUILD_DIR)/licenses/noto-sans-condensed-SOURCE.txt: $(REFIT_MENU_FONT_DIR)/SOURCE.txt
	mkdir -p $(dir $@)
	cp $< $@

refit-modules-test: $(BUILD_DIR)/refit_settings_test
	$(BUILD_DIR)/refit_settings_test

$(BUILD_DIR)/refit_module_smoke.exe: tests/refit_module_smoke.cpp \
        tests/refit_native_fixture.S rust/native_hooks_ffi.h $(RUST_WIN_LIB) \
        modules/refit_common/bindings.hpp modules/refit_common/settings.hpp \
        modules/refit_common/status.hpp sdk/include/a2fo_module_api.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror \
		-static -static-libgcc -static-libstdc++ -Wl,--image-base,0x10000000 \
		-o $@ $< tests/refit_native_fixture.S $(RUST_WIN_LINK)

refit-modules-signatures:
	python3 tools/verify_refit_module_signatures.py --exe ArmadaL.exe

# Explicit disposable headless Wine prefix; never use the user's game prefix.
refit-modules-native-test: refit-modules $(BUILD_DIR)/refit_module_smoke.exe
	python3 tools/run_refit_native_tests.py --prefix "$(REFIT_WINEPREFIX)"

$(BUILD_DIR)/refit_scene_composition_test: tests/refit_scene_composition_test.cpp modules/A2FORefitMenus/scene_composition.cpp modules/A2FORefitMenus/scene_composition.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< modules/A2FORefitMenus/scene_composition.cpp $(RUST_HOST_LINK)

.PHONY: refit-scene
refit-scene:
	tools/build_refit_scene.sh

.PHONY: refit-live-menus
refit-live-menus: refit-menus refit-scene

$(BUILD_DIR)/refit_scene_provider.exe: tests/refit_scene_provider.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -static \
		-static-libgcc -static-libstdc++ -o $@ $<

$(BUILD_DIR)/refit_scene_budget_test: tests/refit_scene_budget_test.cpp modules/A2FORefitMenus/scene_budget.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Werror -o $@ $<

$(BUILD_DIR)/refit_scene_catalogue_test: tests/refit_scene_catalogue_test.cpp modules/A2FORefitMenus/scene_catalogue.cpp modules/A2FORefitMenus/scene_catalogue.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< modules/A2FORefitMenus/scene_catalogue.cpp $(RUST_HOST_LINK)

$(MISSION_SELECTOR_MODULE): sdk/include/a2fo_menu_scene.h
$(BUILD_DIR)/mission_selector_scene_bridge_test.exe: tests/mission_selector_scene_bridge_test.cpp modules/A2FOMissionSelector/module.cpp sdk/include/a2fo_menu_scene.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -static -o $@ $< -lcomctl32 -lgdi32 -lgdiplus

.PHONY: renderer-game-tests
renderer-game-tests: $(BUILD_DIR)/renderer_game_native.exe
.PHONY: renderer-depth-tests
renderer-depth-tests: $(BUILD_DIR)/renderer_depth_snapshot_native.exe
$(BUILD_DIR)/renderer_depth_snapshot_native.exe: tests/renderer_depth_snapshot_native.cpp renderer/d3d11/game/backend.cpp renderer/d3d11/game/draw_pipeline.cpp renderer/shaders/bytecode_cache.cpp renderer/textures/upload.cpp renderer/state/fvf.cpp renderer/shaders/vs11.cpp renderer/shaders/d3d8_tokens.cpp renderer/textures/dds.cpp $(D3D11_GAME_HEADERS) $(RUST_WIN_LIB) $(RUST_FFI_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/d3d11/game/backend.cpp renderer/d3d11/game/draw_pipeline.cpp renderer/shaders/bytecode_cache.cpp renderer/textures/upload.cpp renderer/state/fvf.cpp renderer/shaders/vs11.cpp renderer/shaders/d3d8_tokens.cpp renderer/textures/dds.cpp -ld3dcompiler -ldxguid -luuid $(RUST_WIN_LINK)
$(BUILD_DIR)/renderer_game_native.exe: tests/renderer_game_native.cpp $(D3D11_GAME_SOURCES) $(D3D11_GAME_HEADERS) $(D3D11_UI_SOURCES) $(D3D11_UI_HEADERS) renderer/d3d11/texture_handle.cpp $(D3D8_SHADER_SOURCES) renderer/shaders/vs11.cpp $(RUST_WIN_LIB) $(RUST_FFI_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(D3D11_GAME_SOURCES) $(D3D11_UI_SOURCES) renderer/d3d11/texture_handle.cpp $(D3D8_SHADER_SOURCES) renderer/shaders/vs11.cpp -ld3d8 -ldxguid -luuid $(RUST_WIN_LINK)

# Startup route selection only: synthetic imports and COM factory, no GPU.
$(BUILD_DIR)/d3d8_game_activation_test.exe: tests/d3d8_game_activation_test.cpp renderer/d3d8_compat/observer.cpp renderer/d3d8_compat/observer.hpp $(D3D11_GAME_SOURCES) $(D3D11_GAME_HEADERS) $(D3D11_UI_SOURCES) $(D3D11_UI_HEADERS) renderer/d3d11/texture_handle.cpp $(D3D8_SHADER_SOURCES) renderer/shaders/vs11.cpp core/extension_roots.cpp core/extension_roots.hpp $(RUST_WIN_LIB) $(RUST_FFI_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -fno-strict-aliasing -static -o $@ $< $(D3D11_GAME_SOURCES) $(D3D11_UI_SOURCES) renderer/d3d11/texture_handle.cpp $(D3D8_SHADER_SOURCES) renderer/shaders/vs11.cpp core/extension_roots.cpp -ldxguid -luuid $(RUST_WIN_LINK)

$(BUILD_DIR)/renderer_sampler_native.exe: tests/renderer_sampler_native.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< -ld3d8 -ldxguid -luuid

$(BUILD_DIR)/renderer_alpha_native.exe: tests/renderer_alpha_native.cpp $(D3D11_GAME_SOURCES) $(D3D11_GAME_HEADERS) $(D3D11_UI_SOURCES) $(D3D11_UI_HEADERS) renderer/d3d11/texture_handle.cpp $(D3D8_SHADER_SOURCES) renderer/shaders/vs11.cpp $(RUST_WIN_LIB) $(RUST_FFI_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(D3D11_GAME_SOURCES) $(D3D11_UI_SOURCES) renderer/d3d11/texture_handle.cpp $(D3D8_SHADER_SOURCES) renderer/shaders/vs11.cpp -ld3d8 -ldxguid -luuid $(RUST_WIN_LINK)

# Built-in modules: focused checks do not depend on legacy test sources.
BUILTIN_NATIVE_SOURCES := core/builtin_modules.cpp core/builtin_state.cpp $(BUILTIN_REFIT_SOURCES) $(BUILTIN_INSTANT_SOURCES) $(BUILTIN_BATCH7_SOURCES) $(BUILTIN_BATCH8_SOURCES) $(BUILTIN_BATCH9_SOURCES) $(BUILTIN_BATCH10_SOURCES) modules/A2FORecoveryTests/recovery_tests.cpp \
    core/builtin_wreckage.cpp core/builtin_normal_weapon_tech.cpp core/builtin_build_tooltips.cpp \
    core/builtin_info_ini.cpp core/builtin_muzzle_flashes.cpp core/extension_roots.cpp \
    core/builtin_team_change_weapons.cpp core/builtin_weapon_damage_controls.cpp \
    core/builtin_a1_fallbacks.cpp core/builtin_animated_hardpoints.cpp \
    modules/A1Fallbacks/fallback_policy.cpp $(call a2fo_rust_bridge,modules/A1Fallbacks/thiscall_bridge.S) \
    $(call a2fo_rust_bridge,modules/A2FOAnimatedHardpoints/thiscall_bridge.S) \
    $(call a2fo_rust_bridge,modules/A2FOTeamChangeWeapons/thiscall_bridge.S) $(call a2fo_rust_bridge,modules/A2FOWeaponDamageControls/thiscall_bridge.S) \
    $(call a2fo_rust_bridge,modules/A2FOMuzzleFlashes/thiscall_bridge.S) \
    core/module_loader.cpp core/module_policy.cpp \
    modules/A2FOWreckage/wreckage_policy.cpp $(call a2fo_rust_bridge,modules/A2FONormalWeaponTech/delphi_bridge.S) \
    modules/A2FOBuildTooltips/build_time_text.cpp $(call a2fo_rust_bridge,modules/A2FOBuildTooltips/thiscall_bridge.S)
BUILTIN_NATIVE_HEADERS := core/builtin_modules.hpp core/builtin_state.hpp $(BUILTIN_REFIT_HEADERS) $(BUILTIN_INSTANT_HEADERS) $(BUILTIN_BATCH7_HEADERS) $(BUILTIN_BATCH8_HEADERS) $(BUILTIN_BATCH9_HEADERS) $(BUILTIN_BATCH10_HEADERS) \
    core/module_loader.hpp core/module_policy.hpp sdk/include/a2fo_module_api.h \
    sdk/include/a2fo_supported_armada.hpp modules/A2FOBuildTooltips/build_time_text.hpp \
    modules/A2FOResources/api.hpp modules/A2FOWreckage/wreckage_policy.hpp \
    modules/A2FOWreckage/module.cpp modules/A2FONormalWeaponTech/module.cpp \
    modules/A2FOBuildTooltips/module.cpp core/extension_roots.hpp \
    modules/A2FOInfoIni/module.cpp modules/A2FOMuzzleFlashes/module.cpp modules/A2FOAnimations/api.hpp \
    modules/A2FOTeamChangeWeapons/module.cpp modules/A2FOWeaponDamageControls/module.cpp \
    modules/A2FOWeaponDamageControls/damage_controls.hpp modules/A2FODirectionalShields/api.hpp \
    modules/A1Fallbacks/module.cpp modules/A1Fallbacks/fallback_policy.hpp \
    modules/A2FOAnimatedHardpoints/module.cpp
$(BUILD_DIR)/builtin-native/modules/A2FORecoveryTests/recovery_tests.cpp.o: CXXFLAGS += $(BUILTIN_RECOVERY_FPFLAGS)
BUILTIN_NATIVE_OBJECTS := $(foreach source,$(BUILTIN_NATIVE_SOURCES),$(if $(filter %.o,$(source)),$(source),$(BUILD_DIR)/builtin-native/$(source).o)) $(BUILTIN_BATCH10_SUPPORT_OBJECTS) $(RUST_WIN_LIB)
BUILTIN_NATIVE_WARNING_FLAGS = -Werror
BUILTIN_BATCH10_NATIVE_OBJECTS := $(foreach source,$(BUILTIN_BATCH10_SOURCES),$(if $(filter %.o,$(source)),$(source),$(BUILD_DIR)/builtin-native/$(source).o))
$(BUILTIN_BATCH10_NATIVE_OBJECTS): BUILTIN_NATIVE_WARNING_FLAGS :=
$(BUILD_DIR)/builtin-native/core/builtin_refit_menus.cpp.o $(addprefix $(BUILD_DIR)/builtin-native/,$(addsuffix .o,$(filter modules/A2FORefitMenus/%,$(BUILTIN_BATCH10_SOURCES)))): CPPFLAGS += -DNOMINMAX
$(BUILD_DIR)/builtin-native/%.cpp.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(BUILTIN_NATIVE_WARNING_FLAGS) -MMD -MP -MF $@.d -c -o $@ $<
$(BUILD_DIR)/builtin-native/%.S.o: %.S
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(BUILTIN_NATIVE_WARNING_FLAGS) -MMD -MP -MF $@.d -c -o $@ $<
-include $(addsuffix .d,$(BUILTIN_NATIVE_OBJECTS))
A2FO_BUILTIN_WINEPREFIX ?= /tmp/a2fo-builtin-tests-wine
.PHONY: builtin-tests builtin-native-tests verify-builtins
$(BUILD_DIR)/builtin_state_test: tests/builtin_state_test.cpp core/builtin_state.cpp core/builtin_state.hpp $(RUST_HOST_LIB) | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -pthread -o $@ tests/builtin_state_test.cpp core/builtin_state.cpp $(RUST_HOST_LINK)
builtin-tests: $(BUILD_DIR)/builtin_state_test
	$(BUILD_DIR)/builtin_state_test
$(BUILD_DIR)/builtin_modules_native_test.exe: tests/builtin_modules_native_test.cpp $(BUILTIN_NATIVE_OBJECTS) $(BUILTIN_NATIVE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ tests/builtin_modules_native_test.cpp $(BUILTIN_NATIVE_OBJECTS) -ladvapi32 -lgdi32 $(BUILTIN_BATCH10_LIBS)
$(BUILD_DIR)/builtin_optional_native_test.exe: tests/builtin_optional_native_test.cpp $(BUILTIN_NATIVE_OBJECTS) $(BUILTIN_NATIVE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ tests/builtin_optional_native_test.cpp $(BUILTIN_NATIVE_OBJECTS) -ladvapi32 -lgdi32 $(BUILTIN_BATCH10_LIBS)
$(BUILD_DIR)/builtin_core_defaults_native_test.exe: tests/builtin_core_defaults_native_test.cpp $(BUILD_DIR)/A2FOExtensions.dll $(BUILTIN_NATIVE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -static -o $@ $< $(filter-out $(BUILD_DIR)/core-objects/core/dllmain.cpp.o,$(CORE_OBJECTS)) -lcomctl32 -lgdi32 -ldxguid -ladvapi32 $(BUILTIN_BATCH10_LIBS)
$(BUILD_DIR)/builtin_combat_native_test.exe: tests/builtin_combat_native_test.cpp $(BUILTIN_NATIVE_OBJECTS) $(BUILTIN_NATIVE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(BUILTIN_NATIVE_OBJECTS) -ladvapi32 -lgdi32 $(BUILTIN_BATCH10_LIBS)
$(BUILD_DIR)/builtin_visual_native_test.exe: tests/builtin_visual_native_test.cpp $(BUILTIN_NATIVE_OBJECTS) $(BUILTIN_NATIVE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(BUILTIN_NATIVE_OBJECTS) -ladvapi32 -lgdi32 $(BUILTIN_BATCH10_LIBS)
$(BUILD_DIR)/builtin_refit_native_test.exe: tests/builtin_refit_native_test.cpp tests/refit_module_smoke.cpp tests/refit_native_fixture.S rust/native_hooks_ffi.h $(RUST_WIN_LIB) $(BUILTIN_NATIVE_OBJECTS) $(BUILTIN_NATIVE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -Wl,--image-base,0x10000000 -o $@ $< tests/refit_native_fixture.S $(BUILTIN_NATIVE_OBJECTS) -ladvapi32 -lgdi32 $(BUILTIN_BATCH10_LIBS)
$(BUILD_DIR)/builtin_instant_action_native_test.exe: tests/builtin_instant_action_native_test.cpp $(BUILTIN_NATIVE_OBJECTS) $(BUILTIN_NATIVE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(filter-out $(BUILD_DIR)/builtin-native/core/builtin_instant_action_settings.cpp.o,$(BUILTIN_NATIVE_OBJECTS)) -ladvapi32 -lgdi32 $(BUILTIN_BATCH10_LIBS)
$(BUILD_DIR)/builtin_edit_menu_native_test.exe: tests/builtin_edit_menu_native_test.cpp $(BUILTIN_NATIVE_OBJECTS) $(BUILTIN_NATIVE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(filter-out $(BUILD_DIR)/builtin-native/core/builtin_edit_menu.cpp.o,$(BUILTIN_NATIVE_OBJECTS)) -ladvapi32 -lgdi32 $(BUILTIN_BATCH10_LIBS)
$(BUILD_DIR)/builtin_m3d_native_test.exe: tests/builtin_m3d_native_test.cpp $(BUILTIN_NATIVE_OBJECTS) $(BUILTIN_NATIVE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(filter-out $(BUILD_DIR)/builtin-native/core/builtin_m3d.cpp.o,$(BUILTIN_NATIVE_OBJECTS)) -ladvapi32 -lgdi32 $(BUILTIN_BATCH10_LIBS)
$(BUILD_DIR)/builtin_breeders_native_test.exe: tests/builtin_breeders_native_test.cpp $(BUILTIN_NATIVE_OBJECTS) $(BUILTIN_NATIVE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(filter-out $(BUILD_DIR)/builtin-native/core/builtin_breeders.cpp.o,$(BUILTIN_NATIVE_OBJECTS)) -ladvapi32 -lgdi32 $(BUILTIN_BATCH10_LIBS)
$(BUILD_DIR)/builtin_infestation_native_test.exe: tests/builtin_infestation_native_test.cpp $(BUILTIN_NATIVE_OBJECTS) $(BUILTIN_NATIVE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(filter-out $(BUILD_DIR)/builtin-native/core/builtin_infestation.cpp.o,$(BUILTIN_NATIVE_OBJECTS)) -ladvapi32 -lgdi32 $(BUILTIN_BATCH10_LIBS)
$(BUILD_DIR)/builtin_swarm_system_native_test.exe: tests/builtin_swarm_native_test.cpp $(BUILTIN_NATIVE_OBJECTS) $(BUILTIN_NATIVE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(filter-out $(BUILD_DIR)/builtin-native/core/builtin_swarm_system.cpp.o,$(BUILTIN_NATIVE_OBJECTS)) -ladvapi32 -lgdi32 $(BUILTIN_BATCH10_LIBS)
$(BUILD_DIR)/builtin_odf_variants_native_test.exe: tests/builtin_odf_variants_native_test.cpp $(BUILTIN_NATIVE_OBJECTS) $(BUILTIN_NATIVE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(filter-out $(BUILD_DIR)/builtin-native/core/builtin_odf_variants.cpp.o,$(BUILTIN_NATIVE_OBJECTS)) -ladvapi32 -lgdi32 $(BUILTIN_BATCH10_LIBS)
$(BUILD_DIR)/fixtures/builtin-producer/A2FOFeaturePack.dll: tests/builtin_producer_provider_fixture.cpp | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror $(DLLFLAGS) -o $@ $<
$(BUILD_DIR)/builtin_batch7_native_test.exe: tests/builtin_batch7_native_test.cpp $(BUILTIN_NATIVE_OBJECTS) $(BUILTIN_NATIVE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(BUILTIN_NATIVE_OBJECTS) -ladvapi32 -lgdi32 $(BUILTIN_BATCH10_LIBS)
$(BUILD_DIR)/fixtures/builtin-directional/A2FODirectionalShields.dll: tests/builtin_directional_bridge_fixture.cpp modules/A2FODirectionalShields/api.hpp sdk/include/a2fo_module_api.h | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror $(DLLFLAGS) -o $@ $<

# Stage-eight synthetic native fixtures; no game startup or GUI/input.
$(BUILD_DIR)/builtin_fire_arcs_native_test.exe: tests/builtin_fire_arcs_native_test.cpp $(BUILTIN_NATIVE_OBJECTS) $(BUILTIN_NATIVE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(filter-out $(BUILD_DIR)/builtin-native/core/builtin_fire_arcs.cpp.o,$(BUILTIN_NATIVE_OBJECTS)) -ladvapi32 -lgdi32 $(BUILTIN_BATCH10_LIBS)
$(BUILD_DIR)/builtin_animations_native_test.exe: tests/builtin_animations_native_test.cpp $(BUILTIN_NATIVE_OBJECTS) $(BUILTIN_NATIVE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(filter-out $(BUILD_DIR)/builtin-native/core/builtin_animations.cpp.o,$(BUILTIN_NATIVE_OBJECTS)) -ladvapi32 -lgdi32 $(BUILTIN_BATCH10_LIBS)
$(BUILD_DIR)/builtin_always_shields_native_test.exe: tests/builtin_always_shields_native_test.cpp $(BUILTIN_NATIVE_OBJECTS) $(BUILTIN_NATIVE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(filter-out $(BUILD_DIR)/builtin-native/core/builtin_always_shields.cpp.o,$(BUILTIN_NATIVE_OBJECTS)) -ladvapi32 -lgdi32 $(BUILTIN_BATCH10_LIBS)
$(BUILD_DIR)/builtin_station_rotation_native_test.exe: tests/builtin_station_rotation_native_test.cpp $(BUILTIN_NATIVE_OBJECTS) $(BUILTIN_NATIVE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(filter-out $(BUILD_DIR)/builtin-native/core/builtin_station_rotation.cpp.o,$(BUILTIN_NATIVE_OBJECTS)) -ladvapi32 -lgdi32 $(BUILTIN_BATCH10_LIBS)
$(BUILD_DIR)/builtin_radar_diagnostics_native_test.exe: tests/builtin_radar_diagnostics_native_test.cpp $(BUILTIN_NATIVE_OBJECTS) $(BUILTIN_NATIVE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(filter-out $(BUILD_DIR)/builtin-native/core/builtin_radar_diagnostics.cpp.o,$(BUILTIN_NATIVE_OBJECTS)) -ladvapi32 -lgdi32 $(BUILTIN_BATCH10_LIBS)
$(BUILD_DIR)/builtin_recovery_tests_native_test.exe: tests/builtin_recovery_tests_native_test.cpp $(BUILTIN_NATIVE_OBJECTS) $(BUILTIN_NATIVE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(filter-out $(BUILD_DIR)/builtin-native/core/builtin_recovery_tests.cpp.o,$(BUILTIN_NATIVE_OBJECTS)) -ladvapi32 -lgdi32 $(BUILTIN_BATCH10_LIBS)
$(BUILD_DIR)/builtin_batch8_native_test.exe: tests/builtin_batch8_native_test.cpp $(BUILTIN_NATIVE_OBJECTS) $(BUILTIN_NATIVE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(BUILTIN_NATIVE_OBJECTS) -ladvapi32 -lgdi32 $(BUILTIN_BATCH10_LIBS)
builtin-native-tests: $(BUILD_DIR)/builtin_modules_native_test.exe $(BUILD_DIR)/builtin_edit_menu_native_test.exe $(BUILD_DIR)/builtin_m3d_native_test.exe $(BUILD_DIR)/builtin_breeders_native_test.exe $(BUILD_DIR)/builtin_infestation_native_test.exe $(BUILD_DIR)/builtin_swarm_system_native_test.exe $(BUILD_DIR)/builtin_odf_variants_native_test.exe $(BUILD_DIR)/builtin_batch7_native_test.exe $(BUILD_DIR)/fixtures/builtin-producer/A2FOFeaturePack.dll $(BUILD_DIR)/modules/A2FOInstantActionSettings.dll $(BUILD_DIR)/builtin_instant_action_native_test.exe $(BUILD_DIR)/builtin_refit_native_test.exe $(BUILD_DIR)/builtin_optional_native_test.exe $(BUILD_DIR)/builtin_core_defaults_native_test.exe $(BUILD_DIR)/builtin_combat_native_test.exe $(BUILD_DIR)/builtin_visual_native_test.exe $(BUILD_DIR)/fixtures/builtin-directional/A2FODirectionalShields.dll $(BUILD_DIR)/builtin_fire_arcs_native_test.exe $(BUILD_DIR)/builtin_animations_native_test.exe $(BUILD_DIR)/builtin_always_shields_native_test.exe $(BUILD_DIR)/builtin_station_rotation_native_test.exe $(BUILD_DIR)/builtin_radar_diagnostics_native_test.exe $(BUILD_DIR)/builtin_recovery_tests_native_test.exe $(BUILD_DIR)/builtin_batch8_native_test.exe
	@mkdir -p "$(A2FO_BUILTIN_WINEPREFIX)"
	@for mode in success legacy partial mismatch dispatch-fail rejected; do \
		echo "Built-in native fixture: $$mode"; \
		timeout 45s env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$(A2FO_BUILTIN_WINEPREFIX)" WINEDEBUG=-all \
		WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d" wine $< $$mode || exit $$?; \
	done
	@for mode in success legacy rejected dispatch-fail info-dispatch-fail muzzle-dispatch-fail signature-fail hook-fail info-registration-fail weapon-registration-fail; do \
		echo "Optional built-in native fixture: $$mode"; \
		timeout 45s env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$(A2FO_BUILTIN_WINEPREFIX)" WINEDEBUG=-all \
		WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d" wine $(BUILD_DIR)/builtin_optional_native_test.exe $$mode || exit $$?; \
	done
	@timeout 45s env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$(A2FO_BUILTIN_WINEPREFIX)" WINEDEBUG=-all \
		WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d" wine $(BUILD_DIR)/builtin_core_defaults_native_test.exe
	@for mode in success legacy rejected signature-fail dispatch-fail craft-dispatch-fail registration-fail hook-fail; do \
		echo "Combat built-in native fixture: $$mode"; \
		timeout 45s env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$(A2FO_BUILTIN_WINEPREFIX)" WINEDEBUG=-all \
		WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d" wine $(BUILD_DIR)/builtin_combat_native_test.exe $$mode $(BUILD_DIR)/fixtures/builtin-directional/A2FODirectionalShields.dll || exit $$?; \
	done
	@for mode in success legacy rejected signature-fail hook-fail partial identity-fail; do \
		echo "Visual built-in native fixture: $$mode"; \
		timeout 45s env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$(A2FO_BUILTIN_WINEPREFIX)" WINEDEBUG=-all \
		WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d" wine $(BUILD_DIR)/builtin_visual_native_test.exe $$mode || exit $$?; \
	done
	@for mode in success missing disabled noop invalid identity relocation foreign context patch-fail api partial rejected required legacy; do \
		echo "Camera/view built-in native fixture: $$mode"; \
		timeout 45s env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$(A2FO_BUILTIN_WINEPREFIX)" WINEDEBUG=-all \
		WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d" wine $(BUILD_DIR)/builtin_refit_native_test.exe ArmadaL.exe $$mode || exit $$?; \
	done
	@for mode in success identity signature api fleet fail1 fail2 fail3 fail4 fail5 required rejected legacy; do \
		echo "Instant Action built-in native fixture: $$mode"; \
		timeout 45s env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$(A2FO_BUILTIN_WINEPREFIX)" WINEDEBUG=-all \
		WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d" wine $(BUILD_DIR)/builtin_instant_action_native_test.exe $$mode || exit $$?; \
	done
	@for mode in success signature hook-fail; do \
		echo "Stage7 edit_menu native fixture: $$mode"; \
		timeout 45s env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$(A2FO_BUILTIN_WINEPREFIX)" WINEDEBUG=-all \
		WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d" wine $(BUILD_DIR)/builtin_edit_menu_native_test.exe $$mode || exit $$?; \
	done
	@for mode in success signature partial; do \
		echo "Stage7 m3d native fixture: $$mode"; \
		timeout 45s env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$(A2FO_BUILTIN_WINEPREFIX)" WINEDEBUG=-all \
		WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d" wine $(BUILD_DIR)/builtin_m3d_native_test.exe $$mode || exit $$?; \
	done
	@for mode in success signature registration dispatch; do \
		echo "Stage7 breeders native fixture: $$mode"; \
		timeout 45s env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$(A2FO_BUILTIN_WINEPREFIX)" WINEDEBUG=-all \
		WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d" wine $(BUILD_DIR)/builtin_breeders_native_test.exe $$mode || exit $$?; \
	done
	@for mode in success signature registration dispatch; do \
		echo "Stage7 infestation native fixture: $$mode"; \
		timeout 45s env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$(A2FO_BUILTIN_WINEPREFIX)" WINEDEBUG=-all \
		WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d" wine $(BUILD_DIR)/builtin_infestation_native_test.exe $$mode || exit $$?; \
	done
	@for mode in success signature partial; do \
		echo "Stage7 swarm_system native fixture: $$mode"; \
		timeout 45s env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$(A2FO_BUILTIN_WINEPREFIX)" WINEDEBUG=-all \
		WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d" wine $(BUILD_DIR)/builtin_swarm_system_native_test.exe $$mode || exit $$?; \
	done
	@for mode in success signature registration dispatch; do \
		echo "Stage7 odf_variants native fixture: $$mode"; \
		timeout 45s env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$(A2FO_BUILTIN_WINEPREFIX)" WINEDEBUG=-all \
		WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d" wine $(BUILD_DIR)/builtin_odf_variants_native_test.exe $$mode || exit $$?; \
	done
	@for mode in inventory rejected provider; do \
		echo "Stage7 loader native fixture: $$mode"; \
		timeout 45s env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$(A2FO_BUILTIN_WINEPREFIX)" WINEDEBUG=-all \
		WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d" wine $(BUILD_DIR)/builtin_batch7_native_test.exe $$mode || exit $$?; \
	done
	@for mode in success signature partial dispatch; do \
		echo "Stage8 fire_arcs native fixture: $$mode"; \
		timeout 45s env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$(A2FO_BUILTIN_WINEPREFIX)" WINEDEBUG=-all \
		WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d" wine $(BUILD_DIR)/builtin_fire_arcs_native_test.exe $$mode || exit $$?; \
	done
	@for mode in success signature partial dispatch; do \
		echo "Stage8 animations native fixture: $$mode"; \
		timeout 45s env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$(A2FO_BUILTIN_WINEPREFIX)" WINEDEBUG=-all \
		WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d" wine $(BUILD_DIR)/builtin_animations_native_test.exe $$mode || exit $$?; \
	done
	@for mode in success signature partial; do \
		echo "Stage8 always_shields native fixture: $$mode"; \
		timeout 45s env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$(A2FO_BUILTIN_WINEPREFIX)" WINEDEBUG=-all \
		WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d" wine $(BUILD_DIR)/builtin_always_shields_native_test.exe $$mode || exit $$?; \
	done
	@for mode in success signature partial; do \
		echo "Stage8 station_rotation native fixture: $$mode"; \
		timeout 45s env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$(A2FO_BUILTIN_WINEPREFIX)" WINEDEBUG=-all \
		WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d" wine $(BUILD_DIR)/builtin_station_rotation_native_test.exe $$mode || exit $$?; \
	done
	@for mode in passive success signature partial; do \
		echo "Stage8 radar_diagnostics native fixture: $$mode"; \
		timeout 45s env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$(A2FO_BUILTIN_WINEPREFIX)" WINEDEBUG=-all \
		WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d" wine $(BUILD_DIR)/builtin_radar_diagnostics_native_test.exe $$mode || exit $$?; \
	done
	@for mode in passive unsupported success; do \
		echo "Stage8 recovery_tests native fixture: $$mode"; \
		timeout 45s env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$(A2FO_BUILTIN_WINEPREFIX)" WINEDEBUG=-all \
		WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d" wine $(BUILD_DIR)/builtin_recovery_tests_native_test.exe $$mode || exit $$?; \
	done
	@for mode in inventory rejected; do \
		echo "Stage8 batch8 native fixture: $$mode"; \
		timeout 45s env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$(A2FO_BUILTIN_WINEPREFIX)" WINEDEBUG=-all \
		WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d" wine $(BUILD_DIR)/builtin_batch8_native_test.exe $$mode || exit $$?; \
	done
	@for mode in core fallback incomplete; do \
		echo "Stage8 animation_bridge native fixture: $$mode"; \
		timeout 45s env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$(A2FO_BUILTIN_WINEPREFIX)" WINEDEBUG=-all \
		WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d" wine $(BUILD_DIR)/builtin_animation_bridge_native_test.exe $$mode || exit $$?; \
	done
	@for mode in core fallback; do \
		echo "Stage8 muzzle_animation_bridge native fixture: $$mode"; \
		timeout 45s env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$(A2FO_BUILTIN_WINEPREFIX)" WINEDEBUG=-all \
		WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d" wine $(BUILD_DIR)/builtin_muzzle_animation_bridge_native_test.exe $$mode || exit $$?; \
	done
verify-builtins: $(BUILD_DIR)/A2FOExtensions.dll
	python3 tools/verify_builtin_modules.py --objdump $(OBJDUMP) $<

.PHONY: builtin-stage7-build
builtin-stage7-build: $(BUILD_DIR)/builtin_modules_native_test.exe $(BUILD_DIR)/builtin_edit_menu_native_test.exe $(BUILD_DIR)/builtin_m3d_native_test.exe $(BUILD_DIR)/builtin_breeders_native_test.exe $(BUILD_DIR)/builtin_infestation_native_test.exe $(BUILD_DIR)/builtin_swarm_system_native_test.exe $(BUILD_DIR)/builtin_odf_variants_native_test.exe $(BUILD_DIR)/builtin_batch7_native_test.exe $(BUILD_DIR)/fixtures/builtin-producer/A2FOFeaturePack.dll $(BUILD_DIR)/modules/A2FOInstantActionSettings.dll $(BUILD_DIR)/builtin_instant_action_native_test.exe $(BUILD_DIR)/builtin_refit_native_test.exe $(BUILD_DIR)/builtin_optional_native_test.exe $(BUILD_DIR)/builtin_core_defaults_native_test.exe $(BUILD_DIR)/builtin_combat_native_test.exe $(BUILD_DIR)/builtin_visual_native_test.exe $(BUILD_DIR)/fixtures/builtin-directional/A2FODirectionalShields.dll $(BUILD_DIR)/A2FOExtensions.dll $(MODULE_DIR)/A2FOFeaturePack.dll $(EDIT_MENU_MODULE) $(M3D_MODULE) $(BREEDERS_MODULE) $(INFESTATION_MODULE) $(SWARM_SYSTEM_MODULE) $(ODF_VARIANTS_MODULE) $(BUILD_DIR)/refit_loading_native.exe $(BUILD_DIR)/builtin_state_test

.PHONY: builtin-stage8-build
builtin-stage8-build: builtin-stage7-build $(BUILD_DIR)/builtin_fire_arcs_native_test.exe $(BUILD_DIR)/builtin_animations_native_test.exe $(BUILD_DIR)/builtin_always_shields_native_test.exe $(BUILD_DIR)/builtin_station_rotation_native_test.exe $(BUILD_DIR)/builtin_radar_diagnostics_native_test.exe $(BUILD_DIR)/builtin_recovery_tests_native_test.exe $(BUILD_DIR)/builtin_batch8_native_test.exe $(ALWAYS_SHOW_SHIELDS_MODULE) $(ANIMATIONS_MODULE) $(FIRE_ARCS_MODULE) $(STATION_ROTATION_MODULE) $(RECOVERY_TESTS_MODULE) $(MODULE_DIR)/A2FORadarDiagnostics.dll $(CRAFT_IDENTITY_MODULE) $(TURRETS_MODULE) $(MODULE_DIR)/A2FOHybridBuild.dll

BUILTIN_ANIMATION_BRIDGE_PROVIDERS := $(BUILD_DIR)/fixtures/builtin-animation/core/A2FOExtensions.dll $(BUILD_DIR)/fixtures/builtin-animation/missing/A2FOExtensions.dll $(BUILD_DIR)/fixtures/builtin-animation/standalone/A2FOAnimations.dll
$(BUILD_DIR)/fixtures/builtin-animation/core/A2FOExtensions.dll: tests/builtin_animation_bridge_provider.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror $(DLLFLAGS) -DFIXTURE_TAG=1 -o $@ $<
$(BUILD_DIR)/fixtures/builtin-animation/missing/A2FOExtensions.dll: tests/builtin_animation_bridge_provider.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror $(DLLFLAGS) -DFIXTURE_TAG=3 -DFIXTURE_OMIT_CONTROLLED -o $@ $<
$(BUILD_DIR)/fixtures/builtin-animation/standalone/A2FOAnimations.dll: tests/builtin_animation_bridge_provider.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror $(DLLFLAGS) -DFIXTURE_TAG=2 -o $@ $<
$(BUILD_DIR)/builtin_animation_bridge_native_test.exe: tests/builtin_animation_bridge_native_test.cpp core/builtin_animated_hardpoints.cpp modules/A2FOAnimatedHardpoints/module.cpp $(BUILTIN_ANIMATION_BRIDGE_PROVIDERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(call a2fo_rust_bridge,modules/A2FOAnimatedHardpoints/thiscall_bridge.S) $(RUST_WIN_LINK)
$(BUILD_DIR)/builtin_muzzle_animation_bridge_native_test.exe: tests/builtin_muzzle_animation_bridge_native_test.cpp core/builtin_muzzle_flashes.cpp modules/A2FOMuzzleFlashes/module.cpp $(BUILTIN_ANIMATION_BRIDGE_PROVIDERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(call a2fo_rust_bridge,modules/A2FOMuzzleFlashes/thiscall_bridge.S) $(RUST_WIN_LINK)
builtin-native-tests: $(BUILD_DIR)/builtin_animation_bridge_native_test.exe $(BUILD_DIR)/builtin_muzzle_animation_bridge_native_test.exe
builtin-stage8-build: $(BUILD_DIR)/builtin_animation_bridge_native_test.exe $(BUILD_DIR)/builtin_muzzle_animation_bridge_native_test.exe

# All Windows module consumers must rebuild when their inline engine admission
# contract changes. Exact per-feature signatures remain in each consumer.
ENGINE_PROFILE_HEADERS := sdk/include/a2fo_supported_armada.hpp sdk/include/a2fo_supported_fleetops.hpp
ENGINE_PROFILE_MODULES := $(patsubst modules/%/module.cpp,$(MODULE_DIR)/%.dll,$(wildcard modules/*/module.cpp)) $(MODULE_DIR)/A2FOFeaturePack.dll
$(ENGINE_PROFILE_MODULES) $(BUILD_DIR)/A2FOExtensions.dll: $(ENGINE_PROFILE_HEADERS)
$(BUILD_DIR)/engine_profiles_native_test.exe: tests/engine_profiles_native_test.cpp $(ENGINE_PROFILE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $<
.PHONY: engine-compat-build
engine-compat-build: $(BUILD_DIR)/A2FOExtensions.dll $(ENGINE_PROFILE_MODULES) $(BUILD_DIR)/engine_profiles_native_test.exe $(BUILD_DIR)/roots_profile_smoke.exe

.PHONY: builtin-stage9-build
builtin-stage9-build: builtin-stage8-build $(BUILD_DIR)/builtin_startup_policy_test.exe $(BUILD_DIR)/builtin_provider_test.exe
$(BUILD_DIR)/builtin_startup_policy_test.exe: tests/builtin_startup_policy_test.cpp tests/builtin_startup_stubs.cpp core/builtin_modules.cpp core/builtin_state.cpp core/builtin_startup_modules.cpp core/builtin_modules.hpp core/builtin_state.hpp core/builtin_startup_modules.hpp $(RUST_WIN_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ tests/builtin_startup_policy_test.cpp tests/builtin_startup_stubs.cpp core/builtin_modules.cpp core/builtin_state.cpp core/builtin_startup_modules.cpp $(RUST_WIN_LINK)
BUILTIN_PROVIDER_DIR := $(BUILD_DIR)/fixtures/builtin-provider
$(BUILTIN_PROVIDER_DIR)/partial/A2FOExtensions.dll: tests/builtin_provider_test.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) -DA2FO_PROVIDER_FIXTURE_CORE -Wl,--out-implib,$(dir $@)libcore.a -o $@ $<
$(BUILTIN_PROVIDER_DIR)/complete/A2FOExtensions.dll: tests/builtin_provider_test.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) -DA2FO_PROVIDER_FIXTURE_CORE -DA2FO_PROVIDER_FIXTURE_CORE_COMPLETE -o $@ $<
$(BUILTIN_PROVIDER_DIR)/partial/A2FOStandalone.dll: tests/builtin_provider_test.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) -DA2FO_PROVIDER_FIXTURE_STANDALONE -Wl,--out-implib,$(dir $@)libstandalone.a -o $@ $<
$(BUILD_DIR)/builtin_provider_test.exe: tests/builtin_provider_test.cpp sdk/include/a2fo_module_provider.hpp $(BUILTIN_PROVIDER_DIR)/partial/A2FOExtensions.dll $(BUILTIN_PROVIDER_DIR)/partial/A2FOStandalone.dll $(BUILTIN_PROVIDER_DIR)/complete/A2FOExtensions.dll | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(BUILTIN_PROVIDER_DIR)/partial/libcore.a $(BUILTIN_PROVIDER_DIR)/partial/libstandalone.a

$(BUILD_DIR)/A2FOExtensions.dll $(CRAFT_IDENTITY_MODULE) $(MODULE_DIR)/A2FOFeaturePack.dll $(ODF_VARIANTS_MODULE) $(WEAPON_DAMAGE_CONTROLS_MODULE) $(REFIT_YARDS_MODULE): sdk/include/a2fo_module_provider.hpp

$(BUILD_DIR)/builtin_batch9_native_test.exe: tests/builtin_batch9_native_test.cpp $(BUILTIN_NATIVE_OBJECTS) $(BUILTIN_NATIVE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(BUILTIN_NATIVE_OBJECTS) -ladvapi32 -lgdi32 $(BUILTIN_BATCH10_LIBS)
builtin-stage9-build: $(BUILD_DIR)/builtin_batch9_native_test.exe

.PHONY: builtin-stage9-tests
builtin-stage9-tests: builtin-stage9-build builtin-native-tests
	@for mode in off on; do \
		timeout 45s env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$(A2FO_BUILTIN_WINEPREFIX)" WINEDEBUG=-all \
		WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d" wine $(BUILD_DIR)/builtin_startup_policy_test.exe $$mode || exit $$?; \
	done
	@for index in 0 1 2 3 4 5 6 7 8 9 10 11; do \
		timeout 45s env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$(A2FO_BUILTIN_WINEPREFIX)" WINEDEBUG=-all \
		WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d" wine $(BUILD_DIR)/builtin_startup_policy_test.exe fail $$index || exit $$?; \
	done
	@timeout 45s env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$(A2FO_BUILTIN_WINEPREFIX)" WINEDEBUG=-all \
		WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d" wine $(BUILD_DIR)/builtin_batch9_native_test.exe
	@set -e; for provider in partial complete; do \
		directory="$(BUILTIN_PROVIDER_DIR)/run-$$provider"; mkdir -p "$$directory"; \
		cp $(BUILD_DIR)/builtin_provider_test.exe "$$directory/provider_test.exe"; \
		cp "$(BUILTIN_PROVIDER_DIR)/$$provider/A2FOExtensions.dll" "$$directory/A2FOExtensions.dll"; \
		cp $(BUILTIN_PROVIDER_DIR)/partial/A2FOStandalone.dll "$$directory/A2FOStandalone.dll"; \
		timeout 45s env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX="$(A2FO_BUILTIN_WINEPREFIX)" WINEDEBUG=-all \
		WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d" wine "$$directory/provider_test.exe" || exit $$?; \
	done

.PHONY: builtin-stage10-build
builtin-stage10-build: builtin-stage9-build $(MMM_LOADER_SHIM) $(BUILD_DIR)/builtin_batch10_native_test.exe
$(BUILD_DIR)/builtin_batch10_native_test.exe: tests/builtin_batch10_native_test.cpp $(BUILTIN_NATIVE_OBJECTS) $(BUILTIN_NATIVE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(BUILTIN_NATIVE_OBJECTS) -ladvapi32 -lgdi32 $(BUILTIN_BATCH10_LIBS)

$(BUILD_DIR)/builtin_final_loader_test.exe: tests/builtin_final_loader_test.cpp tests/builtin_startup_stubs.cpp core/module_loader.cpp core/module_policy.cpp core/extension_roots.cpp core/builtin_state.cpp core/builtin_modules.cpp core/builtin_startup_modules.cpp sdk/include/a2fo_module_api.h $(RUST_WIN_LIB)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $(filter %.cpp,$^) $(RUST_WIN_LINK)
$(BUILD_DIR)/builtin_feature_pack_bridge_native_test.exe: tests/builtin_feature_pack_bridge_native_test.cpp core/builtin_feature_pack_hybrid_client.cpp core/builtin_feature_pack_symbols.hpp modules/A2FOFeaturePack/hybrid_bridge_client.cpp modules/A2FOFeaturePack/hybrid_bridge_client.hpp modules/A2FOFeaturePack/hybrid_bridge_api.hpp $(RUST_WIN_LIB) $(RUST_FFI_HEADERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ tests/builtin_feature_pack_bridge_native_test.cpp core/builtin_feature_pack_hybrid_client.cpp $(RUST_WIN_LINK)
STAGE10_FIXTURES := $(BUILD_DIR)/fixtures/builtin-final
$(STAGE10_FIXTURES)/startup/%/A2FOStandalone.dll: tests/builtin_startup_provider_test.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) -DA2FO_STARTUP_PROVIDER_STANDALONE -o $@ $<
$(STAGE10_FIXTURES)/startup/full/A2FOExtensions.dll: tests/builtin_startup_provider_test.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) -DA2FO_STARTUP_PROVIDER_CORE_FULL -o $@ $<
$(STAGE10_FIXTURES)/startup/old/A2FOExtensions.dll: tests/builtin_startup_provider_test.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) -DA2FO_STARTUP_PROVIDER_CORE_OLD -o $@ $<
$(STAGE10_FIXTURES)/startup/partial/A2FOExtensions.dll: tests/builtin_startup_provider_test.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) -DA2FO_STARTUP_PROVIDER_CORE_PARTIAL -o $@ $<
$(STAGE10_FIXTURES)/startup/%/provider_test.exe: tests/builtin_startup_provider_test.cpp sdk/include/a2fo_module_provider.hpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $<
$(STAGE10_FIXTURES)/missions/%/A2FOMissions.dll: tests/builtin_missions_bridge_test.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) -DTEST_STANDALONE -o $@ $<
$(STAGE10_FIXTURES)/missions/active/A2FOExtensions.dll: tests/builtin_missions_bridge_test.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) -DTEST_CORE -DTEST_CORE_READY=1 -o $@ $<
$(STAGE10_FIXTURES)/missions/disabled/A2FOExtensions.dll: tests/builtin_missions_bridge_test.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) -DTEST_CORE -DTEST_CORE_READY=0 -o $@ $<
$(STAGE10_FIXTURES)/missions/old/A2FOExtensions.dll: tests/builtin_missions_bridge_test.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) -DTEST_OLD_CORE -o $@ $<
$(STAGE10_FIXTURES)/missions/partial/A2FOExtensions.dll: tests/builtin_missions_bridge_test.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) -DTEST_CORE -DTEST_CORE_READY=1 -DTEST_CORE_MISSING_CALLBACK -o $@ $<
$(STAGE10_FIXTURES)/missions/%/missions_test.exe: tests/builtin_missions_bridge_test.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $<
$(STAGE10_FIXTURES)/missions/%/mmm_loader.dll: $(MMM_LOADER_SHIM)
	@mkdir -p $(dir $@)
	cp $< $@
$(STAGE10_FIXTURES)/shell/A2FOExtensions.dll: tests/builtin_shell_owner_provider.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) -DA2FO_SHELL_OWNER_CORE -o $@ $<
$(STAGE10_FIXTURES)/shell/A2FOShellDisplay.dll: tests/builtin_shell_owner_provider.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) -DA2FO_SHELL_OWNER_STANDALONE -o $@ $<
$(STAGE10_FIXTURES)/shell/shell_owner_test.exe: tests/builtin_shell_owner_native_test.cpp modules/A2FORefitMenus/skin_native.cpp sdk/include/a2fo_module_provider.hpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -flto -fwhole-program -static -Wl,--gc-sections -o $@ $< -lgdi32 $(RUST_WIN_LINK)
STAGE10_PROVIDER_FIXTURES := $(foreach mode,full old partial no-core,$(STAGE10_FIXTURES)/startup/$(mode)/A2FOStandalone.dll $(STAGE10_FIXTURES)/startup/$(mode)/provider_test.exe) $(foreach mode,full old partial,$(STAGE10_FIXTURES)/startup/$(mode)/A2FOExtensions.dll)
STAGE10_MISSIONS_FIXTURES := $(foreach mode,active disabled old partial,$(STAGE10_FIXTURES)/missions/$(mode)/A2FOMissions.dll $(STAGE10_FIXTURES)/missions/$(mode)/A2FOExtensions.dll $(STAGE10_FIXTURES)/missions/$(mode)/mmm_loader.dll $(STAGE10_FIXTURES)/missions/$(mode)/missions_test.exe)
builtin-stage10-build: $(BUILD_DIR)/builtin_final_loader_test.exe $(BUILD_DIR)/builtin_feature_pack_bridge_native_test.exe $(STAGE10_PROVIDER_FIXTURES) $(STAGE10_MISSIONS_FIXTURES) $(STAGE10_FIXTURES)/shell/shell_owner_test.exe $(STAGE10_FIXTURES)/shell/A2FOExtensions.dll $(STAGE10_FIXTURES)/shell/A2FOShellDisplay.dll

# Exercise the video facade against synthetic Win32 APIs while linking the
# consolidated native modules and shared static support libraries.
$(BUILD_DIR)/builtin_video_native_test.exe: tests/video_module_test.cpp $(BUILTIN_NATIVE_OBJECTS) $(BUILTIN_NATIVE_HEADERS) $(RUST_MEDIA_DLL)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -UNDEBUG -DA2FO_BUILTIN_STARTUP=1 -DA2FO_ModuleInit=A2FOBuiltinVideo_Init -DA2FO_ModuleShutdown=A2FOBuiltinVideo_Shutdown -static -o $@ $< $(filter-out $(BUILD_DIR)/builtin-native/core/builtin_video.cpp.o $(call a2fo_rust_bridge,modules/A2FOVideo/delphi_bridge.S),$(BUILTIN_NATIVE_OBJECTS)) -ladvapi32 -lgdi32 -luser32 $(BUILTIN_BATCH10_LIBS)
builtin-stage10-build: $(BUILD_DIR)/builtin_video_native_test.exe

.PHONY: builtin-stage10-tests
builtin-stage10-tests: builtin-stage10-build builtin-native-tests $(BUILD_DIR)/engine_profiles_native_test.exe $(BUILD_DIR)/roots_dll_load_smoke.exe
	python3 tests/video_fixtures.py
	python3 tools/run_builtin_stage10_tests.py --wine-prefix "$(A2FO_BUILTIN_WINEPREFIX)"

# Direct source-link recipes do not emit .d files. Track their migration ABI
# headers explicitly, in addition to the Rust static-library dependency.
$(MODULE_POLICY_TEST) $(ENERGY_SYSTEMS_MODULE) $(BUILD_DIR)/builtin_state_test $(BUILD_DIR)/resupply_policy_test $(BUILD_DIR)/resupply_runtime_host_test $(BUILD_DIR)/resupply_runtime_test.exe $(BUILD_DIR)/builtin_startup_policy_test.exe $(BUILD_DIR)/builtin_final_loader_test.exe: $(RUST_FFI_HEADERS)
.PHONY: rust-parity-tests
rust-parity-tests: $(RUST_HOST_LIB) $(RUST_WIN_LIB)
	python3 tools/check_rust_migration_parity.py
	python3 tools/check_rust_migration_parity.py --windows --wine-prefix "$(A2FO_BUILTIN_WINEPREFIX)"

# Headless parser/byte-conversion checks; these never create a renderer window.
.PHONY: rust-stage2-checks
rust-stage2-checks: rust-policy-tests $(RUST_WIN_LIB) $(BUILD_DIR)/renderer_dds_test.exe $(BUILD_DIR)/renderer_texture_upload_test.exe
	python3 tools/check_rust_stage2_parity.py
	python3 tools/check_rust_stage2_parity.py --windows --wine-prefix "$(A2FO_BUILTIN_WINEPREFIX)"

# Direct source-link recipes need the Rust archive and private ABI headers as
# normal prerequisites so a policy or ABI edit rebuilds each affected target.
RUST_STAGE3_WIN_CONSUMERS := \
	$(BUILD_DIR)/d3d8_observer_test.exe \
	$(BUILD_DIR)/d3d11_probe_native.exe \
	$(BUILD_DIR)/d3d8_d3d11_window_test.exe \
	$(BUILD_DIR)/renderer_vs11_test.exe \
	$(BUILD_DIR)/renderer_vs11_native.exe \
	$(BUILD_DIR)/renderer_shader_tokens_test.exe \
	$(BUILD_DIR)/renderer_fvf_test.exe \
	$(BUILD_DIR)/renderer_ui_native.exe \
	$(BUILD_DIR)/renderer_bc_native.exe \
	$(BUILD_DIR)/modules/A2FOAlwaysShowShields.dll \
	$(BUILD_DIR)/modules/A2FOFeaturePack.dll \
	$(BUILD_DIR)/modules/A2FORefitYards.dll \
	$(BUILD_DIR)/modules/A2FOSquadrons.dll \
	$(BUILD_DIR)/modules/A2FOCraftIdentity.dll \
	$(BUILD_DIR)/modules/A2FOEditMenu.dll \
	$(BUILD_DIR)/modules/A2FOInstantActionSettings.dll \
	$(BUILD_DIR)/modules/A2FONebulaRenderer.dll \
	$(BUILD_DIR)/modules/A2FOPointDefenseCycles.dll \
	$(BUILD_DIR)/modules/A2FOTextureVariants.dll \
	$(BUILD_DIR)/modules/A2FOResources.dll \
	$(BUILD_DIR)/modules/A1Compat.dll \
	$(BUILD_DIR)/modules/A1Fallbacks.dll \
	$(BUILD_DIR)/game_monitor_win32_smoke.exe \
	$(BUILD_DIR)/squadrons_test.exe \
	$(BUILD_DIR)/squadrons_native_test.exe \
	$(BUILD_DIR)/pod_group_selection_test.exe \
	$(BUILD_DIR)/squadrons_resources_test.exe \
	$(BUILD_DIR)/modules/A2FORefitMenus.dll \
	$(BUILD_DIR)/refit_loading_native.exe \
	$(BUILD_DIR)/refit_loading_gpu.exe \
	$(BUILD_DIR)/refit_scene_worker_native.exe \
	$(BUILD_DIR)/renderer_game_native.exe \
	$(BUILD_DIR)/d3d8_game_activation_test.exe \
	$(BUILD_DIR)/renderer_alpha_native.exe
RUST_STAGE3_HOST_CONSUMERS := \
	$(BUILD_DIR)/fpq_paths_test \
	$(BUILD_DIR)/additional_resources_test \
	$(BUILD_DIR)/a1_race_menu_policy_test \
	$(BUILD_DIR)/a1_team_color_policy_test \
	$(BUILD_DIR)/a1_fallback_policy_test \
	$(BUILD_DIR)/odf_paths_test \
	$(BUILD_DIR)/directional_shield_layout_test \
	$(BUILD_DIR)/craft_identity_test \
	$(BUILD_DIR)/edit_menu_test \
	$(BUILD_DIR)/instant_action_settings_test \
	$(BUILD_DIR)/upgrade_pod_config_test \
	$(BUILD_DIR)/refit_policy_test \
	$(BUILD_DIR)/squadrons_test \
	$(BUILD_DIR)/shield_visibility_test \
	$(BUILD_DIR)/game_monitor_policy_test \
	$(BUILD_DIR)/art_texture_suffix_config_test \
	$(BUILD_DIR)/point_defense_cycle_test \
	$(BUILD_DIR)/texture_variants_test \
	$(BUILD_DIR)/refit_loading_theme_test \
	$(BUILD_DIR)/refit_menu_skin_test \
	$(BUILD_DIR)/refit_scene_composition_test \
	$(BUILD_DIR)/refit_scene_catalogue_test
$(RUST_STAGE3_WIN_CONSUMERS): $(RUST_WIN_LIB) $(RUST_FFI_HEADERS)
$(RUST_STAGE3_HOST_CONSUMERS): $(RUST_HOST_LIB) $(RUST_FFI_HEADERS)

.PHONY: rust-stage3-checks
rust-stage3-checks: rust-stage2-checks $(RUST_WIN_LIB)
	python3 tools/check_rust_stage3_parity.py
	python3 tools/check_rust_stage3_parity.py --windows --wine-prefix "$(A2FO_BUILTIN_WINEPREFIX)"

# Stage-four direct source-link consumers need both the archive and private
# ABI headers. CORE_OBJECTS and BUILTIN_NATIVE_OBJECTS already carry Rust.
RUST_STAGE4_WIN_CONSUMERS := \
	$(STA1_COMPAT_MODULE) \
	$(SOL_MAPS_MODULE) \
	$(BUILD_TOOLTIPS_MODULE) \
	$(FIRE_ARCS_MODULE) \
	$(TURRETS_MODULE)
RUST_STAGE4_HOST_CONSUMERS := \
	$(A1_BZN_POLICY_TEST) \
	$(SOL_FORMAT_TEST) \
	$(BUILD_TIME_TEXT_TEST) \
	$(FIRE_ARC_TEST) \
	$(TURRET_MATH_TEST) \
	$(DECAL_MATH_TEST)
$(RUST_STAGE4_WIN_CONSUMERS): $(RUST_WIN_LIB) $(RUST_FFI_HEADERS)
$(RUST_STAGE4_HOST_CONSUMERS): $(RUST_HOST_LIB) $(RUST_FFI_HEADERS)

.PHONY: rust-stage4-checks
rust-stage4-checks: rust-stage3-checks $(RUST_HOST_LIB) $(RUST_WIN_LIB)
	python3 tools/check_rust_stage4_parity.py
	python3 tools/check_rust_stage4_parity.py --windows --wine-prefix "$(A2FO_BUILTIN_WINEPREFIX)"

# Direct stage-five source links have no dependency files; refresh both the
# archive and private ABI headers when a Rust workflow or callback changes.
RUST_STAGE5_WIN_CONSUMERS := \
	$(COLLECTIVE_XP_MODULE) \
	$(BUILD_DIR)/collective_xp_runtime_test.exe \
	$(SWARM_SYSTEM_MODULE) \
	$(BUILD_DIR)/radar_diagnostics_test.exe \
	$(BUILD_DIR)/renderer_shader_cache_test.exe \
	$(M3D_MODULE) \
	$(M3D_RUNTIME_HIERARCHY_TEST) \
	$(TEAM_CHANGE_WEAPONS_SMOKE) \
	$(MODULE_DIR)/A2FOInfoIni.dll \
	$(BUILD_DIR)/craft_identity_panel_smoke.exe \
	$(EXTENSION_ROOT_SMOKE) \
	$(BUILD_DIR)/renderer_cloak_native.exe \
	$(BUILD_DIR)/refit_loading_render.exe \
	$(BUILD_DIR)/refit_loading_images.exe \
	$(BUILD_DIR)/refit_setup_render.exe \
	$(BUILD_DIR)/refit_setup_native.exe \
	$(BUILD_DIR)/refit_skin_blend_native.exe \
	$(BUILD_DIR)/refit_menu_render.exe \
	$(BUILD_DIR)/refit_menu_artwork_perf.exe \
	$(BUILD_DIR)/refit_mission_backdrop_native.exe \
	$(BUILD_DIR)/refit_menu_native.exe \
	$(BUILD_DIR)/refit_module_smoke.exe \
	$(BUILD_DIR)/builtin_refit_native_test.exe
RUST_STAGE5_HOST_CONSUMERS := \
	$(BUILD_DIR)/collective_xp_test \
	$(M3D_CONVERSION_TEST) \
	$(EXTENSION_ROOTS_TEST) \
	$(NEBULA_EMISSIVE_TEST) \
	$(SWARM_MOTION_TEST) \
	$(BUILD_DIR)/refit_loading_presentation_test \
	$(BUILD_DIR)/refit_loading_content_test \
	$(BUILD_DIR)/refit_setup_layout_test \
	$(BUILD_DIR)/refit_menu_presentation_test \
	$(BUILD_DIR)/refit_menu_model_test
$(RUST_STAGE5_WIN_CONSUMERS): $(RUST_WIN_LIB) $(RUST_FFI_HEADERS)
$(RUST_STAGE5_HOST_CONSUMERS): $(RUST_HOST_LIB) $(RUST_FFI_HEADERS)

.PHONY: rust-stage5-checks
rust-stage5-checks: rust-stage4-checks $(RUST_HOST_LIB) $(RUST_WIN_LIB)
	python3 tools/check_rust_stage5_worker_parity.py --platform both --run-wine --wineprefix "$(A2FO_BUILTIN_WINEPREFIX)"
	python3 tools/check_rust_stage5_core_parity.py
	python3 tools/check_rust_stage5_monitor_parity.py
	python3 tools/check_rust_stage5_monitor_parity.py --target win32 --run-win32 --wine-prefix "$(A2FO_BUILTIN_WINEPREFIX)"
	python3 tools/check_rust_stage5_backdrop_parity.py --run-win32 --wine-prefix "$(A2FO_BUILTIN_WINEPREFIX)"
	python3 tools/check_rust_stage5_d3dx_parity.py --run-win32 --wine-prefix "$(A2FO_BUILTIN_WINEPREFIX)"

.PHONY: rust-stage6-checks
rust-stage6-checks: rust-stage5-checks rust-parity-tests $(RUST_HOST_LIB) $(RUST_WIN_LIB)
	python3 tools/check_rust_stage6_parity.py
	python3 tools/check_rust_stage6_parity.py --windows --wine-prefix "$(A2FO_BUILTIN_WINEPREFIX)"

# Stage-seven direct source consumers share the private Rust implementation.
RUST_STAGE7_WIN_CONSUMERS := \
	$(BUILD_DIR)/modules/A2FOLogMailer.dll \
	$(BUILD_DIR)/log_mailer_native_test.exe \
	$(BUILD_DIR)/d3d8_observer_test.exe \
	$(BUILD_DIR)/d3d8_d3d11_window_test.exe \
	$(BUILD_DIR)/radar_diagnostics_test.exe \
	$(BUILD_DIR)/modules/A2FORadarDiagnostics.dll \
	$(ANIMATIONS_MODULE) \
	$(BUILD_DIR)/animations_native_test.exe \
	$(BUILD_DIR)/animations_runtime_init_smoke.exe \
	$(BUILD_DIR)/muzzle_animation_bridge_test.exe \
	$(ANIMATED_HARDPOINTS_MODULE) \
	$(BUILD_TOOLTIPS_MODULE) \
	$(MODULE_DIR)/A2FOFeaturePack.dll \
	$(MODULE_DIR)/A2FOHybridBuild.dll \
	$(REFIT_YARDS_MODULE) \
	$(MULTI_YARD_MODULE) \
	$(BUILD_DIR)/multi_yard_native_test.exe \
	$(BUILD_DIR)/multi_yard_queue_load_test.exe \
	$(COLLECTIVE_XP_MODULE) \
	$(BUILD_DIR)/collective_xp_queue_runtime_test.exe \
	$(BUILD_DIR)/collective_xp_runtime_test.exe \
	$(SHELL_DISPLAY_MODULE) \
	$(BUILD_DIR)/shell_intro_test.exe \
	$(BUILD_DIR)/shell_native_intro_test.exe \
	$(RECOVERY_TESTS_MODULE) \
	$(TEAM_CHANGE_WEAPONS_MODULE) \
	$(TEAM_CHANGE_WEAPONS_SMOKE) \
	$(STATION_ROTATION_MODULE) \
	$(STATION_ROTATION_SMOKE) \
	$(MODULE_DIR)/A2FOInfoIni.dll \
	$(CRAFT_IDENTITY_MODULE) \
	$(EDIT_MENU_MODULE) \
	$(INSTANT_ACTION_SETTINGS_MODULE) \
	$(MUZZLE_FLASHES_MODULE) \
	$(WEAPON_DAMAGE_CONTROLS_MODULE) \
	$(NORMAL_WEAPON_TECH_MODULE) \
	$(POINT_DEFENSE_CYCLES_MODULE) \
	$(ODF_VARIANTS_MODULE) \
	$(WRECKAGE_MODULE) \
	$(A1_FALLBACKS_MODULE) \
	$(BUILD_DIR)/squadrons_queue_bridge_test.exe \
	$(REFIT_MENUS_MODULE) \
	$(BUILD_DIR)/d3d8_game_activation_test.exe \
	$(BUILD_DIR)/builtin_animation_bridge_native_test.exe \
	$(BUILD_DIR)/builtin_muzzle_animation_bridge_native_test.exe \
	$(BUILD_DIR)/builtin_feature_pack_bridge_native_test.exe \
	$(STAGE10_FIXTURES)/shell/shell_owner_test.exe
$(RUST_STAGE7_WIN_CONSUMERS): $(RUST_WIN_LIB) $(RUST_FFI_HEADERS)

$(CHEATS_MODULE): $(RUST_WIN_LIB) $(RUST_FFI_HEADERS)

$(CAMERA_CONTROLS_MODULE) $(VIEW_DISTANCE_MODULE): $(RUST_WIN_LIB) $(RUST_FFI_HEADERS)

# Stage-eight runtime adapters also consume the private static Rust boundary.
$(TURRETS_MODULE) $(NEBULA_RENDERER_MODULE): $(RUST_WIN_LIB) $(RUST_FFI_HEADERS)

.PHONY: rust-stage7-checks
rust-stage7-checks: rust-tests rust-checks $(RUST_HOST_LIB) $(RUST_WIN_LIB) $(BUILD_DIR)/A2FOExtensions.dll builtin-stage10-build builtin-native-tests
	$(CARGO) clippy --offline --locked --manifest-path $(RUST_MANIFEST) --target-dir $(RUST_TARGET_DIR) --target i686-pc-windows-gnu --all-targets -- -D warnings
	python3 tools/run_rust_stage7_parity.py --wine-prefix "$(A2FO_BUILTIN_WINEPREFIX)"
	python3 tools/run_builtin_stage10_tests.py --wine-prefix "$(A2FO_BUILTIN_WINEPREFIX)" --output-dir build/rust-migration-stage7/recheck-consolidation
	python3 tools/verify_rust_expanded_census.py --dll $(BUILD_DIR)/A2FOExtensions.dll --receipt build/rust-migration-stage7/recheck-census.json

# The shared DLL is emitted by Cargo. Standalone compatibility modules continue
# to consume the Rust static archive until their platform boundaries move.
RUST_DLL_SOURCES := rust/a2fo_dll/Cargo.toml rust/a2fo_dll/Cargo.lock rust/a2fo_dll/build.rs $(wildcard rust/a2fo_dll/src/*.rs)
$(BUILD_DIR)/A2FOExtensions.dll: $(RUST_DLL_SOURCES) $(RUST_SOURCES) tools/build_rust_owned_dll.py tools/rust_dll_linker.py core/A2FOExtensions.def core/runtime_handlers.hpp rust/runtime_globals_ffi.h rust/runtime_handlers_ffi.h rust/runtime_modules_ffi.h

$(NORMAL_WEAPON_TECH_MODULE): rust/runtime_normal_state_ffi.h
$(STATION_ROTATION_MODULE): rust/runtime_station_state_ffi.h

# Headless platform contracts. Running Wine remains explicit and serialized;
# these console fixtures touch owned pages and synthetic COM objects only.
.PHONY: rust-native-platform-build
rust-native-platform-build: $(BUILD_DIR)/platform_memory_native.exe $(BUILD_DIR)/platform_d3d11_resources_native.exe $(BUILD_DIR)/platform_d3d11_allocations_native.exe
$(BUILD_DIR)/platform_memory_native.exe: tests/platform_memory_native.cpp rust/native_hooks_ffi.h $(RUST_WIN_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ tests/platform_memory_native.cpp $(RUST_WIN_LINK)
$(BUILD_DIR)/platform_d3d11_resources_native.exe: tests/platform_d3d11_resources_native.cpp renderer/d3d11/compressed_texture.cpp renderer/d3d11/com_guard.cpp renderer/d3d11/texture_handle.cpp renderer/d3d11/texture_handle.hpp renderer/textures/dds.cpp $(D3D11_UI_HEADERS) $(RUST_FFI_HEADERS) $(RUST_WIN_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ tests/platform_d3d11_resources_native.cpp renderer/d3d11/compressed_texture.cpp renderer/d3d11/com_guard.cpp renderer/d3d11/texture_handle.cpp renderer/textures/dds.cpp -luuid $(RUST_WIN_LINK)
$(BUILD_DIR)/platform_d3d11_allocations_native.exe: tests/platform_d3d11_allocations_native.cpp rust/d3d_native_ffi.h rust/d3d_resources_ffi.h $(RUST_WIN_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ tests/platform_d3d11_allocations_native.cpp -Wl,--wrap=malloc -Wl,--wrap=free $(RUST_WIN_LINK)

# Instruction-only fixtures: owned memory, synthetic callees and FP state.
# Wine is run separately under an explicitly coordinated private prefix.
.PHONY: rust-hook-bridge-build
rust-hook-bridge-build: rust-bridge-objects $(BUILD_DIR)/rust_bridge_calls_native.exe $(BUILD_DIR)/rust_bridge_camera_native.exe $(BUILD_DIR)/rust_bridge_view_native.exe $(BUILD_DIR)/rust_bridge_environment_native.exe $(BUILD_DIR)/platform_memory_native.exe
$(BUILD_DIR)/rust_bridge_calls_native.exe: tests/rust_bridge_calls_native.cpp tests/rust_bridge_call_fixture.S $(call a2fo_rust_bridge,core/delphi_bridge.S) $(call a2fo_rust_bridge,core/builtin_feature_pack_bridge.S) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ tests/rust_bridge_calls_native.cpp tests/rust_bridge_call_fixture.S $(call a2fo_rust_bridge,core/delphi_bridge.S) $(call a2fo_rust_bridge,core/builtin_feature_pack_bridge.S)
$(BUILD_DIR)/rust_bridge_camera_native.exe: tests/rust_bridge_fp_native.cpp tests/refit_native_fixture.S $(call a2fo_rust_bridge,modules/A2FOCameraControls/native_bridge.S) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -DA2FO_BRIDGE_CAMERA -static -o $@ tests/rust_bridge_fp_native.cpp tests/refit_native_fixture.S $(call a2fo_rust_bridge,modules/A2FOCameraControls/native_bridge.S)
$(BUILD_DIR)/rust_bridge_view_native.exe: tests/rust_bridge_fp_native.cpp tests/refit_native_fixture.S $(call a2fo_rust_bridge,modules/A2FOViewDistance/native_bridge.S) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -DA2FO_BRIDGE_VIEW -static -o $@ tests/rust_bridge_fp_native.cpp tests/refit_native_fixture.S $(call a2fo_rust_bridge,modules/A2FOViewDistance/native_bridge.S)
$(BUILD_DIR)/rust_bridge_environment_native.exe: tests/rust_bridge_fp_native.cpp tests/refit_native_fixture.S tests/rust_bridge_fp_env_fixture.S $(RUST_WIN_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -DA2FO_BRIDGE_FP_ENV -static -o $@ tests/rust_bridge_fp_native.cpp tests/refit_native_fixture.S tests/rust_bridge_fp_env_fixture.S $(RUST_WIN_LINK)

.PHONY: rust-patch-profile-build
rust-patch-profile-build: $(BUILD_DIR)/rust_patch_profiles_test $(BUILD_DIR)/rust_patch_profiles_test.exe $(BUILD_DIR)/rust_patch_windows_native.exe
$(BUILD_DIR)/rust_patch_profiles_test: tests/rust_patch_profiles_test.cpp rust/native_hooks_ffi.h $(RUST_HOST_LIB) | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -UNDEBUG -o $@ $< $(RUST_HOST_LINK)
$(BUILD_DIR)/rust_patch_profiles_test.exe: tests/rust_patch_profiles_test.cpp rust/native_hooks_ffi.h $(RUST_WIN_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< $(RUST_WIN_LINK)
$(BUILD_DIR)/rust_patch_windows_native.exe: tests/rust_patch_windows_native.cpp rust/native_hooks_ffi.h $(RUST_WIN_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< $(RUST_WIN_LINK)

.PHONY: native11-texture-source-tests
native11-texture-source-tests: $(BUILD_DIR)/native11_texture_store_test
	$(BUILD_DIR)/native11_texture_store_test
$(BUILD_DIR)/native11_texture_store_test: tests/native11_texture_store_test.cpp renderer/native11/texture_store.cpp renderer/native11/texture_store.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< renderer/native11/texture_store.cpp
native11-capture-tests-build: $(BUILD_DIR)/native11_texture_bridge_test.exe $(BUILD_DIR)/native11_texture_guard_test.exe
$(BUILD_DIR)/native11_texture_bridge_test.exe: tests/native11_texture_bridge_test.cpp tests/native11_texture_bridge_test.S core/native11_texture_bridge.S | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -msse2 -static -o $@ $< tests/native11_texture_bridge_test.S core/native11_texture_bridge.S
$(BUILD_DIR)/native11_texture_guard_test.exe: tests/native11_texture_guard_test.cpp core/native11_textures.cpp core/native11_textures.hpp core/native11_texture_profile.hpp core/native11_texture_bridge.S core/native11_capture.cpp core/native11_capture.hpp core/native11_capture_scale_profile.hpp core/native11_capture_bridge.S core/native11_assets.cpp core/native11_asset_profile.hpp core/native11_asset_bridge.S renderer/native11/texture_store.cpp renderer/native11/texture_store.hpp renderer/native11/model_store.cpp renderer/native11/opened_file.cpp renderer/native11/asset_snapshot.cpp renderer/native11/scene_capture.cpp renderer/native11/engine_camera.cpp renderer/native11/capture_service.cpp renderer/native11/capture_service.hpp core/native11_tga_profile.hpp core/native11_tga_bridge.S renderer/native11/open_read_owner.hpp renderer/native11/model_dependencies.cpp renderer/native11/model_dependencies.hpp renderer/native11/dependency_plan.cpp renderer/native11/dependency_plan.hpp rust/native11_model_ffi.h $(RUST_WIN_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Da2fo_rs_windows_patch_call=native11_fixture_patch_call -DA2FO_NATIVE11_TEXTURE_TESTING -Werror -static -o $@ $< core/native11_textures.cpp core/native11_texture_bridge.S core/native11_capture.cpp core/native11_capture_bridge.S core/native11_assets.cpp core/native11_asset_bridge.S renderer/native11/texture_store.cpp renderer/native11/model_store.cpp renderer/native11/opened_file.cpp renderer/native11/asset_snapshot.cpp renderer/native11/scene_capture.cpp renderer/native11/engine_camera.cpp renderer/native11/capture_service.cpp core/native11_tga_bridge.S renderer/native11/model_dependencies.cpp renderer/native11/dependency_plan.cpp $(NATIVE11_MATERIAL_SOURCES) $(RUST_WIN_LINK) -ladvapi32

.PHONY: native11-open-read-owner-test
native11-open-read-owner-test: $(BUILD_DIR)/native11_open_read_owner_test
	$(BUILD_DIR)/native11_open_read_owner_test
$(BUILD_DIR)/native11_open_read_owner_test: tests/native11_open_read_owner_test.cpp renderer/native11/open_read_owner.cpp renderer/native11/open_read_owner.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< renderer/native11/open_read_owner.cpp
native11-capture-tests-build: $(BUILD_DIR)/native11_tga_bridge_test.exe $(BUILD_DIR)/native11_tga_guard_test.exe
$(BUILD_DIR)/native11_tga_bridge_test.exe: tests/native11_tga_bridge_test.cpp tests/native11_tga_bridge_test.S core/native11_tga_bridge.S | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -msse2 -static -o $@ $< tests/native11_tga_bridge_test.S core/native11_tga_bridge.S
$(BUILD_DIR)/native11_tga_guard_test.exe: tests/native11_tga_guard_test.cpp core/native11_textures.cpp core/native11_textures.hpp core/native11_texture_profile.hpp core/native11_tga_profile.hpp core/native11_texture_bridge.S core/native11_tga_bridge.S core/native11_capture.cpp core/native11_capture.hpp core/native11_capture_scale_profile.hpp core/native11_capture_bridge.S core/native11_assets.cpp core/native11_asset_profile.hpp core/native11_asset_bridge.S renderer/native11/texture_store.cpp renderer/native11/texture_store.hpp renderer/native11/open_read_owner.cpp renderer/native11/open_read_owner.hpp renderer/native11/model_store.cpp renderer/native11/opened_file.cpp renderer/native11/asset_snapshot.cpp renderer/native11/scene_capture.cpp renderer/native11/engine_camera.cpp renderer/native11/capture_service.cpp renderer/native11/model_dependencies.cpp renderer/native11/model_dependencies.hpp renderer/native11/dependency_plan.cpp renderer/native11/dependency_plan.hpp rust/native11_model_ffi.h $(RUST_WIN_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Da2fo_rs_windows_patch_call=native11_fixture_patch_call -DA2FO_NATIVE11_TEXTURE_TESTING -Werror -static -o $@ $< core/native11_textures.cpp core/native11_texture_bridge.S core/native11_tga_bridge.S core/native11_capture.cpp core/native11_capture_bridge.S core/native11_assets.cpp core/native11_asset_bridge.S renderer/native11/texture_store.cpp renderer/native11/open_read_owner.cpp renderer/native11/model_store.cpp renderer/native11/opened_file.cpp renderer/native11/asset_snapshot.cpp renderer/native11/scene_capture.cpp renderer/native11/engine_camera.cpp renderer/native11/capture_service.cpp renderer/native11/model_dependencies.cpp renderer/native11/dependency_plan.cpp $(NATIVE11_MATERIAL_SOURCES) $(RUST_WIN_LINK) -ladvapi32

.PHONY: native11-dependency-tests
native11-dependency-tests: $(BUILD_DIR)/native11_model_dependencies_test
	$(BUILD_DIR)/native11_model_dependencies_test
$(BUILD_DIR)/native11_model_dependencies_test: tests/native11_model_dependencies_test.cpp renderer/native11/model_dependencies.cpp renderer/native11/model_dependencies.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< renderer/native11/model_dependencies.cpp

# Per-instance material observation: no engine execution in these fixtures.
NATIVE11_WORLD_BORG_SOURCES := renderer/native11/world_borg_materials.cpp renderer/native11/craft_borg_material_capture.cpp renderer/native11/craft_borg_material_policy.cpp core/native11_borg_material_source.cpp
NATIVE11_WORLD_BORG_HEADERS := core/native11_world_borg_snapshot.hpp renderer/native11/world_borg_materials.hpp renderer/native11/craft_borg_material_capture.hpp renderer/native11/craft_borg_material_policy.hpp core/native11_borg_material_source.hpp core/native11_borg_material_profile.hpp
NATIVE11_BASELINE_SOURCES := renderer/native11/model_material_baseline.cpp renderer/native11/model_material_baseline_cache.cpp
NATIVE11_BASELINE_HEADERS := renderer/native11/model_material_baseline.hpp renderer/native11/model_material_baseline_cache.hpp
NATIVE11_TEMPLATE_SOURCES := renderer/native11/native_arithmetic.cpp renderer/native11/model_node_template.cpp renderer/native11/model_node_template_cache.cpp renderer/native11/model_lod_template.cpp renderer/native11/model_lod_context.cpp renderer/native11/native_matrix34.cpp $(NATIVE11_BASELINE_SOURCES)
NATIVE11_TEMPLATE_HEADERS := renderer/native11/native_arithmetic.hpp renderer/native11/model_node_template.hpp renderer/native11/model_node_template_cache.hpp renderer/native11/model_lod_template.hpp renderer/native11/model_lod_context.hpp renderer/native11/native_matrix34.hpp core/native11_template_profile.hpp
NATIVE11_TEMPLATE_HEADERS += $(NATIVE11_BASELINE_HEADERS)
NATIVE11_PREPARATION_SOURCES := renderer/native11/craft_preparation.cpp renderer/native11/craft_preparation_capture.cpp renderer/native11/world_preparation_capture.cpp core/native11_craft_preparation_source.cpp
NATIVE11_PREPARATION_HEADERS := renderer/native11/craft_preparation.hpp renderer/native11/craft_preparation_capture.hpp renderer/native11/world_preparation_capture.hpp core/native11_craft_preparation_source.hpp core/native11_craft_preparation_profile.hpp core/native11_craft_preparation_fleet_profile.hpp core/native11_world_preparation_snapshot.hpp
NATIVE11_VISIBILITY_POLICY_SOURCES := renderer/native11/world_prepared_visibility.cpp renderer/native11/craft_visibility_capture.cpp renderer/native11/craft_visibility.cpp renderer/native11/prepared_craft_visibility_capture.cpp
NATIVE11_VISIBILITY_HEADERS := core/native11_world_visibility_snapshot.hpp core/native11_craft_visibility_base_source.hpp core/native11_craft_visibility_base_profile.hpp renderer/native11/world_prepared_visibility.hpp renderer/native11/craft_visibility_capture.hpp renderer/native11/craft_visibility_inputs.hpp renderer/native11/craft_visibility.hpp renderer/native11/prepared_craft_visibility_capture.hpp
NATIVE11_MATERIAL_SOURCES := $(NATIVE11_VISIBILITY_POLICY_SOURCES) core/native11_craft_visibility_base_source.cpp $(NATIVE11_PREPARATION_SOURCES) renderer/native11/world_view_capture.cpp renderer/native11/world_model_association.cpp core/native11_template_fleet.cpp core/native11_fofs_stream.cpp $(NATIVE11_SPRITE_CAPTURE_SOURCES) $(NATIVE11_VIEW_SOURCES) core/native11_lifecycle.cpp core/native11_lifecycle_bridge.S renderer/native11/observed_world.cpp renderer/native11/world_coverage.cpp renderer/native11/readiness_report.cpp core/native11_world_pipeline.cpp core/native11_world_lights.cpp core/native11_light_classes.cpp renderer/native11/world_light_capture.cpp renderer/native11/light_class_store.cpp renderer/native11/light_event.cpp renderer/native11/light_config.cpp renderer/native11/world_lighting.cpp renderer/native11/world_session.cpp renderer/native11/world_service.cpp renderer/native11/material_stream.cpp core/native11_nodes.cpp core/native11_node_bridge.S renderer/native11/node_state.cpp core/native11_materials.cpp core/native11_material_bridge.S renderer/native11/material_observation.cpp renderer/native11/material_snapshot.cpp core/native11_animation_clock.cpp renderer/native11/model_pose_snapshot.cpp renderer/native11/owned_animation_pose.cpp renderer/native11/runtime_pose_channels.cpp renderer/native11/animation_channel.cpp renderer/native11/instance_animation_time.cpp renderer/native11/animation_clock_capture.cpp renderer/native11/animation_phase.cpp $(NATIVE11_TEMPLATE_SOURCES) renderer/native11/class_update_barrier.cpp
NATIVE11_MATERIAL_SOURCES += $(NATIVE11_WORLD_BORG_SOURCES)
NATIVE11_MATERIAL_SOURCES += core/native11_preview_export.cpp renderer/native11/preview_export.cpp
NATIVE11_MATERIAL_HEADERS := $(NATIVE11_VISIBILITY_HEADERS) $(NATIVE11_PREPARATION_HEADERS) renderer/native11/world_view_capture.hpp core/native11_world_view_snapshot.hpp renderer/native11/world_model_association.hpp core/native11_world_model_snapshot.hpp core/native11_template_fleet.hpp core/native11_template_fleet_profile.hpp core/native11_fofs_stream.hpp core/native11_fofs_stream_profile.hpp $(NATIVE11_SPRITE_CAPTURE_HEADERS) $(NATIVE11_VIEW_HEADERS) core/native11_lifecycle.hpp core/native11_lifecycle_profile.hpp $(NATIVE11_TEMPLATE_HEADERS) renderer/native11/world_coverage.hpp renderer/native11/readiness_report.hpp core/native11_world_pipeline.hpp core/native11_world_lights.hpp core/native11_world_light_profile.hpp renderer/native11/world_session.hpp renderer/native11/world_service.hpp sdk/include/a2fo_native11_world_api.h renderer/native11/material_stream.hpp core/native11_nodes.hpp core/native11_node_profile.hpp renderer/native11/part_state.hpp renderer/native11/node_state.hpp core/native11_materials.hpp core/native11_material_profile.hpp renderer/native11/material_observation.hpp renderer/native11/material_snapshot.hpp renderer/native11/model_sources.h rust/native11_model_ffi.h core/native11_animation_clock.hpp core/native11_animation_clock_profile.hpp renderer/native11/animation_clock_capture.hpp renderer/native11/animation_phase.hpp
NATIVE11_MATERIAL_HEADERS += core/native11_light_classes.hpp renderer/native11/class_update_barrier.hpp
NATIVE11_MATERIAL_HEADERS += $(NATIVE11_WORLD_BORG_HEADERS)
NATIVE11_MATERIAL_HEADERS += modules/A2FOAnimations/native_observation.hpp
NATIVE11_MATERIAL_HEADERS += core/native11_preview_export.hpp renderer/native11/preview_export.hpp
NATIVE11_MATERIAL_CONSUMERS := $(BUILD_DIR)/native11_capture_guard_test.exe $(BUILD_DIR)/native11_asset_guard_test.exe $(BUILD_DIR)/native11_texture_guard_test.exe $(BUILD_DIR)/native11_tga_guard_test.exe
$(NATIVE11_MATERIAL_CONSUMERS): $(NATIVE11_MATERIAL_SOURCES) $(NATIVE11_MATERIAL_HEADERS)
# The runtime suite exercises these actual-core fixtures. Rebuild them together
# with the pure fixtures so a new mode cannot silently run an older executable.
native11-tests-build: $(NATIVE11_MATERIAL_CONSUMERS) $(BUILD_DIR)/native11_material_guard_test.exe $(BUILD_DIR)/native11_node_guard_test.exe
.PHONY: native11-material-tests
native11-material-tests: $(BUILD_DIR)/native11_material_observation_test $(BUILD_DIR)/native11_material_snapshot_test
	$(BUILD_DIR)/native11_material_observation_test
	$(BUILD_DIR)/native11_material_snapshot_test
$(BUILD_DIR)/native11_material_observation_test: tests/native11_material_observation_test.cpp renderer/native11/material_observation.cpp $(NATIVE11_MATERIAL_HEADERS) | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< renderer/native11/material_observation.cpp
$(BUILD_DIR)/native11_material_snapshot_test: tests/native11_material_snapshot_test.cpp renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp $(NATIVE11_MATERIAL_HEADERS) | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp

native11-capture-tests-build: $(BUILD_DIR)/native11_material_bridge_test.exe $(BUILD_DIR)/native11_material_guard_test.exe
$(BUILD_DIR)/native11_material_bridge_test.exe: tests/native11_material_bridge_test.cpp tests/native11_material_bridge_test.S core/native11_material_bridge.S | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -msse2 -static -o $@ $< tests/native11_material_bridge_test.S core/native11_material_bridge.S
$(BUILD_DIR)/native11_material_guard_test.exe: tests/native11_material_guard_test.cpp tests/native11_model_fixture.hpp $(NATIVE11_MATERIAL_SOURCES) $(NATIVE11_MATERIAL_HEADERS) core/native11_capture.cpp core/native11_capture.hpp core/native11_capture_scale_profile.hpp core/native11_capture_bridge.S core/native11_assets.cpp core/native11_assets.hpp core/native11_asset_profile.hpp core/native11_asset_bridge.S core/native11_textures.cpp core/native11_textures.hpp core/native11_texture_profile.hpp core/native11_tga_profile.hpp core/native11_texture_bridge.S core/native11_tga_bridge.S renderer/native11/scene_capture.cpp renderer/native11/scene_capture.hpp renderer/native11/engine_camera.cpp renderer/native11/asset_snapshot.cpp renderer/native11/model_store.cpp renderer/native11/opened_file.cpp renderer/native11/texture_store.cpp renderer/native11/open_read_owner.cpp renderer/native11/capture_service.cpp renderer/native11/capture_service.hpp $(RUST_WIN_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Da2fo_rs_windows_patch_call=native11_fixture_patch_call -DA2FO_NATIVE11_CAPTURE_TESTING -Werror -static -o $@ $< $(NATIVE11_MATERIAL_SOURCES) core/native11_capture.cpp core/native11_capture_bridge.S core/native11_assets.cpp core/native11_asset_bridge.S core/native11_textures.cpp core/native11_texture_bridge.S core/native11_tga_bridge.S renderer/native11/scene_capture.cpp renderer/native11/engine_camera.cpp renderer/native11/asset_snapshot.cpp renderer/native11/model_store.cpp renderer/native11/opened_file.cpp renderer/native11/texture_store.cpp renderer/native11/open_read_owner.cpp renderer/native11/capture_service.cpp renderer/native11/model_dependencies.cpp renderer/native11/dependency_plan.cpp $(RUST_WIN_LINK) -ladvapi32

$(BUILD_DIR)/native11_material_guard_test.exe: renderer/native11/model_dependencies.cpp renderer/native11/model_dependencies.hpp renderer/native11/dependency_plan.cpp renderer/native11/dependency_plan.hpp

.PHONY: native11-node-tests
native11-node-tests: $(BUILD_DIR)/native11_node_state_test
	$(BUILD_DIR)/native11_node_state_test
$(BUILD_DIR)/native11_node_state_test: tests/native11_node_state_test.cpp renderer/native11/node_state.cpp $(NATIVE11_MATERIAL_HEADERS) | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< renderer/native11/node_state.cpp
native11-tests-build: $(BUILD_DIR)/native11_part_state_native.exe
$(BUILD_DIR)/native11_part_state_native.exe: tests/native11_part_state_native.cpp tests/native11_model_fixture.hpp sdk/include/a2fo_native11_api.h $(BUILD_DIR)/native11_api_c.o | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(BUILD_DIR)/native11_api_c.o
native11-capture-tests-build: $(BUILD_DIR)/native11_node_bridge_test.exe
$(BUILD_DIR)/native11_node_bridge_test.exe: tests/native11_node_bridge_test.cpp tests/native11_node_bridge_test.S core/native11_node_bridge.S | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -msse2 -static -o $@ $< tests/native11_node_bridge_test.S core/native11_node_bridge.S

native11-capture-tests-build: $(BUILD_DIR)/native11_node_guard_test.exe
$(BUILD_DIR)/native11_node_guard_test.exe: tests/native11_node_guard_test.cpp tests/native11_model_fixture.hpp $(NATIVE11_MATERIAL_SOURCES) $(NATIVE11_MATERIAL_HEADERS) core/native11_capture.cpp core/native11_capture.hpp core/native11_capture_scale_profile.hpp core/native11_capture_bridge.S core/native11_assets.cpp core/native11_assets.hpp core/native11_asset_profile.hpp core/native11_asset_bridge.S core/native11_textures.cpp core/native11_textures.hpp core/native11_texture_profile.hpp core/native11_tga_profile.hpp core/native11_texture_bridge.S core/native11_tga_bridge.S renderer/native11/scene_capture.cpp renderer/native11/scene_capture.hpp renderer/native11/engine_camera.cpp renderer/native11/asset_snapshot.cpp renderer/native11/model_store.cpp renderer/native11/opened_file.cpp renderer/native11/texture_store.cpp renderer/native11/open_read_owner.cpp renderer/native11/capture_service.cpp renderer/native11/capture_service.hpp $(RUST_WIN_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Da2fo_rs_windows_patch_call=native11_fixture_patch_call -DA2FO_NATIVE11_CAPTURE_TESTING -Werror -static -o $@ $< $(NATIVE11_MATERIAL_SOURCES) core/native11_capture.cpp core/native11_capture_bridge.S core/native11_assets.cpp core/native11_asset_bridge.S core/native11_textures.cpp core/native11_texture_bridge.S core/native11_tga_bridge.S renderer/native11/scene_capture.cpp renderer/native11/engine_camera.cpp renderer/native11/asset_snapshot.cpp renderer/native11/model_store.cpp renderer/native11/opened_file.cpp renderer/native11/texture_store.cpp renderer/native11/open_read_owner.cpp renderer/native11/capture_service.cpp renderer/native11/model_dependencies.cpp renderer/native11/dependency_plan.cpp $(RUST_WIN_LINK) -ladvapi32
$(BUILD_DIR)/native11_node_guard_test.exe: renderer/native11/model_dependencies.cpp renderer/native11/dependency_plan.cpp

.PHONY: native11-stream-tests
native11-stream-tests: $(BUILD_DIR)/native11_material_stream_test
	$(BUILD_DIR)/native11_material_stream_test
$(BUILD_DIR)/native11_material_stream_test: tests/native11_material_stream_test.cpp renderer/native11/material_stream.cpp renderer/native11/material_stream.hpp renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp $(NATIVE11_MATERIAL_HEADERS) | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< renderer/native11/material_stream.cpp renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp
native11-tests-build: $(BUILD_DIR)/native11_material_stream_native.exe
$(BUILD_DIR)/native11_material_stream_native.exe: tests/native11_material_stream_native.cpp renderer/native11/material_stream.cpp renderer/native11/material_stream.hpp renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp renderer/native11/capture_service.cpp $(BUILD_DIR)/native11_api_c.o $(NATIVE11_MATERIAL_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/material_stream.cpp renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp renderer/native11/capture_service.cpp $(BUILD_DIR)/native11_api_c.o

NATIVE11_LIGHT_SOURCES := renderer/native11/light_config.cpp renderer/native11/light_event.cpp renderer/native11/world_lighting.cpp renderer/native11/lighting_service.cpp
NATIVE11_LIGHT_HEADERS := renderer/native11/light_config.hpp renderer/native11/light_event.hpp renderer/native11/world_lighting.hpp renderer/native11/lighting_service.hpp
.PHONY: native11-light-tests
native11-light-tests: $(BUILD_DIR)/native11_light_config_test $(BUILD_DIR)/native11_world_lighting_test $(BUILD_DIR)/native11_light_event_test
	$(BUILD_DIR)/native11_light_config_test
	$(BUILD_DIR)/native11_world_lighting_test
	$(BUILD_DIR)/native11_light_event_test
$(BUILD_DIR)/native11_light_config_test: tests/native11_light_config_test.cpp renderer/native11/light_config.cpp $(NATIVE11_LIGHT_HEADERS) | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< renderer/native11/light_config.cpp
$(BUILD_DIR)/native11_world_lighting_test: tests/native11_world_lighting_test.cpp renderer/native11/world_lighting.cpp $(NATIVE11_LIGHT_HEADERS) | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< renderer/native11/world_lighting.cpp
$(BUILD_DIR)/native11_light_event_test: tests/native11_light_event_test.cpp renderer/native11/light_event.cpp renderer/native11/light_config.cpp $(NATIVE11_LIGHT_HEADERS) | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< renderer/native11/light_event.cpp renderer/native11/light_config.cpp
native11-tests-build: $(BUILD_DIR)/native11_world_lighting_native.exe
$(BUILD_DIR)/native11_world_lighting_native.exe: tests/native11_world_lighting_native.cpp tests/native11_model_fixture.hpp $(NATIVE11_LIGHT_SOURCES) $(NATIVE11_LIGHT_HEADERS) renderer/native11/capture_service.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(NATIVE11_LIGHT_SOURCES) renderer/native11/capture_service.cpp

.PHONY: native11-class-light-tests
native11-class-light-tests: $(BUILD_DIR)/native11_light_class_store_test
	$(BUILD_DIR)/native11_light_class_store_test
$(BUILD_DIR)/native11_light_class_store_test: tests/native11_light_class_store_test.cpp renderer/native11/light_class_store.cpp renderer/native11/light_class_store.hpp renderer/native11/light_event.cpp renderer/native11/light_config.cpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< renderer/native11/light_class_store.cpp renderer/native11/light_event.cpp renderer/native11/light_config.cpp

native11-tests-build: $(BUILD_DIR)/native11_light_class_native.exe
$(BUILD_DIR)/native11_light_class_native.exe: tests/native11_light_class_native.cpp core/native11_light_classes.cpp core/native11_light_classes.hpp renderer/native11/light_class_store.cpp renderer/native11/light_class_store.hpp renderer/native11/light_event.cpp renderer/native11/light_config.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< core/native11_light_classes.cpp renderer/native11/class_update_barrier.cpp renderer/native11/light_class_store.cpp renderer/native11/light_event.cpp renderer/native11/light_config.cpp

.PHONY: native11-world-light-tests
native11-world-light-tests: $(BUILD_DIR)/native11_world_light_capture_test
	$(BUILD_DIR)/native11_world_light_capture_test
$(BUILD_DIR)/native11_world_light_capture_test: tests/native11_world_light_capture_test.cpp renderer/native11/world_light_capture.cpp renderer/native11/world_light_capture.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< renderer/native11/world_light_capture.cpp

NATIVE11_WORLD_LIGHT_SOURCES := core/native11_world_lights.cpp core/native11_light_classes.cpp renderer/native11/world_light_capture.cpp renderer/native11/light_class_store.cpp renderer/native11/light_event.cpp renderer/native11/light_config.cpp renderer/native11/world_lighting.cpp renderer/native11/class_update_barrier.cpp
NATIVE11_WORLD_LIGHT_HEADERS := core/native11_world_lights.hpp core/native11_world_light_profile.hpp core/native11_light_classes.hpp renderer/native11/world_light_capture.hpp renderer/native11/light_class_store.hpp renderer/native11/class_update_barrier.hpp
native11-capture-tests-build: $(BUILD_DIR)/native11_world_light_native.exe
native11-capture-tests-build: $(BUILD_DIR)/native11_world_light_owner_native.exe
$(BUILD_DIR)/native11_world_light_owner_native.exe: tests/native11_world_light_owner_native.cpp $(NATIVE11_WORLD_LIGHT_SOURCES) $(NATIVE11_WORLD_LIGHT_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(NATIVE11_WORLD_LIGHT_SOURCES) -ladvapi32
$(BUILD_DIR)/native11_world_light_native.exe: tests/native11_world_light_native.cpp $(NATIVE11_WORLD_LIGHT_SOURCES) $(NATIVE11_WORLD_LIGHT_HEADERS) $(NATIVE11_MATERIAL_SOURCES) $(NATIVE11_MATERIAL_HEADERS) $(RUST_WIN_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(sort $(NATIVE11_WORLD_LIGHT_SOURCES) $(NATIVE11_MATERIAL_SOURCES)) core/native11_capture.cpp core/native11_capture_bridge.S core/native11_assets.cpp core/native11_asset_bridge.S core/native11_textures.cpp core/native11_texture_bridge.S core/native11_tga_bridge.S renderer/native11/scene_capture.cpp renderer/native11/engine_camera.cpp renderer/native11/asset_snapshot.cpp renderer/native11/model_store.cpp renderer/native11/opened_file.cpp renderer/native11/texture_store.cpp renderer/native11/open_read_owner.cpp renderer/native11/capture_service.cpp renderer/native11/model_dependencies.cpp renderer/native11/dependency_plan.cpp $(RUST_WIN_LINK) -ladvapi32

native11-tests-build: $(BUILD_DIR)/native11_world_casters_native.exe
$(BUILD_DIR)/native11_world_api_c.o: tests/native11_world_api_c.c sdk/include/a2fo_native11_world_api.h | $(BUILD_DIR)
	i686-w64-mingw32-gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -c -o $@ $<
$(BUILD_DIR)/native11_world_casters_native.exe: tests/native11_world_casters_native.cpp sdk/include/a2fo_native11_world_api.h tests/native11_model_fixture.hpp $(BUILD_DIR)/native11_world_api_c.o renderer/native11/capture_service.cpp renderer/native11/lighting_service.cpp renderer/native11/world_lighting.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(BUILD_DIR)/native11_world_api_c.o renderer/native11/capture_service.cpp renderer/native11/lighting_service.cpp renderer/native11/world_lighting.cpp
$(BUILD_DIR)/A2FONative11Renderer.dll: sdk/include/a2fo_native11_world_api.h

native11-tests-build: $(BUILD_DIR)/native11_world_session_native.exe
$(BUILD_DIR)/native11_world_session_native.exe: tests/native11_world_session_native.cpp renderer/native11/world_session.cpp renderer/native11/world_session.hpp renderer/native11/world_service.cpp renderer/native11/material_snapshot.cpp renderer/native11/material_snapshot.hpp renderer/native11/asset_snapshot.cpp renderer/native11/world_lighting.cpp sdk/include/a2fo_native11_world_api.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/world_session.cpp renderer/native11/world_service.cpp renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp renderer/native11/world_lighting.cpp

native11-tests-build: $(BUILD_DIR)/native11_world_pipeline_native.exe
$(BUILD_DIR)/native11_world_pipeline_native.exe: tests/native11_world_pipeline_native.cpp renderer/native11/observed_world.cpp renderer/native11/material_stream.cpp renderer/native11/readiness_report.cpp renderer/native11/world_coverage.cpp core/native11_world_pipeline.cpp core/native11_preview_export.cpp core/native11_preview_export.hpp renderer/native11/preview_export.cpp renderer/native11/preview_export.hpp core/native11_world_pipeline.hpp core/native11_light_classes.cpp renderer/native11/light_class_store.cpp renderer/native11/light_event.cpp renderer/native11/light_config.cpp renderer/native11/world_session.cpp renderer/native11/world_service.cpp renderer/native11/world_lighting.cpp renderer/native11/material_snapshot.cpp renderer/native11/material_snapshot.hpp renderer/native11/asset_snapshot.cpp sdk/include/a2fo_native11_world_api.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DA2FO_NATIVE11_PIPELINE_TESTING -Werror -static -o $@ $< renderer/native11/observed_world.cpp renderer/native11/material_stream.cpp renderer/native11/readiness_report.cpp renderer/native11/world_coverage.cpp core/native11_world_pipeline.cpp core/native11_preview_export.cpp renderer/native11/preview_export.cpp core/native11_light_classes.cpp renderer/native11/class_update_barrier.cpp renderer/native11/light_class_store.cpp renderer/native11/light_event.cpp renderer/native11/light_config.cpp renderer/native11/world_session.cpp renderer/native11/world_service.cpp renderer/native11/world_lighting.cpp renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp

native11-tests-build: $(BUILD_DIR)/native11_live_material_content.exe $(BUILD_DIR)/native11_live_material_snapshot.exe $(BUILD_DIR)/native11_live_material_gpu.exe $(BUILD_DIR)/native11_live_material_pipeline.exe
$(BUILD_DIR)/native11_live_material_content.exe: tests/native11_live_material_content_test.cpp renderer/native11/content.cpp renderer/native11/live_material.cpp renderer/native11/live_material.hpp $(NATIVE11_GEOMETRY_SOURCES) renderer/native11/texture.cpp renderer/native11/assets.h $(NATIVE11_ASSET_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< renderer/native11/content.cpp renderer/native11/live_material.cpp $(NATIVE11_GEOMETRY_SOURCES) renderer/native11/texture.cpp $(NATIVE11_ASSET_LIB) -lntdll -luserenv -lws2_32 -lbcrypt -ladvapi32
$(BUILD_DIR)/native11_live_material_snapshot.exe: tests/native11_live_material_snapshot_test.cpp renderer/native11/material_snapshot.cpp renderer/native11/material_snapshot.hpp renderer/native11/asset_snapshot.cpp sdk/include/a2fo_native11_live_material_api.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp
$(BUILD_DIR)/native11_live_material_gpu.exe: tests/native11_live_material_gpu.cpp tests/native11_model_fixture.hpp sdk/include/a2fo_native11_live_material_api.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $<
$(BUILD_DIR)/native11_live_material_pipeline.exe: tests/native11_live_material_pipeline.cpp tests/native11_model_fixture.hpp renderer/native11/observed_world.cpp renderer/native11/material_stream.cpp renderer/native11/readiness_report.cpp renderer/native11/world_coverage.cpp core/native11_world_pipeline.cpp core/native11_preview_export.cpp core/native11_preview_export.hpp renderer/native11/preview_export.cpp renderer/native11/preview_export.hpp core/native11_world_pipeline.hpp core/native11_light_classes.cpp renderer/native11/light_class_store.cpp renderer/native11/light_event.cpp renderer/native11/light_config.cpp renderer/native11/world_session.cpp renderer/native11/world_service.cpp renderer/native11/world_lighting.cpp renderer/native11/material_snapshot.cpp renderer/native11/material_snapshot.hpp renderer/native11/asset_snapshot.cpp sdk/include/a2fo_native11_live_material_api.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< renderer/native11/observed_world.cpp renderer/native11/material_stream.cpp renderer/native11/readiness_report.cpp renderer/native11/world_coverage.cpp core/native11_world_pipeline.cpp core/native11_preview_export.cpp renderer/native11/preview_export.cpp core/native11_light_classes.cpp renderer/native11/class_update_barrier.cpp renderer/native11/light_class_store.cpp renderer/native11/light_event.cpp renderer/native11/light_config.cpp renderer/native11/world_session.cpp renderer/native11/world_service.cpp renderer/native11/world_lighting.cpp renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp

native11-tests-build: $(BUILD_DIR)/native11_preview_export_native.exe $(BUILD_DIR)/native11_live_preview_export_native.exe
$(BUILD_DIR)/native11_preview_export_native.exe: tests/native11_preview_export_test.cpp renderer/native11/preview_export.cpp renderer/native11/preview_export.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< renderer/native11/preview_export.cpp
$(BUILD_DIR)/native11_live_preview_export_native.exe: tests/native11_live_preview_export_native.cpp core/native11_preview_export.cpp core/native11_preview_export.hpp renderer/native11/preview_export.cpp renderer/native11/preview_export.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< core/native11_preview_export.cpp renderer/native11/preview_export.cpp

.PHONY: native11-coverage-tests
native11-coverage-tests: $(BUILD_DIR)/native11_world_coverage_test
	$(BUILD_DIR)/native11_world_coverage_test
$(BUILD_DIR)/native11_world_coverage_test: tests/native11_world_coverage_test.cpp renderer/native11/world_coverage.cpp renderer/native11/world_coverage.hpp renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< renderer/native11/world_coverage.cpp renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp
native11-tests-build: $(BUILD_DIR)/native11_world_coverage_native.exe
$(BUILD_DIR)/native11_world_coverage_native.exe: tests/native11_world_coverage_test.cpp renderer/native11/world_coverage.cpp renderer/native11/world_coverage.hpp renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/world_coverage.cpp renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp

.PHONY: native11-observed-tests
native11-observed-tests: $(BUILD_DIR)/native11_observed_world_test
	$(BUILD_DIR)/native11_observed_world_test
$(BUILD_DIR)/native11_observed_world_test: tests/native11_observed_world_test.cpp renderer/native11/observed_world.cpp renderer/native11/observed_world.hpp renderer/native11/world_coverage.cpp renderer/native11/material_stream.cpp renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< renderer/native11/observed_world.cpp renderer/native11/world_coverage.cpp renderer/native11/material_stream.cpp renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp
native11-tests-build: $(BUILD_DIR)/native11_observed_world_native.exe
$(BUILD_DIR)/native11_observed_world_native.exe: tests/native11_observed_world_native.cpp renderer/native11/observed_world.cpp renderer/native11/observed_world.hpp renderer/native11/world_coverage.cpp renderer/native11/material_stream.cpp renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp renderer/native11/world_session.cpp renderer/native11/world_service.cpp renderer/native11/world_lighting.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/observed_world.cpp renderer/native11/world_coverage.cpp renderer/native11/material_stream.cpp renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp renderer/native11/world_session.cpp renderer/native11/world_service.cpp renderer/native11/world_lighting.cpp

native11-tests-build: $(BUILD_DIR)/native11_mesh_sharing_native.exe
$(BUILD_DIR)/native11_mesh_sharing_native.exe: tests/native11_mesh_sharing_native.cpp tests/native11_model_fixture.hpp renderer/native11/submission.cpp renderer/native11/submission.hpp renderer/native11/content.cpp $(NATIVE11_GEOMETRY_SOURCES) $(NATIVE11_SOURCES) $(NATIVE11_HEADERS) $(NATIVE11_ASSET_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/content.cpp $(NATIVE11_GEOMETRY_SOURCES) renderer/native11/submission.cpp $(NATIVE11_SOURCES) $(NATIVE11_ASSET_LIB) -ldxguid -luuid -lntdll -luserenv -lws2_32 -lbcrypt -ladvapi32

# API6 is append-only; compile its public contract as both C11 and C++.
$(BUILD_DIR)/A2FONative11Renderer.dll: sdk/include/a2fo_native11_geometry_api.h
native11-tests-build: $(BUILD_DIR)/native11_geometry_api_native.exe
$(BUILD_DIR)/native11_geometry_api_c.o: tests/native11_geometry_api_c.c sdk/include/a2fo_native11_geometry_api.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c11 -Wall -Wextra -Werror -c -o $@ $<
$(BUILD_DIR)/native11_geometry_api_native.exe: tests/native11_geometry_api_native.cpp tests/native11_model_fixture.hpp sdk/include/a2fo_native11_geometry_api.h $(BUILD_DIR)/native11_geometry_api_c.o | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(BUILD_DIR)/native11_geometry_api_c.o
$(BUILD_DIR)/native11_world_session_native.exe $(BUILD_DIR)/native11_world_pipeline_native.exe: sdk/include/a2fo_native11_geometry_api.h

.PHONY: native11-readiness-tests
native11-readiness-tests: $(BUILD_DIR)/native11_readiness_report_test
	$(BUILD_DIR)/native11_readiness_report_test
$(BUILD_DIR)/native11_readiness_report_test: tests/native11_readiness_report_test.cpp renderer/native11/readiness_report.cpp renderer/native11/readiness_report.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< renderer/native11/readiness_report.cpp
native11-tests-build: $(BUILD_DIR)/native11_readiness_report_native.exe
$(BUILD_DIR)/native11_readiness_report_native.exe: tests/native11_readiness_report_test.cpp renderer/native11/readiness_report.cpp renderer/native11/readiness_report.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/readiness_report.cpp

NATIVE11_SURFACE_STAGE ?= 33
native11-tests-build: $(BUILD_DIR)/native11_surface_native.exe
$(BUILD_DIR)/native11_surface_native.exe: tests/native11_surface_native.cpp Makefile $(NATIVE11_SOURCES) $(NATIVE11_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DNATIVE11_SURFACE_STAGE=$(NATIVE11_SURFACE_STAGE) -Werror -static -o $@ $< $(NATIVE11_SOURCES) -ldxguid -luuid

.PHONY: native11-quad-tests
native11-quad-tests: $(BUILD_DIR)/native11_quad_geometry_test
	$(BUILD_DIR)/native11_quad_geometry_test
$(BUILD_DIR)/native11_quad_geometry_test: tests/native11_quad_geometry_test.cpp renderer/native11/quad_geometry.cpp renderer/native11/quad_geometry.hpp renderer/native11/scene.hpp | $(BUILD_DIR)
	$(CXX_HOST) -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror -o $@ $< renderer/native11/quad_geometry.cpp
native11-tests-build: $(BUILD_DIR)/native11_quad_geometry_native.exe $(BUILD_DIR)/native11_quad_native.exe
$(BUILD_DIR)/native11_quad_geometry_native.exe: tests/native11_quad_geometry_test.cpp renderer/native11/quad_geometry.cpp renderer/native11/quad_geometry.hpp renderer/native11/scene.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/quad_geometry.cpp
$(BUILD_DIR)/native11_quad_native.exe: tests/native11_quad_native.cpp renderer/native11/quad_geometry.cpp renderer/native11/quad_geometry.hpp Makefile $(NATIVE11_SOURCES) $(NATIVE11_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DNATIVE11_QUAD_STAGE=35 -Werror -static -o $@ $< renderer/native11/quad_geometry.cpp $(NATIVE11_SOURCES) -ldxguid -luuid

# API7 owns resource payloads; API1-6 tables and C records retain their ABI.
NATIVE11_PRIMITIVE_HEADERS := renderer/native11/primitive_scene.hpp renderer/native11/primitive_service.hpp renderer/native11/primitive_session.hpp sdk/include/a2fo_native11_primitives_api.h
$(BUILD_DIR)/A2FONative11Renderer.dll: $(NATIVE11_PRIMITIVE_HEADERS)
native11-tests-build: $(BUILD_DIR)/native11_primitive_scene_native.exe $(BUILD_DIR)/native11_primitives_api_native.exe $(BUILD_DIR)/native11_primitive_encoder_native.exe $(BUILD_DIR)/native11_primitive_session_native.exe
$(BUILD_DIR)/native11_primitive_scene_native.exe: tests/native11_primitive_scene_native.cpp renderer/native11/primitive_scene.cpp $(NATIVE11_PRIMITIVE_HEADERS) $(NATIVE11_SOURCES) $(NATIVE11_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/primitive_scene.cpp $(NATIVE11_SOURCES) -ldxguid -luuid -ladvapi32
$(BUILD_DIR)/native11_primitives_api_c.o: tests/native11_primitives_api_c.c sdk/include/a2fo_native11_primitives_api.h renderer/native11/assets.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c11 -Wall -Wextra -Werror -c -o $@ $<
$(BUILD_DIR)/native11_primitives_api_native.exe: tests/native11_primitives_api_native.cpp $(BUILD_DIR)/native11_primitives_api_c.o $(NATIVE11_PRIMITIVE_HEADERS) renderer/native11/assets.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(BUILD_DIR)/native11_primitives_api_c.o
$(BUILD_DIR)/native11_primitive_encoder_native.exe: tests/native11_primitive_encoder_native.cpp renderer/native11/primitive_service.cpp $(NATIVE11_PRIMITIVE_HEADERS) $(NATIVE11_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/primitive_service.cpp
$(BUILD_DIR)/native11_primitive_session_native.exe: tests/native11_primitive_session_native.cpp renderer/native11/primitive_session.cpp renderer/native11/primitive_service.cpp $(NATIVE11_PRIMITIVE_HEADERS) $(NATIVE11_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/primitive_session.cpp renderer/native11/primitive_service.cpp

# Rebuild live fixtures for shared packet-header changes; frozen clients stay immutable.
NATIVE11_LIVE_HEADER_CLIENTS := $(BUILD_DIR)/native11_asset_bridge_test.exe $(BUILD_DIR)/native11_asset_guard_test.exe $(BUILD_DIR)/native11_assets_native.exe $(BUILD_DIR)/native11_bound_service_native.exe $(BUILD_DIR)/native11_capture_bridge_test.exe $(BUILD_DIR)/native11_capture_guard_test.exe $(BUILD_DIR)/native11_content_native.exe $(BUILD_DIR)/native11_dependency_native.exe $(BUILD_DIR)/native11_geometry_api_native.exe $(BUILD_DIR)/native11_light_class_native.exe $(BUILD_DIR)/native11_material_bridge_test.exe $(BUILD_DIR)/native11_material_guard_test.exe $(BUILD_DIR)/native11_material_stream_native.exe $(BUILD_DIR)/native11_mesh_sharing_native.exe $(BUILD_DIR)/native11_node_bridge_test.exe $(BUILD_DIR)/native11_node_guard_test.exe $(BUILD_DIR)/native11_observed_world_native.exe $(BUILD_DIR)/native11_offscreen_native.exe $(BUILD_DIR)/native11_part_state_native.exe $(BUILD_DIR)/native11_preview.exe $(BUILD_DIR)/native11_primitive_encoder_native.exe $(BUILD_DIR)/native11_primitive_scene_native.exe $(BUILD_DIR)/native11_primitive_session_native.exe $(BUILD_DIR)/native11_primitives_api_native.exe $(BUILD_DIR)/native11_quad_geometry_native.exe $(BUILD_DIR)/native11_quad_native.exe $(BUILD_DIR)/native11_readiness_report_native.exe $(BUILD_DIR)/native11_renderer_native.exe $(BUILD_DIR)/native11_scene_preview.exe $(BUILD_DIR)/native11_service_native.exe $(BUILD_DIR)/native11_shadows_native.exe $(BUILD_DIR)/native11_submission_native.exe $(BUILD_DIR)/native11_surface_native.exe $(BUILD_DIR)/native11_texture_bindings_native.exe $(BUILD_DIR)/native11_texture_bridge_test.exe $(BUILD_DIR)/native11_texture_guard_test.exe $(BUILD_DIR)/native11_texture_native.exe $(BUILD_DIR)/native11_tga_bridge_test.exe $(BUILD_DIR)/native11_tga_guard_test.exe $(BUILD_DIR)/native11_world_casters_native.exe $(BUILD_DIR)/native11_world_coverage_native.exe $(BUILD_DIR)/native11_world_light_native.exe $(BUILD_DIR)/native11_world_lighting_native.exe $(BUILD_DIR)/native11_world_pipeline_native.exe $(BUILD_DIR)/native11_world_session_native.exe
$(NATIVE11_LIVE_HEADER_CLIENTS): $(wildcard renderer/native11/*.hpp renderer/native11/*.h sdk/include/a2fo_native11*.h)

# Deterministic synthetic world + retained HUD acceptance image (no game launch).
native11-tests-build: $(BUILD_DIR)/native11_showcase_native.exe
$(BUILD_DIR)/native11_showcase_native.exe: tests/native11_showcase_native.cpp renderer/native11/primitive_session.cpp renderer/native11/primitive_service.cpp renderer/native11/quad_geometry.cpp renderer/native11/quad_geometry.hpp $(NATIVE11_PRIMITIVE_HEADERS) $(NATIVE11_HEADERS) Makefile | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/primitive_session.cpp renderer/native11/primitive_service.cpp renderer/native11/quad_geometry.cpp

# Native presentation uses its own D3D11 device/swapchain, never a legacy surface.
NATIVE11_PRESENTATION_HEADERS := renderer/native11/frame_bundle.hpp renderer/native11/presentation_session.hpp renderer/native11/native_startup.hpp sdk/include/a2fo_native11_presentation_api.h
NATIVE11_PRESENTATION_CLIENT := renderer/native11/frame_bundle.cpp renderer/native11/primitive_service.cpp renderer/native11/presentation_session.cpp
$(BUILD_DIR)/A2FONative11Renderer.dll: sdk/include/a2fo_native11_presentation_api.h
native11-tests-build: $(BUILD_DIR)/native11_frame_bundle_native.exe $(BUILD_DIR)/native11_presentation_session_native.exe $(BUILD_DIR)/native11_presentation_native.exe $(BUILD_DIR)/native11_presentation_api_native.exe $(BUILD_DIR)/native11_native_startup_native.exe
$(BUILD_DIR)/native11_frame_bundle_native.exe: tests/native11_frame_bundle_test.cpp renderer/native11/frame_bundle.cpp $(NATIVE11_PRESENTATION_HEADERS) $(NATIVE11_PRIMITIVE_HEADERS) Makefile | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< renderer/native11/frame_bundle.cpp
$(BUILD_DIR)/native11_presentation_session_native.exe: tests/native11_presentation_session_native.cpp $(NATIVE11_PRESENTATION_CLIENT) $(NATIVE11_PRESENTATION_HEADERS) $(NATIVE11_PRIMITIVE_HEADERS) Makefile | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< $(NATIVE11_PRESENTATION_CLIENT)
$(BUILD_DIR)/native11_presentation_native.exe: tests/native11_presentation_native.cpp tests/native11_device_slots.cpp $(NATIVE11_SOURCES) $(NATIVE11_HEADERS) Makefile | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DA2FO_NATIVE11_FIXTURE -Werror -static -o $@ $< tests/native11_device_slots.cpp $(filter-out renderer/native11/renderer.cpp,$(NATIVE11_SOURCES)) -ldxguid -luuid
$(BUILD_DIR)/native11_presentation_api_c.o: tests/native11_presentation_api_c.c sdk/include/a2fo_native11_presentation_api.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror -c -o $@ $<
$(BUILD_DIR)/native11_presentation_api_native.exe: tests/native11_presentation_api_native.cpp $(BUILD_DIR)/native11_presentation_api_c.o $(NATIVE11_PRESENTATION_HEADERS) Makefile | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(BUILD_DIR)/native11_presentation_api_c.o
$(BUILD_DIR)/native11_native_startup_native.exe: tests/native11_native_startup_native.cpp renderer/native11/native_startup.cpp $(NATIVE11_PRESENTATION_CLIENT) $(NATIVE11_PRESENTATION_HEADERS) $(NATIVE11_PRIMITIVE_HEADERS) Makefile | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/native_startup.cpp $(NATIVE11_PRESENTATION_CLIENT)

# Owned sprite decoding and combined world/overlay presentation frontends.
NATIVE11_SPRITE_SOURCES := renderer/native11/sprite_frame_stream.cpp renderer/native11/texture_bgra.cpp renderer/native11/texture.cpp renderer/native11/quad_geometry.cpp
NATIVE11_SPRITE_HEADERS := renderer/native11/sprite_frame_stream.hpp renderer/native11/texture_bgra.hpp renderer/native11/assets.h renderer/native11/texture.hpp renderer/native11/texture_store.hpp renderer/native11/quad_geometry.hpp renderer/native11/primitive_scene.hpp renderer/native11/renderer.hpp renderer/native11/scene.hpp
NATIVE11_GAMEPLAY_CLIENT := renderer/native11/gameplay_session.cpp renderer/native11/world_service.cpp renderer/native11/world_lighting.cpp renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp renderer/native11/primitive_service.cpp
NATIVE11_GAMEPLAY_HEADERS := renderer/native11/gameplay_session.hpp renderer/native11/world_service.hpp renderer/native11/world_session.hpp renderer/native11/world_lighting.hpp renderer/native11/material_snapshot.hpp renderer/native11/asset_snapshot.hpp $(NATIVE11_PRESENTATION_HEADERS) $(NATIVE11_PRIMITIVE_HEADERS)
NATIVE11_GAMEPLAY_HEADERS += sdk/include/a2fo_native11_model_scope_api.h
native11-tests-build: $(BUILD_DIR)/native11_gameplay_session_native.exe $(BUILD_DIR)/native11_gameplay_gpu.exe
$(BUILD_DIR)/native11_gameplay_session_native.exe: tests/native11_gameplay_session_native.cpp $(NATIVE11_GAMEPLAY_CLIENT) $(NATIVE11_GAMEPLAY_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< $(NATIVE11_GAMEPLAY_CLIENT)
$(BUILD_DIR)/native11_gameplay_gpu.exe: tests/native11_gameplay_gpu.cpp tests/native11_model_fixture.hpp $(NATIVE11_GAMEPLAY_CLIENT) $(NATIVE11_GAMEPLAY_HEADERS) $(NATIVE11_SPRITE_SOURCES) $(NATIVE11_SPRITE_HEADERS) $(NATIVE11_ASSET_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< $(NATIVE11_GAMEPLAY_CLIENT) $(NATIVE11_SPRITE_SOURCES) $(NATIVE11_ASSET_LIB) -lntdll -luserenv -lws2_32 -lbcrypt -ladvapi32
native11-tests-build: $(BUILD_DIR)/native11_texture_bgra_native.exe $(BUILD_DIR)/native11_sprite_frame_stream_native.exe $(BUILD_DIR)/native11_sprite_stream_gpu.exe
$(BUILD_DIR)/native11_texture_bgra_native.exe: tests/native11_texture_bgra_test.cpp renderer/native11/texture_bgra.cpp renderer/native11/texture.cpp $(NATIVE11_SPRITE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< renderer/native11/texture_bgra.cpp renderer/native11/texture.cpp
$(BUILD_DIR)/native11_sprite_frame_stream_native.exe: tests/native11_sprite_frame_stream_native.cpp $(NATIVE11_SPRITE_SOURCES) $(NATIVE11_SPRITE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< $(NATIVE11_SPRITE_SOURCES)
$(BUILD_DIR)/native11_sprite_stream_gpu.exe: tests/native11_sprite_stream_gpu.cpp $(NATIVE11_SPRITE_SOURCES) $(NATIVE11_SPRITE_HEADERS) renderer/native11/primitive_session.cpp renderer/native11/primitive_service.cpp $(NATIVE11_ASSET_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< $(NATIVE11_SPRITE_SOURCES) renderer/native11/primitive_session.cpp renderer/native11/primitive_service.cpp $(NATIVE11_ASSET_LIB) -lntdll -luserenv -lws2_32 -lbcrypt -ladvapi32

# Recovered CPU animation timing and default-off observation diagnostics.
native11-tests-build: $(BUILD_DIR)/native11_craft_visibility.exe $(BUILD_DIR)/native11_craft_visibility_native.exe
$(BUILD_DIR)/native11_craft_visibility.exe: tests/native11_craft_visibility_test.cpp renderer/native11/craft_visibility.cpp renderer/native11/craft_visibility.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< renderer/native11/craft_visibility.cpp
$(BUILD_DIR)/native11_craft_visibility_native.exe: tests/native11_craft_visibility_native.cpp renderer/native11/craft_visibility.cpp renderer/native11/craft_visibility.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -Wl,--image-base,0x10000000 -o $@ $< renderer/native11/craft_visibility.cpp -ladvapi32
NATIVE11_POSE_SOURCES := renderer/native11/model_pose_snapshot.cpp renderer/native11/owned_animation_pose.cpp renderer/native11/animation_channel.cpp renderer/native11/animation_phase.cpp
NATIVE11_POSE_HEADERS := renderer/native11/model_pose.h renderer/native11/model_pose_snapshot.hpp renderer/native11/owned_animation_pose.hpp renderer/native11/animation_channel.hpp renderer/native11/animation_phase.hpp renderer/native11/instance_animation_time.hpp
native11-tests-build: $(BUILD_DIR)/native11_runtime_pose_channels.exe
$(BUILD_DIR)/native11_runtime_pose_channels.exe: tests/native11_runtime_pose_channels_test.cpp renderer/native11/runtime_pose_channels.cpp renderer/native11/runtime_pose_channels.hpp renderer/native11/instance_animation_time.cpp $(NATIVE11_POSE_SOURCES) $(NATIVE11_POSE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< renderer/native11/runtime_pose_channels.cpp renderer/native11/instance_animation_time.cpp $(NATIVE11_POSE_SOURCES)
native11-tests-build: $(BUILD_DIR)/native11_model_pose_snapshot.exe $(BUILD_DIR)/native11_owned_animation_pose.exe $(BUILD_DIR)/native11_owned_pose_native.exe
$(BUILD_DIR)/native11_model_pose_snapshot.exe: tests/native11_model_pose_snapshot_test.cpp renderer/native11/model_pose_snapshot.cpp $(NATIVE11_POSE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< renderer/native11/model_pose_snapshot.cpp
$(BUILD_DIR)/native11_owned_animation_pose.exe: tests/native11_owned_animation_pose_test.cpp $(NATIVE11_POSE_SOURCES) $(NATIVE11_POSE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< $(filter-out renderer/native11/model_pose_snapshot.cpp,$(NATIVE11_POSE_SOURCES))
$(BUILD_DIR)/native11_model_pose_c.o: tests/native11_model_pose_c.c renderer/native11/model_pose.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror -c -o $@ $<
$(BUILD_DIR)/native11_owned_pose_native.exe: tests/native11_owned_pose_native.cpp renderer/native11/owned_part_states.cpp renderer/native11/owned_part_states.hpp tests/native11_model_fixture.hpp $(NATIVE11_POSE_SOURCES) $(NATIVE11_POSE_HEADERS) $(BUILD_DIR)/native11_model_pose_c.o $(RUST_WIN_LIB) renderer/native11/world_session.cpp renderer/native11/world_service.cpp renderer/native11/world_lighting.cpp renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< $(NATIVE11_POSE_SOURCES) renderer/native11/owned_part_states.cpp renderer/native11/material_observation.cpp $(BUILD_DIR)/native11_model_pose_c.o renderer/native11/world_session.cpp renderer/native11/world_service.cpp renderer/native11/world_lighting.cpp renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp $(RUST_WIN_LINK) -ladvapi32
native11-tests-build: $(BUILD_DIR)/native11_animation_channel_checks.exe $(BUILD_DIR)/native11_animation_channel_native.exe $(BUILD_DIR)/native11_instance_animation_time.exe
$(BUILD_DIR)/native11_instance_animation_time.exe: tests/native11_instance_animation_time_test.cpp renderer/native11/instance_animation_time.cpp renderer/native11/instance_animation_time.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< renderer/native11/instance_animation_time.cpp
$(BUILD_DIR)/native11_animation_channel_checks.exe: tests/native11_animation_channel_test.cpp renderer/native11/animation_channel.cpp renderer/native11/animation_channel.hpp renderer/native11/animation_phase.cpp renderer/native11/animation_phase.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< renderer/native11/animation_channel.cpp renderer/native11/animation_phase.cpp
$(BUILD_DIR)/native11_animation_channel_native.exe: tests/native11_animation_channel_native.cpp renderer/native11/animation_channel.cpp renderer/native11/animation_channel.hpp renderer/native11/animation_phase.cpp renderer/native11/animation_phase.hpp renderer/native11/instance_animation_time.cpp renderer/native11/instance_animation_time.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -Wl,--image-base,0x10000000 -o $@ $< renderer/native11/animation_channel.cpp renderer/native11/animation_phase.cpp renderer/native11/instance_animation_time.cpp -ladvapi32
NATIVE11_CLOCK_SOURCES := $(NATIVE11_VIEW_SOURCES) $(NATIVE11_TEMPLATE_SOURCES) renderer/native11/animation_clock_capture.cpp renderer/native11/animation_phase.cpp renderer/native11/instance_animation_time.cpp renderer/native11/model_pose_snapshot.cpp renderer/native11/owned_animation_pose.cpp renderer/native11/runtime_pose_channels.cpp renderer/native11/animation_channel.cpp
NATIVE11_CLOCK_HEADERS := $(NATIVE11_VIEW_HEADERS) $(NATIVE11_TEMPLATE_HEADERS) $(NATIVE11_POSE_HEADERS) renderer/native11/runtime_pose_channels.hpp renderer/native11/instance_animation_time.hpp renderer/native11/animation_clock_capture.hpp renderer/native11/animation_phase.hpp core/native11_animation_clock.hpp core/native11_animation_clock_profile.hpp modules/A2FOAnimations/native_observation.hpp
NATIVE11_CLOCK_HEADERS += core/native11_material_profile.hpp core/native11_textures.hpp
native11-tests-build: $(BUILD_DIR)/native11_animation_phase_checks.exe $(BUILD_DIR)/native11_animation_phase_native.exe $(BUILD_DIR)/native11_animation_clock_capture.exe $(BUILD_DIR)/native11_animation_clock_guard.exe
$(BUILD_DIR)/native11_animation_phase_checks.exe: tests/native11_animation_phase_test.cpp renderer/native11/animation_phase.cpp $(NATIVE11_CLOCK_HEADERS) Makefile | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/animation_phase.cpp
$(BUILD_DIR)/native11_animation_phase_native.exe: tests/native11_animation_phase_native.cpp renderer/native11/animation_phase.cpp $(NATIVE11_CLOCK_HEADERS) Makefile | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -Wl,--image-base,0x10000000 -o $@ $< renderer/native11/animation_phase.cpp -ladvapi32
$(BUILD_DIR)/native11_animation_clock_capture.exe: tests/native11_animation_clock_capture_test.cpp $(NATIVE11_CLOCK_SOURCES) $(NATIVE11_CLOCK_HEADERS) Makefile | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(NATIVE11_CLOCK_SOURCES)
$(BUILD_DIR)/native11_animation_clock_guard.exe: tests/native11_animation_clock_guard.cpp core/native11_animation_clock.cpp $(NATIVE11_CLOCK_SOURCES) $(NATIVE11_CLOCK_HEADERS) $(RUST_WIN_LIB) Makefile core/native11_template_fleet.cpp core/native11_template_fleet.hpp core/native11_template_fleet_profile.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< core/native11_animation_clock.cpp core/native11_template_fleet.cpp $(NATIVE11_CLOCK_SOURCES) $(RUST_WIN_LINK) -ladvapi32

# API9 retains all authored mesh groups; old service entries keep preview scope.
native11-tests-build: $(BUILD_DIR)/native11_model_scope_clients.exe $(BUILD_DIR)/native11_model_scope_native.exe $(BUILD_DIR)/native11_owned_part_states.exe
$(BUILD_DIR)/native11_model_scope_clients.exe: tests/native11_model_scope_clients.cpp $(NATIVE11_GAMEPLAY_CLIENT) renderer/native11/world_session.cpp $(NATIVE11_GAMEPLAY_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< $(NATIVE11_GAMEPLAY_CLIENT) renderer/native11/world_session.cpp
$(BUILD_DIR)/native11_model_scope_c.o: tests/native11_model_scope_c.c sdk/include/a2fo_native11_model_scope_api.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Werror -c -o $@ $<
$(BUILD_DIR)/native11_model_scope_native.exe: tests/native11_model_scope_native.cpp $(BUILD_DIR)/native11_model_scope_c.o sdk/include/a2fo_native11_model_scope_api.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< $(BUILD_DIR)/native11_model_scope_c.o
$(BUILD_DIR)/native11_owned_part_states.exe: tests/native11_owned_part_states_test.cpp renderer/native11/owned_part_states.cpp renderer/native11/owned_part_states.hpp renderer/native11/owned_animation_pose.hpp renderer/native11/material_observation.hpp renderer/native11/model_pose_snapshot.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< renderer/native11/owned_part_states.cpp

# Load-time template capture never invokes the native loader or node methods.
native11-tests-build: $(BUILD_DIR)/native11_model_node_template.exe $(BUILD_DIR)/native11_model_node_template_cache.exe
$(BUILD_DIR)/native11_model_node_template.exe: tests/native11_model_node_template_test.cpp renderer/native11/model_node_template.cpp $(NATIVE11_TEMPLATE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< renderer/native11/model_node_template.cpp
$(BUILD_DIR)/native11_model_node_template_cache.exe: tests/native11_model_node_template_cache_test.cpp renderer/native11/model_node_template_cache.cpp $(NATIVE11_TEMPLATE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< renderer/native11/model_node_template_cache.cpp

native11-tests-build: $(BUILD_DIR)/native11_template_seeded_pose.exe
$(BUILD_DIR)/native11_template_seeded_pose.exe: tests/native11_template_seeded_pose_test.cpp $(NATIVE11_POSE_SOURCES) $(NATIVE11_POSE_HEADERS) $(NATIVE11_TEMPLATE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< $(NATIVE11_POSE_SOURCES)

# Fresh class inputs are a CPU foundation, not a registered world producer.
native11-tests-build: $(BUILD_DIR)/native11_craft_visibility_capture.exe
$(BUILD_DIR)/native11_craft_visibility_capture.exe: tests/native11_craft_visibility_capture_test.cpp renderer/native11/craft_visibility_capture.cpp renderer/native11/craft_visibility_capture.hpp renderer/native11/craft_visibility.cpp renderer/native11/craft_visibility.hpp $(NATIVE11_TEMPLATE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< renderer/native11/craft_visibility_capture.cpp renderer/native11/craft_visibility.cpp

native11-tests-build: $(BUILD_DIR)/native11_native_arithmetic.exe $(BUILD_DIR)/native11_material_arithmetic.exe
$(BUILD_DIR)/native11_native_arithmetic.exe: tests/native11_native_arithmetic_test.cpp renderer/native11/native_arithmetic.cpp renderer/native11/native_arithmetic.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -msse2 -static -o $@ $< renderer/native11/native_arithmetic.cpp
$(BUILD_DIR)/native11_material_arithmetic.exe: tests/native11_material_arithmetic_test.cpp renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp renderer/native11/material_snapshot.hpp renderer/native11/material_observation.hpp renderer/native11/native_arithmetic.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp

native11-tests-build: $(BUILD_DIR)/native11_model_lod_template.exe $(BUILD_DIR)/native11_native_matrix34.exe $(BUILD_DIR)/native11_model_lod_selection.exe $(BUILD_DIR)/native11_model_lod_native.exe
$(BUILD_DIR)/native11_model_lod_template.exe: tests/native11_model_lod_template_test.cpp renderer/native11/model_lod_template.cpp $(NATIVE11_TEMPLATE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< renderer/native11/model_lod_template.cpp
$(BUILD_DIR)/native11_native_matrix34.exe: tests/native11_native_matrix34_test.cpp renderer/native11/native_matrix34.cpp renderer/native11/native_matrix34.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -msse2 -static -o $@ $< renderer/native11/native_matrix34.cpp
$(BUILD_DIR)/native11_model_lod_selection.exe: tests/native11_model_lod_selection_test.cpp renderer/native11/model_lod_selection.cpp renderer/native11/model_lod_selection.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< renderer/native11/model_lod_selection.cpp
$(BUILD_DIR)/native11_model_lod_native.exe: tests/native11_model_lod_native.cpp renderer/native11/model_lod_selection.cpp renderer/native11/model_lod_selection.hpp renderer/native11/native_matrix34.cpp renderer/native11/native_matrix34.hpp core/native11_template_profile.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< renderer/native11/model_lod_selection.cpp renderer/native11/native_matrix34.cpp -ladvapi32

native11-tests-build: $(BUILD_DIR)/native11_model_lod_context.exe $(BUILD_DIR)/native11_model_lod_visibility.exe
$(BUILD_DIR)/native11_model_lod_context.exe: tests/native11_model_lod_context_test.cpp renderer/native11/model_lod_context.cpp renderer/native11/native_arithmetic.cpp $(NATIVE11_TEMPLATE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< renderer/native11/model_lod_context.cpp renderer/native11/native_arithmetic.cpp
$(BUILD_DIR)/native11_model_lod_visibility.exe: tests/native11_model_lod_visibility_test.cpp renderer/native11/model_lod_visibility.cpp renderer/native11/model_lod_visibility.hpp renderer/native11/model_lod_selection.cpp renderer/native11/model_lod_selection.hpp $(NATIVE11_TEMPLATE_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< renderer/native11/model_lod_visibility.cpp renderer/native11/model_lod_selection.cpp renderer/native11/native_matrix34.cpp renderer/native11/native_arithmetic.cpp

native11-tests-build: $(BUILD_DIR)/native11_lod_gpu_native.exe
$(BUILD_DIR)/native11_lod_gpu_native.exe: tests/native11_lod_gpu_native.cpp renderer/native11/model_lod_visibility.cpp renderer/native11/model_lod_visibility.hpp renderer/native11/model_lod_selection.cpp renderer/native11/model_lod_selection.hpp $(NATIVE11_POSE_SOURCES) $(NATIVE11_POSE_HEADERS) $(NATIVE11_TEMPLATE_HEADERS) renderer/native11/owned_part_states.cpp renderer/native11/material_observation.cpp renderer/native11/world_session.cpp renderer/native11/world_service.cpp renderer/native11/world_lighting.cpp renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp $(RUST_WIN_LIB) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< $(NATIVE11_POSE_SOURCES) renderer/native11/model_lod_visibility.cpp renderer/native11/model_lod_selection.cpp renderer/native11/native_matrix34.cpp renderer/native11/native_arithmetic.cpp renderer/native11/owned_part_states.cpp renderer/native11/material_observation.cpp renderer/native11/world_session.cpp renderer/native11/world_service.cpp renderer/native11/world_lighting.cpp renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp $(RUST_WIN_LINK) -ladvapi32

native11-tests-build: $(BUILD_DIR)/native11_model_geometry_policy.exe $(BUILD_DIR)/native11_geometry_policy_adapter.exe $(BUILD_DIR)/native11_model_geometry_native.exe $(BUILD_DIR)/native11_geometry_policy_native.exe
native11-tests-build: $(BUILD_DIR)/native11_geometry_world_native.exe $(BUILD_DIR)/native11_model_store.exe
$(BUILD_DIR)/native11_model_store.exe: tests/native11_model_store_test.cpp renderer/native11/model_store.cpp renderer/native11/model_store.hpp renderer/native11/native_arithmetic.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< renderer/native11/model_store.cpp
$(BUILD_DIR)/native11_geometry_world_native.exe: tests/native11_geometry_world_native.cpp tests/native11_model_fixture.hpp renderer/native11/model_store.cpp renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp renderer/native11/world_session.cpp renderer/native11/world_service.cpp renderer/native11/world_lighting.cpp sdk/include/a2fo_native11_geometry_policy_api.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< renderer/native11/model_store.cpp renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp renderer/native11/world_session.cpp renderer/native11/world_service.cpp renderer/native11/world_lighting.cpp
$(BUILD_DIR)/native11_model_geometry_policy.exe: tests/native11_model_geometry_policy_test.cpp $(NATIVE11_GEOMETRY_SOURCES) renderer/native11/model_geometry_policy.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< $(NATIVE11_GEOMETRY_SOURCES)
$(BUILD_DIR)/native11_geometry_policy_adapter.exe: tests/native11_geometry_policy_adapter_test.cpp renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp renderer/native11/material_snapshot.hpp renderer/native11/model_geometry_policy.hpp sdk/include/a2fo_native11_geometry_policy_api.h | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< renderer/native11/material_snapshot.cpp renderer/native11/asset_snapshot.cpp
$(BUILD_DIR)/native11_model_geometry_native.exe: tests/native11_model_geometry_native.cpp $(NATIVE11_GEOMETRY_SOURCES) renderer/native11/model_geometry_policy.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< $(NATIVE11_GEOMETRY_SOURCES) -ladvapi32
$(BUILD_DIR)/native11_geometry_policy_c.o: tests/native11_geometry_policy_c.c sdk/include/a2fo_native11_geometry_policy_api.h | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -std=c11 -Wall -Wextra -Wpedantic -Werror -c -o $@ $<
$(BUILD_DIR)/native11_geometry_policy_native.exe: tests/native11_geometry_policy_native.cpp $(BUILD_DIR)/native11_geometry_policy_c.o sdk/include/a2fo_native11_geometry_policy_api.h tests/native11_model_fixture.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< $(BUILD_DIR)/native11_geometry_policy_c.o


native11-tests-build: $(BUILD_DIR)/native11_lifecycle_bridge_test.exe $(BUILD_DIR)/native11_lifecycle_guard.exe
native11-tests-build: $(BUILD_DIR)/native11_fofs_stream_guard.exe
native11-tests-build: $(BUILD_DIR)/native11_template_fleet_guard.exe
$(BUILD_DIR)/native11_template_fleet_guard.exe: tests/native11_template_fleet_guard.cpp core/native11_template_fleet.cpp core/native11_template_fleet.hpp core/native11_template_fleet_profile.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< core/native11_template_fleet.cpp -ladvapi32
$(BUILD_DIR)/native11_fofs_stream_guard.exe: tests/native11_fofs_stream_guard.cpp core/native11_fofs_stream.cpp core/native11_fofs_stream.hpp core/native11_fofs_stream_profile.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< core/native11_fofs_stream.cpp -ladvapi32
$(BUILD_DIR)/native11_lifecycle_bridge_test.exe: tests/native11_lifecycle_bridge_test.cpp tests/native11_lifecycle_bridge_test.S core/native11_lifecycle_bridge.S core/native11_lifecycle.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -msse2 -static -o $@ $< tests/native11_lifecycle_bridge_test.S core/native11_lifecycle_bridge.S
$(BUILD_DIR)/native11_lifecycle_guard.exe: tests/native11_lifecycle_guard.cpp core/native11_lifecycle.cpp core/native11_lifecycle_bridge.S core/native11_lifecycle.hpp core/native11_lifecycle_profile.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< core/native11_lifecycle.cpp core/native11_lifecycle_bridge.S -ladvapi32


native11-tests-build: $(BUILD_DIR)/native11_sprite_colour_policy.exe $(BUILD_DIR)/native11_sprite_colour_native.exe
$(BUILD_DIR)/native11_sprite_colour_policy.exe: tests/native11_sprite_colour_policy_test.cpp renderer/native11/sprite_colour_policy.cpp renderer/native11/sprite_colour_policy.hpp renderer/native11/native_arithmetic.cpp renderer/native11/native_arithmetic.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/sprite_colour_policy.cpp renderer/native11/native_arithmetic.cpp
$(BUILD_DIR)/native11_sprite_colour_native.exe: tests/native11_sprite_colour_native.cpp renderer/native11/sprite_colour_policy.cpp renderer/native11/sprite_colour_policy.hpp renderer/native11/native_arithmetic.cpp renderer/native11/native_arithmetic.hpp core/native11_sprite_profile.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/sprite_colour_policy.cpp renderer/native11/native_arithmetic.cpp -ladvapi32


native11-tests-build: $(BUILD_DIR)/native11_craft_view_policy.exe $(BUILD_DIR)/native11_craft_view_native.exe $(BUILD_DIR)/native11_sprite_observation.exe
$(BUILD_DIR)/native11_craft_view_policy.exe: tests/native11_craft_view_policy_test.cpp renderer/native11/craft_view_policy.cpp renderer/native11/craft_view_policy.hpp renderer/native11/native_arithmetic.cpp renderer/native11/native_arithmetic.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/craft_view_policy.cpp renderer/native11/native_arithmetic.cpp
$(BUILD_DIR)/native11_craft_view_native.exe: tests/native11_craft_view_native.cpp renderer/native11/craft_view_policy.cpp renderer/native11/craft_view_policy.hpp renderer/native11/native_arithmetic.cpp renderer/native11/native_arithmetic.hpp core/native11_craft_view_profile.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/craft_view_policy.cpp renderer/native11/native_arithmetic.cpp -ladvapi32
$(BUILD_DIR)/native11_sprite_observation.exe: tests/native11_sprite_observation_test.cpp renderer/native11/sprite_observation.cpp renderer/native11/sprite_observation.hpp renderer/native11/sprite_colour_policy.cpp renderer/native11/sprite_colour_policy.hpp renderer/native11/native_arithmetic.cpp renderer/native11/native_arithmetic.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/sprite_observation.cpp renderer/native11/sprite_colour_policy.cpp renderer/native11/native_arithmetic.cpp


native11-tests-build: $(BUILD_DIR)/native11_craft_view_capture.exe
$(BUILD_DIR)/native11_craft_view_capture.exe: tests/native11_craft_view_capture_test.cpp $(NATIVE11_VIEW_SOURCES) $(NATIVE11_VIEW_HEADERS) renderer/native11/native_arithmetic.cpp renderer/native11/native_arithmetic.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(NATIVE11_VIEW_SOURCES) renderer/native11/native_arithmetic.cpp


native11-tests-build: $(BUILD_DIR)/native11_sprite_bridge_test.exe
$(BUILD_DIR)/native11_sprite_bridge_test.exe: tests/native11_sprite_bridge_test.cpp tests/native11_sprite_bridge_test.S core/native11_sprite_bridge.S core/native11_sprites.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -msse2 -static -o $@ $< tests/native11_sprite_bridge_test.S core/native11_sprite_bridge.S


native11-tests-build: $(BUILD_DIR)/native11_sprite_guard.exe
$(BUILD_DIR)/native11_sprite_guard.exe: tests/native11_sprite_guard.cpp $(NATIVE11_SPRITE_CAPTURE_SOURCES) $(NATIVE11_SPRITE_CAPTURE_HEADERS) renderer/native11/native_arithmetic.cpp renderer/native11/native_arithmetic.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -DA2FO_NATIVE11_SPRITE_TESTING -Werror -static -o $@ $< $(NATIVE11_SPRITE_CAPTURE_SOURCES) renderer/native11/native_arithmetic.cpp -ladvapi32

# Passive builtin animation coverage has no native execution dependency.
native11-tests-build: $(BUILD_DIR)/native11_animation_coverage.exe
$(BUILD_DIR)/native11_animation_coverage.exe: tests/native11_animation_coverage_test.cpp modules/A2FOAnimations/native_observation.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $<
$(BUILD_DIR)/builtin_animations_native_test.exe: core/builtin_animation_observation.hpp modules/A2FOAnimations/native_observation.hpp modules/A2FOAnimations/module.cpp

# Checked-list DB associations are raw owned metadata, not render geometry.
native11-tests-build: $(BUILD_DIR)/native11_world_model_association.exe
$(BUILD_DIR)/native11_world_model_association.exe: tests/native11_world_model_association_test.cpp renderer/native11/world_model_association.cpp renderer/native11/world_model_association.hpp renderer/native11/world_light_capture.hpp renderer/native11/model_store.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/world_model_association.cpp

# Fresh world-roster distance/fade inputs, with explicit unavailable entries.
native11-tests-build: $(BUILD_DIR)/native11_world_view_capture.exe
$(BUILD_DIR)/native11_world_view_capture.exe: tests/native11_world_view_capture_test.cpp renderer/native11/world_view_capture.cpp renderer/native11/world_view_capture.hpp renderer/native11/world_model_association.cpp renderer/native11/world_model_association.hpp $(NATIVE11_VIEW_SOURCES) $(NATIVE11_VIEW_HEADERS) renderer/native11/native_arithmetic.cpp renderer/native11/native_arithmetic.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/world_view_capture.cpp renderer/native11/world_model_association.cpp $(NATIVE11_VIEW_SOURCES) renderer/native11/native_arithmetic.cpp

# Fresh logical Craft setup state: no native update calls or geometry claims.
native11-tests-build: $(BUILD_DIR)/native11_craft_preparation.exe $(BUILD_DIR)/native11_craft_preparation_native.exe $(BUILD_DIR)/native11_craft_preparation_capture.exe $(BUILD_DIR)/native11_world_preparation_capture.exe $(BUILD_DIR)/native11_craft_preparation_source.exe
$(BUILD_DIR)/native11_craft_preparation.exe: tests/native11_craft_preparation_test.cpp renderer/native11/craft_preparation.cpp renderer/native11/craft_preparation.hpp renderer/native11/native_arithmetic.cpp renderer/native11/native_arithmetic.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/craft_preparation.cpp renderer/native11/native_arithmetic.cpp
$(BUILD_DIR)/native11_craft_preparation_native.exe: tests/native11_craft_preparation_native.cpp renderer/native11/craft_preparation.cpp renderer/native11/craft_preparation.hpp core/native11_craft_preparation_profile.hpp renderer/native11/native_arithmetic.cpp renderer/native11/native_arithmetic.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/craft_preparation.cpp renderer/native11/native_arithmetic.cpp -ladvapi32
$(BUILD_DIR)/native11_craft_preparation_capture.exe: tests/native11_craft_preparation_capture_test.cpp renderer/native11/craft_preparation_capture.cpp renderer/native11/craft_preparation.cpp $(NATIVE11_PREPARATION_HEADERS) renderer/native11/native_arithmetic.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/craft_preparation_capture.cpp renderer/native11/craft_preparation.cpp renderer/native11/native_arithmetic.cpp
$(BUILD_DIR)/native11_world_preparation_capture.exe: tests/native11_world_preparation_capture_test.cpp renderer/native11/world_preparation_capture.cpp renderer/native11/craft_preparation_capture.cpp renderer/native11/craft_preparation.cpp renderer/native11/world_model_association.cpp renderer/native11/world_model_association.hpp $(NATIVE11_PREPARATION_HEADERS) renderer/native11/native_arithmetic.cpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/world_preparation_capture.cpp renderer/native11/craft_preparation_capture.cpp renderer/native11/craft_preparation.cpp renderer/native11/world_model_association.cpp renderer/native11/native_arithmetic.cpp
$(BUILD_DIR)/native11_craft_preparation_source.exe: tests/native11_craft_preparation_source_test.cpp core/native11_craft_preparation_source.cpp $(NATIVE11_PREPARATION_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< core/native11_craft_preparation_source.cpp -ladvapi32

# Prepared visibility consumes copied logical state, never last-rendered flags.
$(BUILD_DIR)/native11_craft_visibility_capture.exe: renderer/native11/craft_visibility_inputs.hpp renderer/native11/craft_preparation.hpp
native11-tests-build: $(BUILD_DIR)/native11_prepared_visibility_capture.exe
$(BUILD_DIR)/native11_prepared_visibility_capture.exe: tests/native11_prepared_visibility_capture_test.cpp renderer/native11/prepared_craft_visibility_capture.cpp renderer/native11/prepared_craft_visibility_capture.hpp renderer/native11/craft_visibility_inputs.hpp renderer/native11/craft_visibility_capture.cpp renderer/native11/craft_visibility_capture.hpp renderer/native11/craft_visibility.cpp renderer/native11/craft_visibility.hpp renderer/native11/craft_preparation_capture.cpp renderer/native11/craft_preparation_capture.hpp renderer/native11/craft_preparation.cpp renderer/native11/craft_preparation.hpp renderer/native11/native_arithmetic.cpp renderer/native11/native_arithmetic.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/prepared_craft_visibility_capture.cpp renderer/native11/craft_visibility_capture.cpp renderer/native11/craft_visibility.cpp renderer/native11/craft_preparation_capture.cpp renderer/native11/craft_preparation.cpp renderer/native11/native_arithmetic.cpp


native11-tests-build: $(BUILD_DIR)/native11_world_prepared_visibility.exe $(BUILD_DIR)/native11_craft_visibility_base_source.exe
$(BUILD_DIR)/native11_world_prepared_visibility.exe: tests/native11_world_prepared_visibility_test.cpp $(NATIVE11_VISIBILITY_POLICY_SOURCES) $(NATIVE11_VISIBILITY_HEADERS) renderer/native11/world_model_association.cpp renderer/native11/world_preparation_capture.cpp renderer/native11/craft_preparation_capture.cpp renderer/native11/craft_preparation.cpp renderer/native11/native_arithmetic.cpp $(NATIVE11_PREPARATION_HEADERS) | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(NATIVE11_VISIBILITY_POLICY_SOURCES) renderer/native11/world_model_association.cpp renderer/native11/world_preparation_capture.cpp renderer/native11/craft_preparation_capture.cpp renderer/native11/craft_preparation.cpp renderer/native11/native_arithmetic.cpp
$(BUILD_DIR)/native11_craft_visibility_base_source.exe: tests/native11_craft_visibility_base_source_test.cpp core/native11_craft_visibility_base_source.cpp core/native11_craft_visibility_base_source.hpp core/native11_craft_visibility_base_profile.hpp | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< core/native11_craft_visibility_base_source.cpp -ladvapi32

# Immutable raw mesh texture slots captured only at the completed model load.
native11-tests-build: $(BUILD_DIR)/native11_model_material_baseline.exe
$(BUILD_DIR)/native11_model_material_baseline.exe: tests/native11_model_material_baseline_test.cpp $(NATIVE11_BASELINE_SOURCES) $(NATIVE11_BASELINE_HEADERS) renderer/native11/model_node_template.hpp renderer/native11/material_observation.hpp Makefile | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -UNDEBUG -Werror -static -o $@ $< $(NATIVE11_BASELINE_SOURCES)

# Original base/Borg writes on owned prior slot state; not a full material producer.
NATIVE11_BORG_POLICY := renderer/native11/craft_borg_material_policy.cpp
NATIVE11_BORG_HEADERS := renderer/native11/craft_borg_material_policy.hpp renderer/native11/craft_borg_material_capture.hpp $(NATIVE11_BASELINE_HEADERS) renderer/native11/model_node_template.hpp renderer/native11/material_observation.hpp renderer/native11/texture_store.hpp renderer/native11/model_store.hpp renderer/native11/light_class_store.hpp renderer/native11/scene_capture.hpp
native11-tests-build: $(BUILD_DIR)/native11_craft_borg_material_policy.exe $(BUILD_DIR)/native11_craft_borg_material_native.exe $(BUILD_DIR)/native11_craft_borg_material_capture.exe
$(BUILD_DIR)/native11_craft_borg_material_policy.exe: tests/native11_craft_borg_material_policy_test.cpp $(NATIVE11_BORG_POLICY) $(NATIVE11_BORG_HEADERS) Makefile | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(NATIVE11_BORG_POLICY)
$(BUILD_DIR)/native11_craft_borg_material_native.exe: tests/native11_craft_borg_material_native.cpp $(NATIVE11_BORG_POLICY) $(NATIVE11_BORG_HEADERS) $(NATIVE11_BASELINE_SOURCES) renderer/native11/craft_borg_material_capture.cpp core/native11_borg_material_source.cpp core/native11_borg_material_source.hpp core/native11_borg_material_profile.hpp Makefile | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(NATIVE11_BORG_POLICY) renderer/native11/craft_borg_material_capture.cpp $(NATIVE11_BASELINE_SOURCES) core/native11_borg_material_source.cpp -ladvapi32
$(BUILD_DIR)/native11_craft_borg_material_capture.exe: tests/native11_craft_borg_material_capture_test.cpp renderer/native11/craft_borg_material_capture.cpp $(NATIVE11_BORG_POLICY) $(NATIVE11_BORG_HEADERS) $(NATIVE11_BASELINE_SOURCES) renderer/native11/model_lod_template.hpp Makefile | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/craft_borg_material_capture.cpp $(NATIVE11_BORG_POLICY) $(NATIVE11_BASELINE_SOURCES)

# Exact world/model/preparation join for diagnostic base/Borg candidate operations.
native11-tests-build: $(BUILD_DIR)/native11_world_borg_materials.exe
$(BUILD_DIR)/native11_world_borg_materials.exe: tests/native11_world_borg_materials_test.cpp $(NATIVE11_WORLD_BORG_SOURCES) $(NATIVE11_WORLD_BORG_HEADERS) $(NATIVE11_PREPARATION_SOURCES) $(NATIVE11_PREPARATION_HEADERS) $(NATIVE11_BASELINE_SOURCES) $(NATIVE11_BASELINE_HEADERS) renderer/native11/world_model_association.cpp renderer/native11/world_model_association.hpp renderer/native11/native_arithmetic.cpp renderer/native11/native_arithmetic.hpp Makefile | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< $(filter-out core/native11_borg_material_source.cpp,$(NATIVE11_WORLD_BORG_SOURCES)) $(sort $(filter-out core/native11_craft_preparation_source.cpp,$(NATIVE11_PREPARATION_SOURCES)) renderer/native11/world_model_association.cpp renderer/native11/native_arithmetic.cpp) $(NATIVE11_BASELINE_SOURCES)

# Class update quiescence: pure policy and synthetic factory/callback coordinator.
native11-tests-build: $(BUILD_DIR)/native11_class_update_barrier.exe $(BUILD_DIR)/native11_class_load_scope.exe
$(BUILD_DIR)/native11_class_update_barrier.exe: tests/native11_class_update_barrier_test.cpp renderer/native11/class_update_barrier.cpp renderer/native11/class_update_barrier.hpp Makefile | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< renderer/native11/class_update_barrier.cpp
$(BUILD_DIR)/native11_class_load_scope.exe: tests/native11_class_load_scope_native.cpp core/native11_light_classes.cpp core/native11_light_classes.hpp core/class_load_context.hpp renderer/native11/class_update_barrier.cpp renderer/native11/class_update_barrier.hpp renderer/native11/light_class_store.cpp renderer/native11/light_class_store.hpp renderer/native11/light_config.cpp renderer/native11/light_event.cpp rust/stage8_core_classload_ffi.h $(RUST_WIN_LIB) Makefile | $(BUILD_DIR)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -Werror -static -o $@ $< core/native11_light_classes.cpp renderer/native11/class_update_barrier.cpp renderer/native11/light_class_store.cpp renderer/native11/light_config.cpp renderer/native11/light_event.cpp $(RUST_WIN_LINK)

$(BUILD_DIR)/native11_light_class_native.exe: renderer/native11/class_update_barrier.cpp renderer/native11/class_update_barrier.hpp
$(BUILD_DIR)/native11_world_pipeline_native.exe $(BUILD_DIR)/native11_live_material_pipeline.exe: renderer/native11/class_update_barrier.cpp renderer/native11/class_update_barrier.hpp
