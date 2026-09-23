	# Core-owned MSVC thiscall bridges and early DX8 Nebula renderer stubs.

	.text

	.globl _a2fo_nebula_call_thiscall_0
_a2fo_nebula_call_thiscall_0:
	pushl %ebp
	movl %esp, %ebp
	movl 12(%ebp), %ecx
	call *8(%ebp)
	leave
	ret

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

	# Replay ST3D_Mesh::Render's complete MeshVB selector. Normally this follows
	# Armada's pointer, virtual eligibility, polygon-sort, and external-renderer
	# checks exactly. With the fast non-bump policy active, a native-opaque
	# material in a global alpha transition can apply Storm3D's own z-sort blend
	# state immediately and retain the GPU MeshVB route. Authored transparent
	# materials continue through the native CPU triangle sorter.
	.globl _a2fo_nebula_mesh_selector_hook
_a2fo_nebula_mesh_selector_hook:
	movl 0x128(%esi), %ecx
	testl %ecx, %ecx
	jz .Lmesh_selector_no_vb
	movl (%ecx), %eax
	pushl %ebx
	call *0x4(%eax)
	testb %al, %al
	jz .Lmesh_selector_eligibility
	movl (%ebx), %edx
	movl %ebx, %ecx
	call *0x74(%edx)
	testb %al, %al
	jnz .Lmesh_selector_polygon_sort
	testl %edi, %edi
	jnz .Lmesh_selector_external
	pushfl
	pushal
	pushl $4
	call _a2fo_nebula_mesh_selector_reason
	# Original reason plus saved ESI=mesh and EBX=device wrapper.
	movl 8(%esp), %eax
	movl 20(%esp), %edx
	movl (%esp), %ecx
	pushl %edx
	pushl %eax
	pushl %ecx
	call _a2fo_nebula_general_mesh_selector
	addl $12, %esp
	addl $4, %esp
	popal
	popfl
	jmp *_g_a2fo_nebula_mesh_selector_fast

.Lmesh_selector_no_vb:
	pushfl
	pushal
	pushl $0
	call _a2fo_nebula_mesh_selector_reason
	# Original reason plus saved ESI=mesh and EBX=device wrapper.
	movl 8(%esp), %eax
	movl 20(%esp), %edx
	movl (%esp), %ecx
	pushl %edx
	pushl %eax
	pushl %ecx
	call _a2fo_nebula_general_mesh_selector
	addl $12, %esp
	addl $4, %esp
	popal
	popfl
	jmp *_g_a2fo_nebula_mesh_selector_legacy

.Lmesh_selector_eligibility:
	pushfl
	pushal
	pushl $1
	call _a2fo_nebula_mesh_selector_reason
	# Original reason plus saved ESI=mesh and EBX=device wrapper.
	movl 8(%esp), %eax
	movl 20(%esp), %edx
	movl (%esp), %ecx
	pushl %edx
	pushl %eax
	pushl %ecx
	call _a2fo_nebula_general_mesh_selector
	addl $12, %esp
	addl $4, %esp
	popal
	popfl
	jmp *_g_a2fo_nebula_mesh_selector_legacy

.Lmesh_selector_polygon_sort:
	testl %edi, %edi
	jnz .Lmesh_selector_polygon_sort_fallback
	pushl %ebx
	pushl -4(%ebp)
	call _a2fo_nebula_try_fast_nonbump_alpha_meshvb
	addl $8, %esp
	testl %eax, %eax
	jnz .Lmesh_selector_alpha_fast

.Lmesh_selector_polygon_sort_fallback:
	pushfl
	pushal
	pushl $2
	call _a2fo_nebula_mesh_selector_reason
	# Original reason plus saved ESI=mesh and EBX=device wrapper.
	movl 8(%esp), %eax
	movl 20(%esp), %edx
	movl (%esp), %ecx
	pushl %edx
	pushl %eax
	pushl %ecx
	call _a2fo_nebula_general_mesh_selector
	addl $12, %esp
	addl $4, %esp
	popal
	popfl
	jmp *_g_a2fo_nebula_mesh_selector_legacy

.Lmesh_selector_alpha_fast:
	pushfl
	pushal
	pushl $5
	call _a2fo_nebula_mesh_selector_reason
	# Original reason plus saved ESI=mesh and EBX=device wrapper.
	movl 8(%esp), %eax
	movl 20(%esp), %edx
	movl (%esp), %ecx
	pushl %edx
	pushl %eax
	pushl %ecx
	call _a2fo_nebula_general_mesh_selector
	addl $12, %esp
	addl $4, %esp
	popal
	popfl
	jmp *_g_a2fo_nebula_mesh_selector_fast

.Lmesh_selector_external:
	pushfl
	pushal
	pushl $3
	call _a2fo_nebula_mesh_selector_reason
	# Original reason plus saved ESI=mesh and EBX=device wrapper.
	movl 8(%esp), %eax
	movl 20(%esp), %edx
	movl (%esp), %ecx
	pushl %edx
	pushl %eax
	pushl %ecx
	call _a2fo_nebula_general_mesh_selector
	addl $12, %esp
	addl $4, %esp
	popal
	popfl
	jmp *_g_a2fo_nebula_mesh_selector_legacy

	# Standard (non-DOT3) meshes use the fixed-function texture combiner. The
	# pre-draw helper temporarily adds the current craft's emissive texture at
	# stage 1 after Armada has configured the material; the post-draw helper
	# restores every displaced state before the next mesh.
	# Only the non-DOT3 construction branch is intercepted. Preserve the
	# complete native standard-MeshVB decision on rejection. ESI is the mesh;
	# after pushfl/pushal its saved value is at +4. Successful promotion has
	# already assigned mesh+0x128, so continue after native factory selection.
	.globl _a2fo_nebula_unmapped_meshvb_create_hook
_a2fo_nebula_unmapped_meshvb_create_hook:
	pushfl
	pushal
	pushl 4(%esp)
	call _a2fo_nebula_create_unmapped_or_phong_meshvb
	addl $4, %esp
	testl %eax, %eax
	jnz .Lunmapped_created
	popal
	popfl
	testb $1, %al
	jz .Lunmapped_native_skip
	pushl %esi
	jmp *_g_a2fo_unmapped_standard_create_return
.Lunmapped_created:
	popal
	popfl
.Lunmapped_native_skip:
	jmp *_g_a2fo_unmapped_create_done_return

	.globl _a2fo_nebula_standard_pre_hook
_a2fo_nebula_standard_pre_hook:
	pushfl
	pushal
	call _a2fo_nebula_phong_pre
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
	call _a2fo_nebula_phong_post
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

	# Fleet Operations' per-light DOT3 helper submits through the same D3D8
	# indexed-draw slot. While native bumps are disabled, replace only this
	# pass's vertex shader and stage-0 DOT3 combiner, execute the original draw,
	# then restore both before Fleet Operations advances to the next light.
	.globl _a2fo_nebula_fleetops_dot3_light_draw_hook
_a2fo_nebula_fleetops_dot3_light_draw_hook:
	# Cloak lighting is collected for the final textured draw. Never expose
	# FO's opaque lighting-only accumulation in the scene framebuffer.
	pushfl
	pushal
	pushl 36(%esp)
	call _a2fo_nebula_cloak_collect_light
	addl $4, %esp
	testl %eax, %eax
	jnz .Lcloak_light_collected
	popal
	popfl
	pushfl
	pushal
	pushl 36(%esp)
	call _a2fo_nebula_fast_nonbump_light_draw
	addl $4, %esp
	popal
	popfl
	call *0x11c(%eax)
	pushfl
	pushal
	call _a2fo_nebula_fast_nonbump_light_post
	popal
	popfl
	jmp *_g_a2fo_nebula_fleetops_dot3_light_draw_return
.Lcloak_light_collected:
	popal
	popfl
	# Six stdcall arguments were already pushed for DrawIndexedPrimitive.
	addl $24, %esp
	xorl %eax, %eax
	jmp *_g_a2fo_nebula_fleetops_dot3_light_draw_return

	# Fleet Operations' replacement DOT3 renderer reaches its primary indexed
	# draw with EAX holding the live device vtable and the device itself as the
	# first already-pushed COM argument. Bind stage 2 after FO's final diffuse
	# assignment, optionally replay the exact vertex/triangle range into the
	# selective bloom mask, preserve the draw HRESULT, then restore state.
	.globl _a2fo_nebula_fleetops_dot3_draw_hook
_a2fo_nebula_fleetops_dot3_draw_hook:
	pushfl
	pushal
	pushl 36(%esp)
	call _a2fo_nebula_cloak_begin_final
	addl $4, %esp
	# FO callback's saved EBP gives its lighting-material argument.
	movl 8(%esp), %eax
	pushl 12(%eax)
	pushl 40(%esp)
	call _a2fo_nebula_general_mesh_material
	addl $8, %esp
	# Original COM arguments after pushfl/pushal: device at +36,
	# NumVertices at +48 and PrimitiveCount at +56. Account for each push while
	# constructing the cdecl helper arguments in reverse order.
	pushl 56(%esp)
	pushl 52(%esp)
	pushl 44(%esp)
	call _a2fo_nebula_fleetops_dot3_draw
	addl $12, %esp
	pushl 36(%esp)
	call _a2fo_nebula_cloak_final_pre
	addl $4, %esp
	# Native FO group info is [EBP-0x1c]; the pending DIP count is ESP+56.
	movl 8(%esp), %eax
	pushl 56(%esp)
	pushl -28(%eax)
	pushl 44(%esp)
	call _a2fo_nebula_transparent_indices_pre
	addl $12, %esp
	popal
	popfl
	call *0x11c(%eax)
	pushfl
	pushal
	call _a2fo_nebula_transparent_indices_post
	call _a2fo_nebula_cloak_final_post
	call _a2fo_nebula_fleetops_dot3_post
	popal
	popfl
	jmp *_g_a2fo_nebula_fleetops_dot3_draw_return
