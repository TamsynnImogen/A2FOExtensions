use super::*;
use std::collections::HashMap;

#[derive(Default)]
struct Mock {
    core: usize,
    module: usize,
    symbols: HashMap<(usize, Vec<u8>), usize>,
    filenames: HashMap<usize, (Vec<u8>, u32)>,
    loads: Vec<usize>,
    ready: bool,
    events: Vec<String>,
}

impl Mock {
    fn symbol_value(&mut self, module: usize, name: &[u8], value: usize) {
        self.symbols.insert((module, name.to_vec()), value);
    }
    fn callbacks(&mut self, module: usize, extend: usize, init: usize, unload: usize) {
        self.symbol_value(module, EXTEND, extend);
        self.symbol_value(module, INIT, init);
        self.symbol_value(module, UNLOAD, unload);
    }
    fn filename_value(&mut self, module: usize, path: &str) {
        self.filenames
            .insert(module, (path.as_bytes().to_vec(), path.len() as u32));
    }
}

fn label(bytes: &[u8]) -> String {
    String::from_utf8_lossy(bytes.split(|&byte| byte == 0).next().unwrap()).into_owned()
}

impl Host for Mock {
    fn module(&mut self, name: &[u8]) -> usize {
        self.events.push(format!("module:{}", label(name)));
        if name == CORE {
            self.core
        } else if name == MODULE {
            self.module
        } else {
            panic!("unexpected module")
        }
    }
    fn symbol(&mut self, module: usize, name: &[u8]) -> usize {
        self.events.push(format!("symbol:{module}:{}", label(name)));
        *self.symbols.get(&(module, name.to_vec())).unwrap_or(&0)
    }
    fn filename(&mut self, module: usize, path: &mut [u8; CAPACITY]) -> u32 {
        self.events.push(format!("filename:{module}"));
        let (bytes, count) = self
            .filenames
            .get(&module)
            .expect("filename fixture missing");
        assert!(bytes.len() <= CAPACITY);
        path[..bytes.len()].copy_from_slice(bytes);
        *count
    }
    fn load(&mut self, path: &[u8; CAPACITY]) -> usize {
        self.events.push(format!("load:{}", label(path)));
        if self.loads.is_empty() {
            0
        } else {
            self.loads.remove(0)
        }
    }
    fn ready(&mut self, function: usize) -> bool {
        self.events.push(format!("ready:{function}"));
        self.ready
    }
    fn debug(&mut self, text: &[u8]) {
        self.events.push(format!("debug:{}", label(text)));
    }
}

fn callbacks(state: &State) -> (usize, usize, usize) {
    (
        state.extend.load(Ordering::Acquire),
        state.init.load(Ordering::Acquire),
        state.unloaded.load(Ordering::Acquire),
    )
}

#[test]
fn builtin_publishes_all_callbacks_and_cache_skips_host() {
    let state = State::new();
    let mut host = Mock {
        core: 10,
        ready: true,
        ..Default::default()
    };
    host.symbol_value(10, b"A2FOBuiltinModule_StartupReady\0", 11);
    host.symbol_value(10, b"A2FOBuiltinMissions_Init\0", 12);
    host.callbacks(10, 21, 22, 23);
    assert!(state.resolve(&mut host));
    assert_eq!(callbacks(&state), (21, 22, 23));
    assert_eq!(
        host.events,
        [
            "module:A2FOExtensions.dll",
            "symbol:10:A2FOBuiltinModule_StartupReady",
            "symbol:10:A2FOBuiltinMissions_Init",
            "symbol:10:A2FOMissions_ScriptExtendInterface",
            "symbol:10:A2FOMissions_ScriptInit",
            "symbol:10:A2FOMissions_MarkMissionUnloaded",
            "ready:11",
        ]
    );
    host.events.clear();
    assert!(state.resolve(&mut host));
    assert!(host.events.is_empty());
}

#[test]
fn authoritative_builtin_not_ready_blocks_standalone_and_retries() {
    let state = State::new();
    let mut host = Mock {
        core: 10,
        module: 20,
        ..Default::default()
    };
    host.symbol_value(10, b"A2FOBuiltinModule_StartupReady\0", 11);
    host.symbol_value(10, b"A2FOBuiltinMissions_Init\0", 12);
    host.callbacks(10, 21, 22, 23);
    host.callbacks(20, 31, 32, 33);
    for _ in 0..2 {
        host.events.clear();
        assert!(!state.resolve(&mut host));
        assert_eq!(callbacks(&state), (0, 0, 0));
        assert_eq!(
            host.events.last().unwrap(),
            "debug:built-in Missions unavailable"
        );
        assert!(
            !host
                .events
                .iter()
                .any(|event| event == "module:A2FOMissions.dll")
        );
        assert_eq!(host.events[6], "ready:11");
    }
    host.ready = true;
    assert!(state.resolve(&mut host));
    assert_eq!(callbacks(&state), (21, 22, 23));
}

#[test]
fn authoritative_builtin_missing_export_blocks_standalone_without_ready_call() {
    let state = State::new();
    let mut host = Mock {
        core: 10,
        module: 20,
        ready: true,
        ..Default::default()
    };
    host.symbol_value(10, b"A2FOBuiltinModule_StartupReady\0", 11);
    host.symbol_value(10, b"A2FOBuiltinMissions_Init\0", 12);
    host.callbacks(10, 21, 0, 23);
    assert!(!state.resolve(&mut host));
    assert_eq!(callbacks(&state), (0, 0, 0));
    assert_eq!(
        host.events.last().unwrap(),
        "debug:built-in Missions unavailable"
    );
    assert!(
        !host
            .events
            .iter()
            .any(|event| event.starts_with("ready:") || event == "module:A2FOMissions.dll")
    );
}

#[test]
fn old_core_uses_already_loaded_standalone_and_optional_unload() {
    let state = State::new();
    let mut host = Mock {
        core: 10,
        module: 20,
        ..Default::default()
    };
    host.callbacks(20, 31, 32, 0);
    assert!(state.resolve(&mut host));
    assert_eq!(callbacks(&state), (31, 32, 0));
    assert_eq!(
        host.events,
        [
            "module:A2FOExtensions.dll",
            "symbol:10:A2FOBuiltinModule_StartupReady",
            "symbol:10:A2FOBuiltinMissions_Init",
            "module:A2FOMissions.dll",
            "symbol:20:A2FOMissions_ScriptExtendInterface",
            "symbol:20:A2FOMissions_ScriptInit",
            "symbol:20:A2FOMissions_MarkMissionUnloaded",
        ]
    );
}

#[test]
fn old_core_tries_core_root_before_shim_parent() {
    let state = State::new();
    state.instance.store(40, Ordering::Release);
    let mut host = Mock {
        core: 10,
        loads: vec![0, 30],
        ..Default::default()
    };
    host.filename_value(10, r"C:\FleetOps\A2FOExtensions.dll");
    host.filename_value(40, r"D:\Mod\missions\mmm_loader.dll");
    host.callbacks(30, 51, 52, 53);
    assert!(state.resolve(&mut host));
    assert_eq!(callbacks(&state), (51, 52, 53));
    assert_eq!(
        host.events,
        [
            "module:A2FOExtensions.dll",
            "symbol:10:A2FOBuiltinModule_StartupReady",
            "symbol:10:A2FOBuiltinMissions_Init",
            "module:A2FOMissions.dll",
            "module:A2FOExtensions.dll",
            "filename:10",
            r"load:C:\FleetOps\modules\A2FOMissions.dll",
            "filename:40",
            r"load:D:\Mod\modules\A2FOMissions.dll",
            "symbol:30:A2FOMissions_ScriptExtendInterface",
            "symbol:30:A2FOMissions_ScriptInit",
            "symbol:30:A2FOMissions_MarkMissionUnloaded",
        ]
    );
}

#[test]
fn no_core_uses_shim_parent_and_reports_missing_module() {
    let state = State::new();
    state.instance.store(40, Ordering::Release);
    let mut host = Mock::default();
    host.filename_value(40, r"D:\Mod\missions\mmm_loader.dll");
    assert!(!state.resolve(&mut host));
    assert_eq!(callbacks(&state), (0, 0, 0));
    assert_eq!(
        host.events,
        [
            "module:A2FOExtensions.dll",
            "module:A2FOMissions.dll",
            "module:A2FOExtensions.dll",
            "filename:40",
            r"load:D:\Mod\modules\A2FOMissions.dll",
            "debug:unable to locate modules/A2FOMissions.dll",
        ]
    );
}

#[test]
fn partial_standalone_publication_retries_and_replaces_optional_callback() {
    let state = State::new();
    let mut host = Mock {
        module: 20,
        ..Default::default()
    };
    host.callbacks(20, 31, 0, 33);
    assert!(!state.resolve(&mut host));
    assert_eq!(callbacks(&state), (31, 0, 33));
    assert_eq!(
        host.events.last().unwrap(),
        "debug:A2FOMissions.dll is missing its MMM compatibility exports"
    );
    host.events.clear();
    host.callbacks(20, 41, 42, 0);
    assert!(state.resolve(&mut host));
    assert_eq!(callbacks(&state), (41, 42, 0));
    assert_eq!(host.events[0], "module:A2FOExtensions.dll");
    assert_eq!(host.events[1], "module:A2FOMissions.dll");
    host.events.clear();
    assert!(state.resolve(&mut host));
    assert!(host.events.is_empty());
}

#[test]
fn filename_rejections_and_capacity_do_not_load() {
    for (path, count) in [
        (r"C:\FleetOps\A2FOExtensions.dll".to_string(), 0),
        (r"C:\FleetOps\A2FOExtensions.dll".to_string(), 260),
        ("A2FOExtensions.dll".to_string(), 18),
        (format!("C:\\{}\\x", "a".repeat(247)), 253),
    ] {
        let state = State::new();
        let mut host = Mock {
            core: 10,
            ..Default::default()
        };
        host.filenames.insert(10, (path.into_bytes(), count));
        assert!(!state.resolve(&mut host));
        assert!(!host.events.iter().any(|event| event.starts_with("load:")));
        assert_eq!(
            host.events.last().unwrap(),
            "debug:unable to locate modules/A2FOMissions.dll"
        );
    }
}

#[test]
fn mixed_separators_prefer_last_backslash_and_shim_needs_parent() {
    let state = State::new();
    let mut host = Mock {
        core: 10,
        loads: vec![20],
        ..Default::default()
    };
    host.filename_value(10, r"C:\FleetOps/odd\A2FOExtensions.dll");
    host.callbacks(20, 31, 32, 0);
    assert!(state.resolve(&mut host));
    assert!(
        host.events
            .contains(&r"load:C:\FleetOps/odd\modules\A2FOMissions.dll".to_string())
    );

    let state = State::new();
    state.instance.store(40, Ordering::Release);
    let mut host = Mock::default();
    host.filename_value(40, r"missions\mmm_loader.dll");
    assert!(!state.resolve(&mut host));
    assert!(!host.events.iter().any(|event| event.starts_with("load:")));
}

#[test]
fn filename_count_259_and_slash_only_root_still_load() {
    let state = State::new();
    let mut host = Mock {
        core: 10,
        loads: vec![20],
        ..Default::default()
    };
    let path = format!("C:/FleetOps/{}", "x".repeat(247));
    assert_eq!(path.len(), 259);
    host.filenames.insert(10, (path.into_bytes(), 259));
    host.callbacks(20, 31, 32, 0);
    assert!(state.resolve(&mut host));
    assert_eq!(callbacks(&state), (31, 32, 0));
    assert!(
        host.events
            .contains(&r"load:C:/FleetOps/modules\A2FOMissions.dll".to_string())
    );
}
