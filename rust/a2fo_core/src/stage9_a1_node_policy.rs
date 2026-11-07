#[cfg(test)]
extern crate std;

use core::ffi::c_void;

/// Synchronous native node operation. Operation 0 returns first child,
/// operation 1 tests a node name against the request in `context`, and
/// operation 2 returns the next sibling.
pub type NodeStep = unsafe extern "C" fn(*mut c_void, u32, usize) -> usize;

const MAX_DIRECT_CHILDREN: usize = 256;
const MAX_DEPTH: usize = 32;

/// Search native direct children in order. `parent` and `requested` are
/// validated by the native caller; name extraction/comparison and link reads
/// remain in `step`.
///
/// # Safety
/// `context` and `step` must satisfy the callback contract and remain valid
/// for this synchronous call. The callback must not unwind across the ABI.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_node_find_direct_child(
    context: *mut c_void,
    parent: usize,
    requested: usize,
    step: Option<NodeStep>,
) -> usize {
    let Some(step) = step else { return 0 };
    if parent == 0 || requested == 0 {
        return 0;
    }
    let mut child = unsafe { step(context, 0, parent) };
    for _ in 0..MAX_DIRECT_CHILDREN {
        if child == 0 {
            return 0;
        }
        if unsafe { step(context, 1, child) } != 0 {
            return child;
        }
        // Preserve the native order: read NEXT only after a failed name test,
        // including after the final allowed child test.
        child = unsafe { step(context, 2, child) };
    }
    0
}

unsafe fn find_node_inner(
    context: *mut c_void,
    node: usize,
    requested: usize,
    depth: usize,
    remaining: &mut usize,
    step: NodeStep,
) -> usize {
    if node == 0 || requested == 0 || *remaining == 0 || depth > MAX_DEPTH {
        return 0;
    }
    *remaining -= 1;
    if unsafe { step(context, 1, node) } != 0 {
        return node;
    }

    let mut child = unsafe { step(context, 0, node) };
    while child != 0 && *remaining != 0 {
        // The native walk captures NEXT before descending. The name callback
        // may mutate the link, so retain this value for the parent's advance.
        let next = unsafe { step(context, 2, child) };
        let found =
            unsafe { find_node_inner(context, child, requested, depth + 1, remaining, step) };
        if found != 0 {
            return found;
        }
        child = next;
    }
    0
}

/// Depth-first bounded name search with native link/name callbacks and a
/// private Rust budget. `remaining` is copied before callbacks and copied back
/// only after traversal has returned.
///
/// # Safety
/// `remaining` must be aligned, readable and writable for one `usize`, and
/// must not overlap the callback context or any native node memory. `context`
/// and `step` must remain valid for the synchronous call. The callback must
/// not unwind across the ABI and must not access `remaining`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_a1_node_find_named_bounded(
    context: *mut c_void,
    node: usize,
    requested: usize,
    depth: usize,
    remaining: *mut usize,
    step: Option<NodeStep>,
) -> usize {
    if node == 0 || requested == 0 || remaining.is_null() {
        return 0;
    }
    let Some(step) = step else { return 0 };
    // SAFETY: The caller guarantees the live, aligned and disjoint budget span.
    let mut budget = unsafe { remaining.read() };
    let found = unsafe { find_node_inner(context, node, requested, depth, &mut budget, step) };
    // SAFETY: As above; no callback is active and the private budget is final.
    unsafe { remaining.write(budget) };
    found
}

#[unsafe(no_mangle)]
pub extern "C" fn a2fo_rs_stage9_a1_node_hidden_flags(flags: u32, hidden: u8, mask: u32) -> u32 {
    if hidden != 0 {
        flags | mask
    } else {
        flags & !mask
    }
}

#[cfg(test)]
mod tests {
    use super::{
        NodeStep, a2fo_rs_stage9_a1_node_find_direct_child as direct,
        a2fo_rs_stage9_a1_node_find_named_bounded as bounded,
        a2fo_rs_stage9_a1_node_hidden_flags as hidden_flags,
    };
    use core::ffi::c_void;
    use std::{vec, vec::Vec};

    #[derive(Clone, Copy, Default)]
    struct Node {
        first: usize,
        next: usize,
        matches: bool,
    }

    struct Trace {
        nodes: Vec<Node>,
        calls: Vec<(u32, usize)>,
        mutate_next_on_name: Option<(usize, usize)>,
    }

    unsafe extern "C" fn step(opaque: *mut c_void, op: u32, node: usize) -> usize {
        let state = unsafe { &mut *opaque.cast::<Trace>() };
        state.calls.push((op, node));
        match op {
            0 => state.nodes[node].first,
            1 => {
                if let Some((target, replacement)) = state.mutate_next_on_name
                    && target == node
                {
                    state.nodes[node].next = replacement;
                    state.mutate_next_on_name = None;
                }
                usize::from(state.nodes[node].matches)
            }
            2 => state.nodes[node].next,
            _ => 0,
        }
    }

    fn context(state: &mut Trace) -> *mut c_void {
        (state as *mut Trace).cast()
    }
    fn callback() -> Option<NodeStep> {
        Some(step)
    }
    fn trace(nodes: Vec<Node>) -> Trace {
        Trace {
            nodes,
            calls: Vec::new(),
            mutate_next_on_name: None,
        }
    }

    #[test]
    fn direct_child_checks_name_before_reading_next_and_stops_at_match() {
        let mut state = trace(vec![
            Node::default(),
            Node {
                first: 2,
                ..Node::default()
            },
            Node {
                next: 3,
                ..Node::default()
            },
            Node::default(),
            Node {
                matches: true,
                ..Node::default()
            },
        ]);
        state.mutate_next_on_name = Some((2, 4));
        let found = unsafe { direct(context(&mut state), 1, 99, callback()) };
        assert_eq!(found, 4);
        assert_eq!(state.calls, [(0, 1), (1, 2), (2, 2), (1, 4)]);
    }

    #[test]
    fn direct_child_tests_at_most_256_and_reads_final_next_after_miss() {
        let mut nodes = vec![Node::default(); 259];
        nodes[1].first = 2;
        for (index, node) in nodes.iter_mut().enumerate().take(258).skip(2) {
            node.next = index + 1;
        }
        nodes[258].matches = true;
        let mut state = trace(nodes);
        let found = unsafe { direct(context(&mut state), 1, 99, callback()) };
        assert_eq!(found, 0);
        assert_eq!(state.calls.len(), 1 + 256 * 2);
        assert_eq!(state.calls.first(), Some(&(0, 1)));
        assert_eq!(state.calls[state.calls.len() - 2], (1, 257));
        assert_eq!(state.calls.last(), Some(&(2, 257)));
    }

    #[test]
    fn depth_first_reads_sibling_before_descending_and_keeps_snapshot() {
        let mut state = trace(vec![
            Node::default(),
            Node {
                first: 2,
                ..Node::default()
            },
            Node {
                next: 3,
                ..Node::default()
            },
            Node {
                matches: true,
                next: 4,
                ..Node::default()
            },
            Node {
                matches: true,
                ..Node::default()
            },
        ]);
        state.mutate_next_on_name = Some((2, 4));
        let mut remaining = 8;
        let found = unsafe { bounded(context(&mut state), 1, 99, 0, &mut remaining, callback()) };
        assert_eq!(found, 3);
        assert_eq!(remaining, 5);
        assert_eq!(
            state.calls,
            [(1, 1), (0, 1), (2, 2), (1, 2), (0, 2), (2, 3), (1, 3)]
        );
    }

    #[test]
    fn zero_budget_skips_all_callbacks_and_depth_33_is_not_counted_or_named() {
        let mut state = trace(vec![Node::default(); 40]);
        let mut zero = 0;
        assert_eq!(
            unsafe { bounded(context(&mut state), 1, 99, 0, &mut zero, callback()) },
            0
        );
        assert!(state.calls.is_empty());
        assert_eq!(zero, 0);
        let mut positive = 5;
        assert_eq!(
            unsafe { bounded(context(&mut state), 0, 99, 0, &mut positive, callback()) },
            0
        );
        assert_eq!(
            unsafe { bounded(context(&mut state), 1, 0, 0, &mut positive, callback()) },
            0
        );
        assert_eq!(
            unsafe { bounded(context(&mut state), 1, 99, 33, &mut positive, callback()) },
            0
        );
        assert_eq!(positive, 5);
        assert!(state.calls.is_empty());

        state.nodes[1].first = 2;
        for index in 2..34 {
            state.nodes[index].first = index + 1;
        }
        let mut remaining = 40;
        assert_eq!(
            unsafe { bounded(context(&mut state), 1, 99, 0, &mut remaining, callback()) },
            0
        );
        assert_eq!(remaining, 7); // depths 0 through 32 consume; depth 33 does not.
        assert!(!state.calls.contains(&(1, 34)));
        assert!(state.calls.contains(&(2, 34))); // parent's NEXT read precedes depth-33 return.
    }

    #[test]
    fn root_match_consumes_budget_before_name_check() {
        let mut state = trace(vec![
            Node::default(),
            Node {
                matches: true,
                ..Node::default()
            },
        ]);
        let mut remaining = 1;
        assert_eq!(
            unsafe { bounded(context(&mut state), 1, 99, 0, &mut remaining, callback()) },
            1
        );
        assert_eq!(remaining, 0);
        assert_eq!(state.calls, [(1, 1)]);
    }

    #[test]
    fn hidden_flag_policy_changes_only_the_requested_mask() {
        assert_eq!(hidden_flags(0b1010, 1, 0b0100), 0b1110);
        assert_eq!(hidden_flags(0b1110, 0, 0b0100), 0b1010);
        assert_eq!(hidden_flags(0b1010, 1, 0), 0b1010);
    }
}
