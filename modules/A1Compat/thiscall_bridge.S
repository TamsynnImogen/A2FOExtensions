	# ABI and mid-function continuation stubs owned by A1Compat. These wrappers
	# preserve the supported executable's exact 32-bit register/stack contract;
	# semantic compatibility policy remains in module.cpp.

	.text

	# ST3D_Node virtual methods use MSVC thiscall: self is supplied in ECX.
	# Keep that ABI detail outside C++ so MinGW cannot silently lower it as
	# cdecl and corrupt the call while diagnosing a malformed SOD node.
	.globl _a2fo_a1_call_thiscall_0
_a2fo_a1_call_thiscall_0:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_a1_call_thiscall_1
_a2fo_a1_call_thiscall_1:
	pushl %ebp
	movl %esp, %ebp
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_a1_call_thiscall_2
_a2fo_a1_call_thiscall_2:
	pushl %ebp
	movl %esp, %ebp
	pushl 20(%ebp)
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_a1_call_thiscall_3
_a2fo_a1_call_thiscall_3:
	pushl %ebp
	movl %esp, %ebp
	pushl 24(%ebp)
	pushl 20(%ebp)
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_a1_call_thiscall_4
_a2fo_a1_call_thiscall_4:
	pushl %ebp
	movl %esp, %ebp
	pushl 28(%ebp)
	pushl 24(%ebp)
	pushl 20(%ebp)
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	# AIP::m_UpdateTechnologyLevel retries a missing unit-class lookup but then
	# dereferences the null result. EDX is that result, EBX is the current
	# 0x80-byte Build_List_Element, and [EBP-8] is the AIP object. Preserve the
	# exact native path for resolved units and skip only the unresolved entry.
	.globl _a2fo_a1_aip_technology_unit_guard_hook
_a2fo_a1_aip_technology_unit_guard_hook:
	testl %edx, %edx
	jz .Laip_technology_unit_missing
	movl -8(%ebp), %ecx
	movl 0x6c(%edx), %eax
	jmp *_g_a2fo_a1_aip_technology_unit_continuation

.Laip_technology_unit_missing:
	pushal
	movl -8(%ebp), %eax
	addl $0x248, %eax
	leal 0x0c(%ebx), %edx
	pushl %edx
	pushl %eax
	call _a2fo_a1_report_missing_aip_technology_unit
	addl $8, %esp
	popal
	jmp *_g_a2fo_a1_aip_technology_unit_skip

	# AIP_Manager::Look_Up_New_AIP is MSVC thiscall with the manager in ECX
	# and one callee-popped name argument. Let C++ select an A1 build-list name,
	# call the native gateway, and reproduce the original ret 4 contract.
	.globl _a2fo_a1_aip_lookup_hook
_a2fo_a1_aip_lookup_hook:
	pushl %ebp
	movl %esp, %ebp
	pushl 8(%ebp)
	pushl %ecx
	call _a2fo_a1_resolve_aip_lookup
	addl $8, %esp
	leave
	ret $4

	# GameObjectClass's one `resource` lookup is MSVC thiscall with the
	# ParameterDB in ECX and four stack arguments. A1Compat first preserves the
	# normal/core resolver, then supplies an A2 Classic missing-only value for
	# the exact legacy Scrap/moon ODF family. Reproduce thiscall's ret 16.
	.globl _a2fo_a1_game_object_resource_lookup_hook
_a2fo_a1_game_object_resource_lookup_hook:
	pushl %ebp
	movl %esp, %ebp
	pushl 20(%ebp)
	pushl 16(%ebp)
	pushl 12(%ebp)
	pushl 8(%ebp)
	pushl %ecx
	call _a2fo_a1_resolve_game_object_resource
	addl $20, %esp
	leave
	ret $16

	# PhysicsClass's `physics` selector lookup has the same GetString thiscall
	# ABI. Preserve normal resolution, then translate the legacy borgPhysics
	# boolean (or default ordinary A1 craft to A2 smooth physics) only when it
	# is absent.
	.globl _a2fo_a1_physics_model_lookup_hook
_a2fo_a1_physics_model_lookup_hook:
	pushl %ebp
	movl %esp, %ebp
	pushl 20(%ebp)
	pushl 16(%ebp)
	pushl 12(%ebp)
	pushl 8(%ebp)
	pushl %ecx
	call _a2fo_a1_resolve_physics_model
	addl $20, %esp
	leave
	ret $16

	# Checked replacement for DisplayInterface::PostLoadAll's one direct call
	# to ST3D_TextFileParser::ReadTable. The native caller supplies the parser
	# in ECX and two stack arguments (database, filename). Forward that exact
	# state to C++ policy, which loads the essential A2 table first and the
	# caller-selected gui_global.spr second, then reproduce thiscall's ret 8.
	.globl _a2fo_a1_gui_sprite_read_table_hook
_a2fo_a1_gui_sprite_read_table_hook:
	pushl %ebp
	movl %esp, %ebp
	pushl 12(%ebp)
	pushl 8(%ebp)
	pushl %ecx
	call _a2fo_a1_load_gui_sprite_tables
	addl $12, %esp
	leave
	ret $8

	# Post-construction boundary in DisplayInterface::PostLoadAll. EAX is the
	# freshly built gameplay ParameterDB and EDI is the selected Race, whose
	# inline CFG filename begins at +0x194. Configure the database, then let the
	# gateway publish EAX to DisplayInterface's global pointer unchanged.
	.globl _a2fo_a1_gui_parameter_db_post_construct_hook
_a2fo_a1_gui_parameter_db_post_construct_hook:
	pushfl
	pushal
	movl 0(%esp), %edx
	movl 28(%esp), %eax
	addl $0x194, %edx
	pushl %edx
	pushl %eax
	call _a2fo_a1_configure_gui_parameter_db
	addl $8, %esp
	popal
	popfl
	jmp *_g_a2fo_a1_gui_parameter_db_post_construct_gateway

	# Race::InitAll reads the declared count through ParameterDB::GetInt.
	# Preserve the native thiscall contract while A1Compat appends a runtime-only
	# neutral Race when the A1 registry omits A2's required norace entry.
	.globl _a2fo_a1_race_count_lookup_hook
_a2fo_a1_race_count_lookup_hook:
	pushl %ebp
	movl %esp, %ebp
	pushl 16(%ebp)
	pushl 12(%ebp)
	pushl 8(%ebp)
	pushl %ecx
	call _a2fo_a1_resolve_race_count
	addl $16, %esp
	leave
	ret $12

	# The matching Race::InitAll GetString call receives raceN. Normal lookup
	# remains authoritative; only the synthetic final index receives
	# norace.odf after the native lookup reports it missing.
	.globl _a2fo_a1_race_entry_lookup_hook
_a2fo_a1_race_entry_lookup_hook:
	pushl %ebp
	movl %esp, %ebp
	pushl 20(%ebp)
	pushl 16(%ebp)
	pushl 12(%ebp)
	pushl 8(%ebp)
	pushl %ecx
	call _a2fo_a1_resolve_race_entry
	addl $20, %esp
	leave
	ret $16

	# Missing-only A2 Classic neutral-Race default for
	# CraftEnhancement.Craft_mLevelUp. At this
	# point EAX is the Race pointer obtained through craft+0xf0 (Side), then
	# Side+0x244 (Race); A1 races.odf can omit A2's `norace` record, leaving
	# neutral scenery with EAX == null. The native
	# Craft pointer is stored at EBP-4 and the force flag remains in EBX.
	# Preserve every register and flag and report the object in C++. Valid Race
	# pointers execute the displaced Race+0x634 read unchanged through the
	# gateway. A null Race receives `canGainXP = false` in EAX and continues
	# after the displaced read. JECXZ and the data-movement instructions leave
	# the native flags unchanged on both paths.
	.globl _a2fo_a1_craft_level_up_race_hook
_a2fo_a1_craft_level_up_race_hook:
	pushfl
	pushal
	movl 28(%esp), %eax
	pushl %eax
	pushl %ebx
	pushl 4(%ebp)
	pushl -4(%ebp)
	call _a2fo_a1_log_craft_level_up_race
	addl $16, %esp
	popal
	popfl
	pushl %ecx
	movl %eax, %ecx
	jecxz .La2fo_a1_craft_level_up_no_race
	popl %ecx
	jmp *_g_a2fo_a1_craft_level_up_race_gateway
.La2fo_a1_craft_level_up_no_race:
	popl %ecx
	movl $0, %eax
	jmp *_g_a2fo_a1_craft_level_up_race_continuation

	# Fleet Operations' Starbase::InitializeGeometry no longer prepares the
	# Armada 1 oqN branches before cloning the shared class hierarchy. ECX is
	# the Starbase instance at entry. Preserve the complete native state while
	# C++ applies the A1 visibility flags, then execute the checked six-byte
	# prologue through the gateway and continue in the original function.
	.globl _a2fo_a1_starbase_initialize_geometry_hook
_a2fo_a1_starbase_initialize_geometry_hook:
	pushfl
	pushal
	pushl 24(%esp)
	call _a2fo_a1_prepare_starbase_officer_quarters
	addl $4, %esp
	popal
	popfl
	jmp *_g_a2fo_a1_starbase_initialize_geometry_gateway

	# A1 relationship bridge at GameTypeToTheDeath::CheckAll. ECX is the
	# GameType instance. Preserve the full caller state while the C wrapper
	# restores relationships and invokes the original gateway.
	.globl _a2fo_a1_to_the_death_check_hook
_a2fo_a1_to_the_death_check_hook:
	pushfl
	pushal
	movl 24(%esp), %eax
	movl 36(%esp), %edx
	pushl %edx
	pushl %eax
	call _a2fo_a1_run_to_the_death_check
	addl $8, %esp
	popal
	popfl
	ret
