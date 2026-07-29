CXX_MINGW ?= i686-w64-mingw32-g++
CXX_HOST ?= g++

CPPFLAGS := -Isrc
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic
DLLFLAGS := -shared -static -static-libgcc -static-libstdc++ -Wl,--enable-stdcall-fixup

BUILD := build
DLL := $(BUILD)/A2FOExtensions.dll
STARTUP_PROXY := $(BUILD)/Win2kDisableTaskSwitch.proxy.dll
FPQ_PATHS_TEST := $(BUILD)/fpq_paths_test
ODF_PATHS_TEST := $(BUILD)/odf_paths_test
HOST_TESTS := $(FPQ_PATHS_TEST) $(ODF_PATHS_TEST)
SMOKE_TEST := $(BUILD)/dll_load_smoke.exe

DLL_SOURCES := src/dllmain.cpp src/hook.cpp src/fpq_paths.cpp src/odf_paths.cpp src/delphi_bridge.S

.PHONY: all clean smoke test

all: $(DLL) $(STARTUP_PROXY)

$(BUILD):
	mkdir -p $(BUILD)

$(DLL): $(DLL_SOURCES) src/hook.hpp src/fpq_paths.hpp src/odf_paths.hpp | $(BUILD)
	$(CXX_MINGW) $(CPPFLAGS) $(CXXFLAGS) $(DLLFLAGS) -o $@ $(DLL_SOURCES)

$(STARTUP_PROXY): src/startup_proxy.cpp src/startup_proxy.def | $(BUILD)
	$(CXX_MINGW) $(CXXFLAGS) $(DLLFLAGS) -o $@ $^ -luser32

$(FPQ_PATHS_TEST): tests/fpq_paths_test.cpp src/fpq_paths.cpp src/fpq_paths.hpp | $(BUILD)
	$(CXX_HOST) $(CPPFLAGS) $(CXXFLAGS) -o $@ tests/fpq_paths_test.cpp src/fpq_paths.cpp

$(ODF_PATHS_TEST): tests/odf_paths_test.cpp src/odf_paths.cpp src/odf_paths.hpp | $(BUILD)
	$(CXX_HOST) $(CPPFLAGS) $(CXXFLAGS) -o $@ tests/odf_paths_test.cpp src/odf_paths.cpp

test: $(HOST_TESTS)
	$(FPQ_PATHS_TEST)
	$(ODF_PATHS_TEST)

$(SMOKE_TEST): tests/dll_load_smoke.cpp | $(BUILD)
	$(CXX_MINGW) $(CXXFLAGS) -static -static-libgcc -static-libstdc++ -o $@ $<

smoke: $(DLL) $(SMOKE_TEST)
	cd $(BUILD) && wine dll_load_smoke.exe

clean:
	$(RM) -r $(BUILD)
