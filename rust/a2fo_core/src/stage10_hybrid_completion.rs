//! Portable HybridBuild traversal order. Storage and checked native access
//! remain behind synchronous C ABI probes.
use core::ffi::c_void;

type Probe = unsafe extern "C" fn(*mut c_void, usize, u32) -> u32;

#[repr(C)]
pub struct VisitResult {
    index: usize,
    status: u32,
}

/// # Safety
/// `context` and `probe` must remain valid for every synchronous probe.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_hybrid_visit(
    kind: u32,
    count: usize,
    context: *mut c_void,
    probe: Probe,
) -> VisitResult {
    let mut result = VisitResult {
        index: count,
        status: 0,
    };
    match kind {
        0 | 1 => {
            for index in 0..count {
                let outcome = unsafe { probe(context, index, 0) };
                if outcome == 2 {
                    result.status = 1;
                    return result;
                }
                if kind == 0 && outcome == 1 {
                    result.index = index;
                    return result;
                }
            }
        }
        2 => {
            let mut first_empty = count;
            for index in 0..count {
                let outcome = unsafe { probe(context, index, 0) };
                if outcome == 2 {
                    result.status = 1;
                    return result;
                }
                if outcome == 1 {
                    result.index = index;
                    return result;
                }
                if first_empty == count {
                    let empty = unsafe { probe(context, index, 1) };
                    if empty == 2 {
                        result.status = 1;
                        return result;
                    }
                    if empty == 1 {
                        first_empty = index;
                    }
                }
            }
            result.index = first_empty;
        }
        4 => {
            let mut first_empty = count;
            for index in 0..count {
                match unsafe { probe(context, index, 0) } {
                    1 => {
                        result.index = index;
                        return result;
                    }
                    2 => {
                        result.status = 1;
                        return result;
                    }
                    5 if first_empty == count => first_empty = index,
                    _ => {}
                }
            }
            result.index = first_empty;
        }
        5 => {
            let mut included = 0usize;
            for index in 0..count {
                match unsafe { probe(context, index, 0) } {
                    1 => {
                        if unsafe { probe(context, index, (included + 1) as u32) } == 2 {
                            result.status = 1;
                            return result;
                        }
                        included += 1;
                    }
                    2 => {
                        result.status = 1;
                        return result;
                    }
                    _ => {}
                }
            }
            result.index = included;
        }
        6 => {
            for index in 0..count {
                let outcome = unsafe { probe(context, index, 0) };
                if outcome == 1 {
                    result.index = index;
                    return result;
                }
                if outcome == 2 {
                    result.status = 1;
                    return result;
                }
            }
        }
        _ => result.status = 1,
    }
    result
}

/// # Safety
/// `context` and `probe` must remain valid while the synchronous loop
/// re-reads its native live bound and probes each binding.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_hybrid_live_bindings(
    kind: u32,
    context: *mut c_void,
    probe: Probe,
) -> VisitResult {
    let mut index = 0usize;
    loop {
        if unsafe { probe(context, index, 0) } == 0 {
            return VisitResult {
                index,
                status: if kind == 0 { 2 } else { 0 },
            };
        }
        let outcome = unsafe { probe(context, index, 1) };
        if outcome == 2 {
            return VisitResult { index, status: 1 };
        }
        if kind == 0 && outcome == 1 {
            return VisitResult { index, status: 0 };
        }
        if kind > 1 {
            return VisitResult { index, status: 1 };
        }
        index += 1;
    }
}

#[repr(C)]
pub struct QueueResult {
    id: u32,
    status: u32,
}

/// # Safety
/// `context` and `probe` must remain valid for all synchronous node reads.
/// Probe phase 1 advances only after phase 0 has supplied the current node.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_hybrid_queue(
    limit: usize,
    preferred_id: u32,
    context: *mut c_void,
    probe: unsafe extern "C" fn(*mut c_void, u32, *mut u32) -> u32,
) -> QueueResult {
    let mut last_match = 0;
    for _ in 0..limit {
        let mut id = 0;
        match unsafe { probe(context, 0, &mut id) } {
            1 => {
                if preferred_id != 0 && id == preferred_id {
                    return QueueResult { id, status: 0 };
                }
                last_match = id;
            }
            2 => break,
            3 => return QueueResult { id: 0, status: 1 },
            _ => {}
        }
        // The native successor is sampled after the current class and ID,
        // and never after an exact preferred match or invalid node.
        unsafe { probe(context, 1, core::ptr::null_mut()) };
    }
    QueueResult {
        id: if preferred_id == 0 { last_match } else { 0 },
        status: 0,
    }
}

/// # Safety
/// `context` and `copy` must remain valid for all synchronous copy calls.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_hybrid_rotate(
    from: usize,
    destination: usize,
    context: *mut c_void,
    copy: unsafe extern "C" fn(*mut c_void, usize, usize),
) {
    if from < destination {
        for index in from..destination {
            unsafe { copy(context, index, index + 1) };
        }
    } else {
        for index in (destination + 1..=from).rev() {
            unsafe { copy(context, index, index - 1) };
        }
    }
}

#[repr(C)]
pub struct Roots {
    yard_index: usize,
    construct_index: usize,
}

/// # Safety
/// `context` and `probe` must remain valid for every synchronous probe.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_hybrid_roots(
    count: usize,
    has_yard: bool,
    context: *mut c_void,
    probe: Probe,
) -> Roots {
    let mut yard = count;
    let mut construct = count;
    let mut first_legacy = count;
    let mut second_legacy = count;
    for index in 0..count {
        let bits = unsafe { probe(context, index, 0) };
        if bits & 1 != 0 {
            if yard == count {
                yard = index;
            }
            if first_legacy == count {
                first_legacy = index;
            } else if second_legacy == count
                && unsafe { probe(context, index, (first_legacy + 1) as u32) } != 0
            {
                second_legacy = index;
            }
        }
        if bits & 2 != 0 && construct == count {
            construct = index;
        }
    }
    Roots {
        yard_index: if yard != count { yard } else { first_legacy },
        construct_index: if construct != count {
            construct
        } else if has_yard {
            second_legacy
        } else {
            first_legacy
        },
    }
}

#[repr(C)]
pub struct RefitPositions {
    refit_index: usize,
    back_index: usize,
}

/// # Safety
/// `context` and `probe` must remain valid for every synchronous probe.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_hybrid_refit_positions(
    count: usize,
    context: *mut c_void,
    probe: Probe,
) -> RefitPositions {
    let mut positions = RefitPositions {
        refit_index: count,
        back_index: count,
    };
    for index in 0..count {
        let bits = unsafe { probe(context, index, 0) };
        if bits & 1 != 0 {
            positions.refit_index = index;
        }
        if bits & 2 != 0 {
            positions.back_index = index;
        }
    }
    positions
}

#[repr(C)]
pub struct SubmenuResult {
    count: usize,
    status: u32,
}

/// # Safety
/// `context` and `step` must remain valid for all synchronous page/child
/// operations. A step returns `u32::MAX` only when its native exception was
/// captured; the caller rethrows it in the original C++ try scope.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_hybrid_submenus(
    kind: u32,
    root_count: usize,
    child_count: usize,
    context: *mut c_void,
    step: unsafe extern "C" fn(*mut c_void, u32, usize, usize, usize) -> u32,
) -> SubmenuResult {
    let mut result = SubmenuResult {
        count: 0,
        status: 0,
    };
    let call = |op, root, child, output| unsafe { step(context, op, root, child, output) };
    if kind == 0 {
        for root in 0..root_count {
            let state = call(0, root, 0, 0);
            if state == u32::MAX {
                result.status = 1;
                return result;
            }
            if state == 0 {
                continue;
            }
            if state == 1 || state == 2 {
                if call(state, root, 0, 0) == u32::MAX {
                    result.status = 1;
                    return result;
                }
                continue;
            }
            for child in 0..child_count {
                let child_state = call(3, root, child, 0);
                if child_state == u32::MAX {
                    result.status = 1;
                    return result;
                }
                if child_state == 1 && call(4, root, child, 0) == u32::MAX {
                    result.status = 1;
                    return result;
                }
            }
            if call(5, root, 0, 0) == u32::MAX {
                result.status = 1;
                return result;
            }
        }
        return result;
    }
    if kind != 1 {
        result.status = 1;
        return result;
    }
    for root in 0..root_count {
        let state = call(0, root, 0, 0);
        if state == u32::MAX {
            result.status = 1;
            return result;
        }
        if state == 1 {
            for child in 0..child_count {
                let include = call(1, root, child, 0);
                if include == u32::MAX {
                    result.status = 1;
                    return result;
                }
                if include == 0 {
                    continue;
                }
                if result.count >= root_count {
                    call(4, root, child, result.count);
                    result.status = 2;
                    return result;
                }
                if call(2, root, child, result.count) == u32::MAX {
                    result.status = 1;
                    return result;
                }
                result.count += 1;
            }
        } else if state == 2 {
            if result.count >= root_count {
                call(4, root, 0, result.count);
                result.status = 2;
                return result;
            }
            if call(3, root, 0, result.count) == u32::MAX {
                result.status = 1;
                return result;
            }
            result.count += 1;
        }
    }
    result
}

#[repr(C)]
pub struct PublishResult {
    declared: [u32; 4],
    published: [usize; 4],
}

/// # Safety
/// `context` and `step` must remain valid through all synchronous entry
/// reads and native target-class resolution/placement operations.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_hybrid_publish(
    entry_count: usize,
    capacity: usize,
    context: *mut c_void,
    step: unsafe extern "C" fn(*mut c_void, u32, usize, u32) -> usize,
) -> PublishResult {
    let mut result = PublishResult {
        declared: [0; 4],
        published: [0; 4],
    };
    // Preserve the four independent first-match declaration scans.
    for method in 1..=4 {
        for index in 0..entry_count {
            if unsafe { step(context, 0, index, method) } != 0 {
                result.declared[(method - 1) as usize] = 1;
                break;
            }
        }
    }
    for index in 0..entry_count {
        let method = unsafe { step(context, 1, index, 0) };
        if !(1..=4).contains(&method) {
            continue;
        }
        // Native class resolution and missing-class logging precede the
        // configured-index capacity check, matching the original order.
        if unsafe { step(context, 2, index, 0) } == 0 {
            continue;
        }
        if unsafe { step(context, 3, index, 0) } >= capacity {
            continue;
        }
        unsafe { step(context, 4, index, method as u32) };
        result.published[method - 1] += 1;
    }
    result
}

/// # Safety
/// `context` and `probe` must remain valid across synchronous native pointer
/// checks and output writes. The probe compares pointers without encoding
/// them into a sentinel-bearing integer return value.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage10_hybrid_unique(
    candidate_count: usize,
    capacity: usize,
    context: *mut c_void,
    probe: unsafe extern "C" fn(*mut c_void, u32, usize, usize) -> u32,
) -> VisitResult {
    let mut result = VisitResult {
        index: 0,
        status: 0,
    };
    let mut accepted = [0usize; 12];
    if candidate_count > accepted.len() || capacity > accepted.len() {
        result.status = 1;
        return result;
    }
    for candidate in 0..candidate_count {
        if unsafe { probe(context, 0, candidate, 0) } == 0 {
            continue;
        }
        let mut duplicate = false;
        for existing in accepted.iter().take(result.index) {
            if unsafe { probe(context, 1, candidate, *existing) } != 0 {
                duplicate = true;
                break;
            }
        }
        if duplicate || result.index >= capacity {
            continue;
        }
        unsafe { probe(context, 2, candidate, result.index) };
        accepted[result.index] = candidate;
        result.index += 1;
    }
    result
}

#[cfg(test)]
mod tests {
    use super::*;

    struct Sequence {
        values: [u32; 5],
        calls: [usize; 5],
        advance: usize,
    }

    unsafe extern "C" fn probe(ctx: *mut c_void, index: usize, phase: u32) -> u32 {
        let seq = unsafe { &mut *ctx.cast::<Sequence>() };
        if phase == 1 {
            seq.advance += 1;
            return 0;
        }
        seq.calls[index] += 1;
        seq.values[index]
    }

    fn visit(kind: u32, count: usize, seq: &mut Sequence) -> VisitResult {
        unsafe { a2fo_rs_stage10_hybrid_visit(kind, count, seq as *mut _ as *mut c_void, probe) }
    }

    #[test]
    fn first_exact_beats_first_empty_and_reads_once() {
        let mut seq = Sequence {
            values: [5, 0, 1, 0, 0],
            calls: [0; 5],
            advance: 0,
        };
        let result = visit(4, 5, &mut seq);
        assert_eq!((result.index, result.status), (2, 0));
        assert_eq!(seq.calls, [1, 1, 1, 0, 0]);
        seq.values[2] = 0;
        let result = visit(4, 5, &mut seq);
        assert_eq!((result.index, result.status), (0, 0));
    }

    struct QueueSequence {
        ids: [u32; 5],
        tags: [u32; 5],
        cursor: usize,
        advances: usize,
    }

    unsafe extern "C" fn queue_probe(ctx: *mut c_void, phase: u32, id: *mut u32) -> u32 {
        let seq = unsafe { &mut *ctx.cast::<QueueSequence>() };
        if phase == 1 {
            seq.cursor += 1;
            seq.advances += 1;
            return 0;
        }
        let tag = seq.tags[seq.cursor];
        if tag == 1 {
            unsafe { *id = seq.ids[seq.cursor] };
        }
        tag
    }

    #[test]
    fn queue_preferred_last_fallback_and_invalid_abort() {
        let mut seq = QueueSequence {
            ids: [7, 99, u32::MAX, 0, 0],
            tags: [1, 0, 1, 2, 0],
            cursor: 0,
            advances: 0,
        };
        let result = unsafe {
            a2fo_rs_stage10_hybrid_queue(
                5,
                u32::MAX,
                &mut seq as *mut _ as *mut c_void,
                queue_probe,
            )
        };
        assert_eq!((result.id, result.status, seq.advances), (u32::MAX, 0, 2));
        seq.cursor = 0;
        seq.advances = 0;
        let result = unsafe {
            a2fo_rs_stage10_hybrid_queue(5, 0, &mut seq as *mut _ as *mut c_void, queue_probe)
        };
        assert_eq!((result.id, result.status, seq.advances), (u32::MAX, 0, 3));
        seq.tags[3] = 3;
        seq.cursor = 0;
        let result = unsafe {
            a2fo_rs_stage10_hybrid_queue(5, 0, &mut seq as *mut _ as *mut c_void, queue_probe)
        };
        assert_eq!((result.id, result.status), (0, 1));
    }

    #[test]
    fn visit_count_is_bound_even_when_context_changes() {
        let mut seq = Sequence {
            values: [0, 0, 1, 1, 1],
            calls: [0; 5],
            advance: 0,
        };
        let result = visit(0, 2, &mut seq);
        assert_eq!((result.index, result.status), (2, 0));
        assert_eq!(seq.calls, [1, 1, 0, 0, 0]);
    }

    struct LiveBindings {
        count: usize,
        after_first: usize,
        match_index: usize,
        visits: [u32; 4],
    }

    unsafe extern "C" fn live_probe(ctx: *mut c_void, index: usize, phase: u32) -> u32 {
        let state = unsafe { &mut *ctx.cast::<LiveBindings>() };
        if phase == 0 {
            return u32::from(index < state.count);
        }
        state.visits[index] += 1;
        if index == 0 {
            state.count = state.after_first;
        }
        u32::from(index == state.match_index)
    }

    #[test]
    fn live_binding_count_growth_shrink_and_first_match() {
        let mut state = LiveBindings {
            count: 1,
            after_first: 3,
            match_index: 2,
            visits: [0; 4],
        };
        let result = unsafe {
            a2fo_rs_stage10_hybrid_live_bindings(0, &mut state as *mut _ as *mut c_void, live_probe)
        };
        assert_eq!(
            (result.index, result.status, state.visits),
            (2, 0, [1, 1, 1, 0])
        );
        state = LiveBindings {
            count: 3,
            after_first: 1,
            match_index: 2,
            visits: [0; 4],
        };
        let result = unsafe {
            a2fo_rs_stage10_hybrid_live_bindings(0, &mut state as *mut _ as *mut c_void, live_probe)
        };
        assert_eq!(
            (result.index, result.status, state.visits),
            (1, 2, [1, 0, 0, 0])
        );
        state = LiveBindings {
            count: 3,
            after_first: 3,
            match_index: 0,
            visits: [0; 4],
        };
        let result = unsafe {
            a2fo_rs_stage10_hybrid_live_bindings(0, &mut state as *mut _ as *mut c_void, live_probe)
        };
        assert_eq!(
            (result.index, result.status, state.visits),
            (0, 0, [1, 0, 0, 0])
        );
    }

    unsafe extern "C" fn copy(ctx: *mut c_void, from: usize, to: usize) {
        let slots = unsafe { &mut *ctx.cast::<[u32; 5]>() };
        slots[from] = slots[to];
    }

    #[test]
    fn rotation_copies_in_direction_needed_for_native_compaction() {
        let mut slots: [u32; 5] = [0, 1, 2, 3, 4];
        unsafe { a2fo_rs_stage10_hybrid_rotate(1, 3, (&mut slots as *mut [u32; 5]).cast(), copy) };
        assert_eq!(slots, [0, 2, 3, 3, 4]);
        let mut slots: [u32; 5] = [0, 1, 2, 3, 4];
        unsafe { a2fo_rs_stage10_hybrid_rotate(3, 1, (&mut slots as *mut [u32; 5]).cast(), copy) };
        assert_eq!(slots, [0, 1, 1, 2, 4]);
    }

    unsafe extern "C" fn roots_probe(ctx: *mut c_void, index: usize, phase: u32) -> u32 {
        let (bits, identity) = unsafe { &*ctx.cast::<([u32; 5], [u32; 5])>() };
        if phase != 0 {
            return u32::from(identity[index] != identity[(phase - 1) as usize]);
        }
        bits[index]
    }

    #[test]
    fn root_binding_prefers_construct_then_distinct_legacy_fallback() {
        let mut rows = ([1, 1, 2, 0, 0], [10, 10, 20, 30, 40]);
        let roots = unsafe {
            a2fo_rs_stage10_hybrid_roots(5, true, &mut rows as *mut _ as *mut c_void, roots_probe)
        };
        assert_eq!((roots.yard_index, roots.construct_index), (0, 2));
        rows.0[2] = 1;
        let roots = unsafe {
            a2fo_rs_stage10_hybrid_roots(5, true, &mut rows as *mut _ as *mut c_void, roots_probe)
        };
        assert_eq!((roots.yard_index, roots.construct_index), (0, 2));
    }

    #[test]
    fn refit_positions_choose_last_matching_slots() {
        let mut seq = Sequence {
            values: [1, 2, 1, 2, 0],
            calls: [0; 5],
            advance: 0,
        };
        let positions = unsafe {
            a2fo_rs_stage10_hybrid_refit_positions(5, &mut seq as *mut _ as *mut c_void, probe)
        };
        assert_eq!((positions.refit_index, positions.back_index), (2, 3));
        assert_eq!(seq.calls, [1; 5]);
    }

    struct SubmenuSequence {
        roots: [u32; 3],
        children: [[u32; 3]; 3],
        output: [u32; 3],
        overflow_logs: u32,
    }

    unsafe extern "C" fn submenu_step(
        ctx: *mut c_void,
        op: u32,
        root: usize,
        child: usize,
        output: usize,
    ) -> u32 {
        let seq = unsafe { &mut *ctx.cast::<SubmenuSequence>() };
        match op {
            0 => seq.roots[root],
            1 => seq.children[root][child],
            2 => {
                seq.output[output] = seq.children[root][child] + 10;
                0
            }
            3 => {
                seq.output[output] = root as u32 + 20;
                0
            }
            4 => {
                seq.overflow_logs += 1;
                0
            }
            _ => 0,
        }
    }

    #[test]
    fn submenu_flatten_preserves_order_and_rejects_overflow() {
        let mut seq = SubmenuSequence {
            roots: [1, 2, 0],
            children: [[1, 0, 1], [0; 3], [0; 3]],
            output: [0; 3],
            overflow_logs: 0,
        };
        let result = unsafe {
            a2fo_rs_stage10_hybrid_submenus(
                1,
                3,
                3,
                &mut seq as *mut _ as *mut c_void,
                submenu_step,
            )
        };
        assert_eq!(
            (result.count, result.status, seq.output),
            (3, 0, [11, 11, 21])
        );
        seq.children[0][1] = 1;
        let result = unsafe {
            a2fo_rs_stage10_hybrid_submenus(
                1,
                3,
                3,
                &mut seq as *mut _ as *mut c_void,
                submenu_step,
            )
        };
        assert_eq!((result.count, result.status, seq.overflow_logs), (3, 2, 1));
    }

    struct PublishSequence {
        methods: [usize; 5],
        indices: [usize; 5],
        found: [usize; 5],
        find_calls: [u32; 5],
        placed: [u32; 4],
    }

    unsafe extern "C" fn publish_step(
        ctx: *mut c_void,
        op: u32,
        index: usize,
        method: u32,
    ) -> usize {
        let seq = unsafe { &mut *ctx.cast::<PublishSequence>() };
        match op {
            0 => usize::from(seq.methods[index] == method as usize),
            1 => seq.methods[index],
            2 => {
                seq.find_calls[index] += 1;
                seq.found[index]
            }
            3 => seq.indices[index],
            4 => {
                seq.placed[(method - 1) as usize] += 1;
                0
            }
            _ => 0,
        }
    }

    #[test]
    fn publish_declared_first_match_and_find_before_index_check() {
        let mut seq = PublishSequence {
            methods: [1, 2, 3, 4, 1],
            indices: [0, 99, 1, 2, usize::MAX],
            found: [1, 1, 0, 1, 1],
            find_calls: [0; 5],
            placed: [0; 4],
        };
        let result = unsafe {
            a2fo_rs_stage10_hybrid_publish(5, 3, &mut seq as *mut _ as *mut c_void, publish_step)
        };
        assert_eq!(result.declared, [1; 4]);
        assert_eq!(result.published, [1, 0, 0, 1]);
        assert_eq!(seq.find_calls, [1; 5]);
        assert_eq!(seq.placed, [1, 0, 0, 1]);
    }

    struct UniqueSequence {
        identities: [usize; 5],
        stored: [usize; 5],
    }

    unsafe extern "C" fn unique_probe(
        ctx: *mut c_void,
        phase: u32,
        candidate: usize,
        other: usize,
    ) -> u32 {
        let seq = unsafe { &mut *ctx.cast::<UniqueSequence>() };
        match phase {
            0 => u32::from(seq.identities[candidate] != 0),
            1 => u32::from(seq.identities[candidate] == seq.identities[other]),
            2 => {
                seq.stored[other] = seq.identities[candidate];
                0
            }
            _ => 0,
        }
    }

    #[test]
    fn unique_interface_admission_keeps_first_pointer_once() {
        let mut seq = UniqueSequence {
            identities: [usize::MAX, 0, usize::MAX, 17, 17],
            stored: [0; 5],
        };
        let result = unsafe {
            a2fo_rs_stage10_hybrid_unique(5, 5, &mut seq as *mut _ as *mut c_void, unique_probe)
        };
        assert_eq!((result.index, result.status), (2, 0));
        assert_eq!(&seq.stored[..2], &[usize::MAX, 17]);
    }
}
