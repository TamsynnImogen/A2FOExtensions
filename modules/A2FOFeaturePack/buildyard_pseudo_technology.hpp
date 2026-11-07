#pragma once

/*
 * BuildYard/Borg module technology-tree gates (symbolic MODULE; v14 selected-tree rules).
 *
 * XML syntax:
 *
 *   <object MODULE="bor_test_module_gate.odf">
 *       <requirement>
 *           <obj>romR_leahval2.odf</obj>
 *       </requirement>
 *   </object>
 *
 * The MODULE name is symbolic and requires no backing ODF.
 *
 * Canonical BuildYard configuration command:
 *   moduleXTechnologyEntry = "bor_test_module_gate"
 *
 * Compatibility alias:
 *   moduleXPseudoTechnology = "bor_test_module_gate"
 *
 * Multiple <requirement> nodes are OR alternatives; multiple direct <obj>
 * nodes inside one requirement are AND conditions. <obj mustNotHave="true">
 * is supported. v14 also supports race, logicGroups, ally and enemy predicates.
 * Rules follow the native selected tree, including actual include/parentInclude
 * and archived XML loads. Unsupported caps/button hiding fail closed.
 * See docs/buildyard-module-v14.md for examples and acceptance tests.
 *
 * If moduleXRequiredTechnology is also present, it is ANDed with the MODULE
 * gate. v13 uses native completed-object counts for <obj>, excluding queued
 * objects, and appends currently missing requirements through the visually
 * verified native MSVCP60 tooltip path. Names use unitName or ODF basename.
 * See docs/buildyard-tooltip-abi.md.
 *
 * A real ODF named by TechnologyEntry/PseudoTechnology is still accepted as
 * the old recursive v4 compatibility path when no MODULE definition exists.
 */

#include "../../sdk/include/a2fo_module_api.h"

#include <windows.h>

namespace a2fo {

bool initialize_buildyard_pseudo_technology(
    const A2FO_ModuleApi* api, HMODULE armada,
    HMODULE fleet_ops) noexcept;

}  // namespace a2fo
