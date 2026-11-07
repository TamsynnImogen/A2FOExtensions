#pragma once

#include <cstdint>

using A2foStage9RendererStep = std::uint64_t (*)(void*, std::uint32_t, std::uint32_t);

extern "C" {
std::uint32_t a2fo_rs_stage9_renderer_join(void*, A2foStage9RendererStep);
bool a2fo_rs_stage9_renderer_same_file(void*, A2foStage9RendererStep);
bool a2fo_rs_stage9_renderer_ensure_directory(void*, A2foStage9RendererStep);
std::uint32_t a2fo_rs_stage9_renderer_requested(void*, A2foStage9RendererStep);
void a2fo_rs_stage9_renderer_form_show(void*, A2foStage9RendererStep);
void a2fo_rs_stage9_renderer_checkbox(void*, A2foStage9RendererStep);
void a2fo_rs_stage9_renderer_combo_change(void*, A2foStage9RendererStep);
bool a2fo_rs_stage9_renderer_file_exists(std::uint32_t);
std::uint32_t a2fo_rs_stage9_renderer_backend_from_text(
    void*, A2foStage9RendererStep);
std::uint32_t a2fo_rs_stage9_renderer_backend_label(std::uint32_t);
bool a2fo_rs_stage9_renderer_write_effect(void*, A2foStage9RendererStep);
std::int32_t a2fo_rs_stage9_renderer_native_display(
    void*, A2foStage9RendererStep);
void a2fo_rs_stage9_renderer_restart_text(void*, A2foStage9RendererStep);
}
