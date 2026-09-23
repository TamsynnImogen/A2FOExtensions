	.text
	.globl _a2fo_station_position_bridge
_a2fo_station_position_bridge:
	# Replaces CALL at Armada RVA 0x031500. ECX is the build interface;
	# EDI is the AiCmdInfo admitted by CraftProcess (also for HybridBuild).
	# Original stack: return, Vector3*, GameObjectClass*; original ret 8.
	pushl %edi
	pushl 12(%esp)
	pushl 12(%esp)
	pushl %ecx
	call _a2fo_station_apply_position
	addl $16, %esp
	ret $8

	# Checked path-planner CALL sites supply their live object in the named
	# nonvolatile register. Original stack: return, position, BBOX, bool.
	.macro planner_bridge name, owner, callee
	.globl _\name
_\name:
	pushl %\owner
	pushl 16(%esp)
	pushl 16(%esp)
	pushl 16(%esp)
	pushl %ecx
	call _\callee
	addl $20, %esp
	ret $12
	.endm
	planner_bridge a2fo_station_add_esi, esi, a2fo_station_add_footprint
	planner_bridge a2fo_station_add_ebx, ebx, a2fo_station_add_footprint
	planner_bridge a2fo_station_remove_esi, esi, a2fo_station_remove_footprint
	planner_bridge a2fo_station_remove_edi, edi, a2fo_station_remove_footprint
	planner_bridge a2fo_station_remove_ebx, ebx, a2fo_station_remove_footprint
