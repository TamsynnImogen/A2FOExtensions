//! Remaining module-menu control. Native adapters own COM interface lifetimes,
//! Win32 enumeration/control records, strings and original exception scopes.
use core::ffi::c_void;
type Step = unsafe extern "C" fn(*mut c_void, u32, u64) -> u64;
struct Flow {
    step: Step,
    context: *mut c_void,
    failed: *const u8,
}
impl Flow {
    fn call(&self, op: u32) -> Option<u64> {
        self.arg(op, 0)
    }
    fn arg(&self, op: u32, arg: u64) -> Option<u64> {
        let value = unsafe { (self.step)(self.context, op, arg) };
        (unsafe { self.failed.read() } == 0).then_some(value)
    }
    fn run(&self, kind: u32) -> Option<u64> {
        match kind {
            0 => {
                if self.call(0)? == 0 || self.call(1)? == 0 {
                    return Some(0);
                }
                if self.call(2)? == 0 || self.call(3)? == 0 {
                    self.call(4)?;
                    return Some(0);
                }
                Some(1)
            }
            4 => {
                if self.call(6)? != 0
                    || self.call(7)? == 0
                    || self.call(9)? == 0
                    || self.call(10)? == 0
                {
                    self.call(8)?;
                    self.call(4)?;
                    return Some(0);
                }
                self.call(11)?;
                let changed = self.call(12)? != 0;
                let saved = !changed || (self.call(13)? != 0 && self.call(14)? != 0);
                self.call(8)?;
                self.call(4)?;
                Some(u64::from(
                    saved && (changed || self.call(15)? != 0 || self.call(16)? != 0),
                ))
            }
            1 => {
                if self.call(0)? != 0
                    || self.call(1)? != 0
                    || (self.call(2)? == 0 && self.call(3)? == 0)
                {
                    return Some(1);
                }
                let shell = self.call(4)? != 0;
                let ole = self.call(5)? != 0;
                if !shell || !ole {
                    if shell {
                        self.call(8)?;
                    }
                    if ole {
                        self.call(9)?;
                    }
                    return Some(0);
                }
                for op in 10..=13 {
                    self.call(op)?;
                }
                let mut valid = true;
                for op in 14..=17 {
                    if self.call(op)? == 0 {
                        valid = false;
                        break;
                    }
                }
                if !valid || self.call(18)? == 0 {
                    self.call(8)?;
                    self.call(9)?;
                    return Some(0);
                }
                let uninitialize = self.call(19)? != 0;
                if !uninitialize && self.call(20)? == 0 {
                    self.call(8)?;
                    self.call(9)?;
                    return Some(0);
                }
                self.call(21)?;
                let search = self.call(22)? != 0;
                let mut updated = false;
                if search {
                    let start = self.call(23)?;
                    let earliest = start.saturating_sub(50_000_000);
                    loop {
                        if self.call(24)? == 0 && self.call(25)? >= earliest {
                            self.call(26)?;
                            let accepted = self.call(27)? != 0;
                            self.call(28)?;
                            if accepted {
                                updated = true;
                                break;
                            }
                        }
                        if self.call(29)? == 0 {
                            break;
                        }
                    }
                    self.call(30)?;
                }
                if uninitialize {
                    self.call(31)?;
                }
                self.call(8)?;
                self.call(9)?;
                Some(u64::from(updated))
            }
            2 => {
                for op in 0..=3 {
                    self.call(op)?;
                }
                let mut index = 0;
                while index < self.call(4)? {
                    for op in 5..=10 {
                        self.arg(op, index)?;
                    }
                    index += 1;
                }
                self.call(11)?;
                Some(0)
            }
            3 => {
                let needed = self.call(0)? as i64;
                if needed <= 1 {
                    return Some(0);
                }
                self.call(1)?;
                self.call(2)?;
                self.call(3)?;
                Some(1)
            }
            _ => Some(0),
        }
    }
}
/// # Safety
/// Callback/context/failure storage remain valid for this synchronous call.
/// Native callbacks contain C++ exceptions and retain every resource and cursor.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn a2fo_rs_stage9_module_menu_flow(
    kind: u32,
    step: Step,
    context: *mut c_void,
    failed: *const u8,
) -> u64 {
    if failed.is_null() {
        return 0;
    }
    Flow {
        step,
        context,
        failed,
    }
    .run(kind)
    .unwrap_or(0)
}
