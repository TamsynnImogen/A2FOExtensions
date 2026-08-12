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
	call _a2fo_nebula_nonvb_pre
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
	jmp *_g_a2fo_nebula_workspace_dx8_draw_gateway

	# ST3D_DeviceDirectX8 is about to call IDirect3DDevice8::EndScene. ESI is
	# the live device, and the scene remains open long enough to blur and add
	# the accumulated ODF emissive mask to the completed frame.
	.globl _a2fo_nebula_frame_bloom_hook
_a2fo_nebula_frame_bloom_hook:
	pushfl
	pushal
	pushl 4(%esp)
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

	# The DOT3 indexed draw has EAX=device, EBX=mesh stream and ESI=triangle
	# count. Accumulate its ODF emissive sampler before replaying the untouched
	# native draw call and its already-pushed Direct3D arguments.
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
	jmp *_g_a2fo_nebula_dot3_draw_gateway
