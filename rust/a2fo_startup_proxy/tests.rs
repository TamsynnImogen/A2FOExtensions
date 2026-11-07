use super::*;
#[derive(Default)]
struct Mock {
    calls: Vec<String>,
    fail_fleet: bool,
    fail_core: bool,
    fail_init: bool,
    fail_early: bool,
    incompatible: bool,
    sleeps: usize,
}
impl Host for Mock {
    fn compatible(&mut self, module: usize) -> bool {
        self.calls.push("compatible".into());
        module == 13 && !self.incompatible
    }
    fn early_attach(&mut self, module: usize) -> bool {
        assert_eq!(module, 13);
        self.calls.push("early_attach".into());
        !self.fail_early
    }
    fn load_relative(&mut self, instance: usize, parent: bool, name: &str) -> usize {
        assert_eq!(instance, 7);
        self.calls.push(format!("load:{parent}:{name}"));
        if parent {
            if self.fail_fleet {
                0
            } else {
                11
            }
        } else if self.fail_core {
            0
        } else {
            13
        }
    }
    fn initialize(&mut self, module: usize) -> bool {
        assert_eq!(module, 13);
        self.calls.push("initialize".into());
        !self.fail_init
    }
    fn sleep(&mut self) {
        self.sleeps += 1;
    }
    fn log(&mut self, _message: &'static [u8]) {}
}
#[test]
fn early_attach_orders_fleet_then_core_without_initializing() {
    let state = State::new();
    let mut host = Mock::default();
    assert!(state.attach(7, &mut host));
    assert_eq!(
        host.calls,
        [
            "load:true:FleetOpsHook.dll",
            "load:false:A2FOExtensions.dll",
            "compatible",
            "early_attach",
        ]
    );
    assert!(state.ensure_ready(&mut host));
    assert!(state.ensure_ready(&mut host));
    assert_eq!(
        host.calls,
        [
            "load:true:FleetOpsHook.dll",
            "load:false:A2FOExtensions.dll",
            "compatible",
            "early_attach",
            "initialize",
        ]
    );
}
#[test]
fn each_attach_failure_stops_and_remains_failed() {
    for fleet in [true, false] {
        let state = State::new();
        let mut host = Mock {
            fail_fleet: fleet,
            fail_core: !fleet,
            ..Mock::default()
        };
        assert!(!state.attach(7, &mut host));
        assert!(!state.ensure_ready(&mut host));
        assert_eq!(host.calls.len(), if fleet { 1 } else { 2 });
    }
}
#[test]
fn initialization_failure_is_not_published_or_retried() {
    let state = State::new();
    let mut host = Mock {
        fail_init: true,
        ..Mock::default()
    };
    assert!(state.attach(7, &mut host));
    assert!(!state.ensure_ready(&mut host));
    assert!(!state.ensure_ready(&mut host));
    assert_eq!(host.calls.len(), 5);
}
#[test]
fn old_core_without_contained_api_is_refused() {
    let state = State::new();
    let mut host = Mock {
        incompatible: true,
        ..Mock::default()
    };
    assert!(!state.attach(7, &mut host));
    assert!(!state.ensure_ready(&mut host));
    assert_eq!(
        host.calls,
        [
            "load:true:FleetOpsHook.dll",
            "load:false:A2FOExtensions.dll",
            "compatible",
        ]
    );
}
#[test]
fn refused_early_attach_never_publishes_core_or_initializes() {
    let state = State::new();
    let mut host = Mock {
        fail_early: true,
        ..Mock::default()
    };
    assert!(!state.attach(7, &mut host));
    assert_eq!(state.phase.load(Ordering::Acquire), FAILED);
    assert_eq!(state.core.load(Ordering::Acquire), 0);
    assert!(!state.ensure_ready(&mut host));
    assert_eq!(
        host.calls,
        [
            "load:true:FleetOpsHook.dll",
            "load:false:A2FOExtensions.dll",
            "compatible",
            "early_attach",
        ]
    );
}

#[test]
fn legacy_host_uses_default_optional_early_attach() {
    struct LegacyHost {
        initialized: bool,
    }
    impl Host for LegacyHost {
        fn load_relative(&mut self, _: usize, parent: bool, _: &str) -> usize {
            if parent {
                11
            } else {
                13
            }
        }
        fn compatible(&mut self, module: usize) -> bool {
            module == 13
        }
        fn initialize(&mut self, module: usize) -> bool {
            assert_eq!(module, 13);
            self.initialized = true;
            true
        }
        fn sleep(&mut self) {}
        fn log(&mut self, _: &'static [u8]) {}
    }
    let state = State::new();
    let mut host = LegacyHost { initialized: false };
    assert!(state.attach(7, &mut host));
    assert!(!host.initialized);
    assert!(state.ensure_ready(&mut host));
    assert!(host.initialized);
}
#[test]
fn concurrent_or_reentrant_initialization_has_a_bounded_wait() {
    let state = State::new();
    let mut host = Mock::default();
    assert!(!state.ensure_ready(&mut host));
    state.phase.store(INITIALIZING, Ordering::Release);
    assert!(!state.ensure_ready(&mut host));
    assert_eq!(host.sleeps, 5000);
    assert_eq!(state.phase.load(Ordering::Acquire), INITIALIZING);
}
fn rewritten(input: &str, parent: bool, name: &str) -> Option<String> {
    let mut path = [0u16; 256];
    let units: Vec<_> = input.encode_utf16().collect();
    path[..units.len()].copy_from_slice(&units);
    if !relative_name(&mut path, units.len(), parent, name) {
        return None;
    }
    Some(String::from_utf16(&path[..path.iter().position(|&c| c == 0).unwrap()]).unwrap())
}
#[test]
fn paths_are_module_relative_and_preserve_unicode() {
    let path = "C:\\Fleet Öps\\Data\\Win2kDisableTaskSwitch.dll";
    assert_eq!(
        rewritten(path, true, "FleetOpsHook.dll").as_deref(),
        Some("C:\\Fleet Öps\\FleetOpsHook.dll")
    );
    assert_eq!(
        rewritten(path, false, "A2FOExtensions.dll").as_deref(),
        Some("C:\\Fleet Öps\\Data\\A2FOExtensions.dll")
    );
    assert_eq!(
        rewritten(
            "\\\\server\\game\\Data\\proxy.dll",
            true,
            "FleetOpsHook.dll"
        )
        .as_deref(),
        Some("\\\\server\\game\\FleetOpsHook.dll")
    );
    assert_eq!(
        rewritten("C:\\Data\\proxy.dll", true, "FleetOpsHook.dll").as_deref(),
        Some("C:\\FleetOpsHook.dll")
    );
}
#[test]
fn paths_reject_incomplete_root_truncation_and_invalid_components() {
    for (path, parent, name) in [
        ("proxy.dll", false, "Core.dll"),
        ("C:\\proxy.dll", true, "FleetOpsHook.dll"),
        ("/proxy.dll", false, "Core.dll"),
        ("C:\\Data\\proxy.dll", false, "../escape.dll"),
        ("C:\\Data\\proxy.dll", false, "bad\0.dll"),
    ] {
        assert_eq!(rewritten(path, parent, name), None);
    }
    let mut short = [0u16; 8];
    assert!(!relative_name(&mut short, 8, false, "Core.dll"));
    let mut path: Vec<_> = "C:\\p.dll".encode_utf16().collect();
    path.push(0);
    assert!(!relative_name(&mut path, 8, false, "TooLong.dll"));
}
