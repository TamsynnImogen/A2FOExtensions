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

	# Mid-function diagnostic for RtimeClass::Load. EBP is still the native
	# function's frame and its 40-byte serialized type-name buffer starts at
	# EBP-0x30. Preserve the complete integer/flags state, report the name,
	# native caller, FileReader, and the second read's AL result, then enter the
	# gateway so the displaced instruction and native flow continue.
	.globl _a2fo_a1_rtime_load_name_hook
_a2fo_a1_rtime_load_name_hook:
	pushfl
	pushal
	pushl 28(%esp)
	pushl 8(%ebp)
	pushl 4(%ebp)
	leal -0x30(%ebp), %eax
	pushl %eax
	call _a2fo_a1_log_rtime_class_name
	addl $16, %esp
	popal
	popfl
	jmp *_g_a2fo_a1_rtime_load_name_gateway

	# Mid-function diagnostic for StandardText::InitializeConfiguration. The
	# ST3D sprite lookup result is still in EAX, the generated sprite-name
	# TString is at EBP-0x28, and the configuration TString reference is the
	# first native argument. Preserve state, report only null results in C++,
	# then execute the displaced temporary-string cleanup setup unchanged.
	.globl _a2fo_a1_standard_text_sprite_hook
_a2fo_a1_standard_text_sprite_hook:
	pushfl
	pushal
	pushl 28(%esp)
	pushl %esi
	pushl 4(%ebp)
	leal -0x28(%ebp), %eax
	pushl %eax
	pushl 8(%ebp)
	call _a2fo_a1_log_standard_text_sprite_lookup
	addl $20, %esp
	popal
	popfl
	jmp *_g_a2fo_a1_standard_text_sprite_gateway

	# Mid-function diagnostic for CraftEnhancement.Craft_mLevelUp. At this
	# point EAX is the Race pointer obtained through craft+0xf0 (Side), then
	# Side+0x244 (Race); the current A1 failure has EAX == null. The native
	# Craft pointer is stored at EBP-4 and the force flag remains in EBX.
	# Preserve every register and flag, report the object in C++, then execute
	# the displaced Race+0x634 canGainXP read unchanged through the gateway.
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
	jmp *_g_a2fo_a1_craft_level_up_race_gateway

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
