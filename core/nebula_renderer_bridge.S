	# Core-owned MSVC thiscall bridges and early DX8 Nebula renderer stubs.

	.text

	.globl _a2fo_nebula_call_thiscall_1
_a2fo_nebula_call_thiscall_1:
	pushl %ebp
	movl %esp, %ebp
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	.globl _a2fo_nebula_call_thiscall_2
_a2fo_nebula_call_thiscall_2:
	pushl %ebp
	movl %esp, %ebp
	pushl 20(%ebp)
	pushl 16(%ebp)
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

	# Storm3D's shader-handle lookup is thiscall with one stack argument.
	.globl _a2fo_nebula_set_pixel_shader_hook
_a2fo_nebula_set_pixel_shader_hook:
	pushl %ebp
	movl %esp, %ebp
	pushl 8(%ebp)
	pushl %ecx
	call _a2fo_nebula_set_pixel_shader
	addl $8, %esp
	leave
	ret $4

	# Disable the custom shader, then replay the displaced Fleet Operations
	# instructions via the checked inline-hook gateway. The original EAX and
	# flags are restored because the displaced block consumes both.
	.globl _a2fo_nebula_alpha_hook
_a2fo_nebula_alpha_hook:
	pushfl
	pushal
	call _a2fo_nebula_disable_pixel_shader
	popal
	popfl
	jmp *_g_a2fo_nebula_alpha_gateway

	# Standard (non-DOT3) meshes use the fixed-function texture combiner. The
	# pre-draw helper temporarily adds the current craft's emissive texture at
	# stage 1 after Armada has configured the material; the post-draw helper
	# restores every displaced state before the next mesh.
	.globl _a2fo_nebula_standard_pre_hook
_a2fo_nebula_standard_pre_hook:
	pushfl
	pushal
	call _a2fo_nebula_standard_pre
	popal
	popfl
	jmp *_g_a2fo_nebula_standard_pre_gateway

	.globl _a2fo_nebula_standard_post_hook
_a2fo_nebula_standard_post_hook:
	pushfl
	pushal
	pushl 4(%esp)
	call _a2fo_nebula_standard_post
	addl $4, %esp
	popal
	popfl
	jmp *_g_a2fo_nebula_standard_post_gateway

	# Legacy SODs queue their faces in CPU/workspace buffers, finalize the counts
	# with Flush, apply the material pass, and only then issue the Direct3D draw
	# through Workspace::Submit. Keep the emissive stage around that Submit call
	# and replay both safely relocatable displaced blocks.
	.globl _a2fo_nebula_nonvb_pre_hook
_a2fo_nebula_nonvb_pre_hook:
	pushfl
	pushal
	# ESI is the native material-pass index.  The surrounding Armada function
	# stores its pass count at [EBP-4] immediately before entering this loop.
	movl 4(%esp), %edx
	movl 8(%esp), %eax
	pushl -4(%eax)
	pushl %edx
	call _a2fo_nebula_nonvb_pre
	addl $8, %esp
	popal
	popfl
	jmp *_g_a2fo_nebula_nonvb_pre_gateway

	.globl _a2fo_nebula_nonvb_post_hook
_a2fo_nebula_nonvb_post_hook:
	pushfl
	pushal
	pushl 16(%esp)
	call _a2fo_nebula_nonvb_post
	addl $4, %esp
	popal
	popfl
	jmp *_g_a2fo_nebula_nonvb_post_gateway

	# The GPU-buffer workspace has reached its exact native indexed draw. EDI is
	# the live device and ESI is the workspace. EBX, ECX, and EAX hold the exact
	# NumVertices, StartIndex, and PrimitiveCount values already pushed for the
	# native call; pass copies to the helper while leaving that stack untouched.
	.globl _a2fo_nebula_workspace_dx8_draw_hook
_a2fo_nebula_workspace_dx8_draw_hook:
	pushfl
	pushal
	pushl 28(%esp)
	pushl 28(%esp)
	pushl 24(%esp)
	pushl 16(%esp)
	pushl 16(%esp)
	call _a2fo_nebula_workspace_dx8_draw
	addl $20, %esp
	popal
	popfl
	call *0x11c(%edx)
	pushfl
	pushal
	call _a2fo_nebula_workspace_dx8_post
	popal
	popfl
	jmp *_g_a2fo_nebula_workspace_dx8_draw_return

	# Reset the private emissive-mask lifecycle at the authoritative native
	# BeginScene boundary. ESI is the live device and the original COM call is
	# replayed through the inline-hook gateway after all registers are restored.
	.globl _a2fo_nebula_frame_begin_hook
_a2fo_nebula_frame_begin_hook:
	pushfl
	pushal
	pushl 4(%esp)
	call _a2fo_nebula_frame_begin
	addl $4, %esp
	popal
	popfl
	jmp *_g_a2fo_nebula_frame_begin_gateway

	# ST3D_DeviceDirectX8::RefreshDisplay is in its final scene immediately
	# before Present. ESI is the ST3D device wrapper; +0x90 is the live D3D8
	# device. Composite here exactly once per displayed frame, then replay the
	# displaced `mov eax,[esi+0x90]; push eax` through the gateway.
	.globl _a2fo_nebula_frame_bloom_hook
_a2fo_nebula_frame_bloom_hook:
	pushfl
	pushal
	movl 4(%esp), %eax
	movl 0x90(%eax), %eax
	pushl %eax
	call _a2fo_nebula_frame_bloom
	addl $4, %esp
	popal
	popfl
	jmp *_g_a2fo_nebula_frame_bloom_gateway

	# Release A2FOExtensions' default-pool render targets before Armada calls
	# IDirect3DDevice8::Reset. EAX is the live device at this checked site.
	.globl _a2fo_nebula_device_reset_hook
_a2fo_nebula_device_reset_hook:
	pushfl
	pushal
	pushl 28(%esp)
	call _a2fo_nebula_before_device_reset
	addl $4, %esp
	popal
	popfl
	jmp *_g_a2fo_nebula_device_reset_gateway

	# Fleet Operations' checked DestroyDevice callback receives the live
	# ST3D_DeviceDirectX8 wrapper in ECX. Drop A2FO's GPU resources and owned COM
	# reference before replaying Fleet Operations' callback prologue and native
	# destruction route.
	.globl _a2fo_nebula_device_destroy_hook
_a2fo_nebula_device_destroy_hook:
	pushfl
	pushal
	pushl 24(%esp)
	call _a2fo_nebula_before_device_destroy
	addl $4, %esp
	popal
	popfl
	jmp *_g_a2fo_nebula_device_destroy_gateway

	# The DOT3 indexed draw has EAX=device, EBX=mesh stream and ESI=triangle
	# count. Configure any native emissive fallback, execute the displaced COM
	# draw exactly once with its already-pushed arguments, restore stage state,
	# and preserve the HRESULT in EAX for Armada's following test.
	.globl _a2fo_nebula_dot3_draw_hook
_a2fo_nebula_dot3_draw_hook:
	pushfl
	pushal
	pushl 4(%esp)
	pushl 20(%esp)
	pushl 36(%esp)
	call _a2fo_nebula_dot3_draw
	addl $12, %esp
	popal
	popfl
	call *0x11c(%edx)
	pushfl
	pushal
	call _a2fo_nebula_dot3_post
	popal
	popfl
	jmp *_g_a2fo_nebula_dot3_draw_return

	# Fleet Operations' replacement DOT3 renderer reaches its primary indexed
	# draw with EAX holding the live device vtable and the device itself as the
	# first already-pushed COM argument. Bind stage 2 after FO's final diffuse
	# assignment, optionally replay the exact vertex/triangle range into the
	# selective bloom mask, preserve the draw HRESULT, then restore state.
	.globl _a2fo_nebula_fleetops_dot3_draw_hook
_a2fo_nebula_fleetops_dot3_draw_hook:
	pushfl
	pushal
	# Original COM arguments after pushfl/pushal: device at +36,
	# NumVertices at +48 and PrimitiveCount at +56. Account for each push while
	# constructing the cdecl helper arguments in reverse order.
	pushl 56(%esp)
	pushl 52(%esp)
	pushl 44(%esp)
	call _a2fo_nebula_fleetops_dot3_draw
	addl $12, %esp
	popal
	popfl
	call *0x11c(%eax)
	pushfl
	pushal
	call _a2fo_nebula_fleetops_dot3_post
	popal
	popfl
	jmp *_g_a2fo_nebula_fleetops_dot3_draw_return
