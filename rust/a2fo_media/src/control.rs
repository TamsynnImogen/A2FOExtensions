use std::sync::{
    Condvar, Mutex,
    atomic::{AtomicBool, AtomicU64, Ordering},
};
use std::thread::ThreadId;

pub struct Control {
    pub cancel: AtomicBool,
    pub audio_frames: AtomicU64,
    pub audio_ready: AtomicBool,
    pub audio_finished: AtomicBool,
    pub seek: Mutex<Option<i64>>,
    owner: Mutex<Option<ThreadId>>,
    done: Condvar,
}
impl Control {
    pub fn new() -> Self {
        Self {
            cancel: AtomicBool::new(false),
            audio_frames: AtomicU64::new(0),
            audio_ready: AtomicBool::new(false),
            audio_finished: AtomicBool::new(false),
            seek: Mutex::new(None),
            owner: Mutex::new(None),
            done: Condvar::new(),
        }
    }
    pub fn cancelled(&self) -> bool {
        self.cancel.load(Ordering::Acquire)
    }
    pub fn start(&self) {
        *self.owner.lock().unwrap_or_else(|p| p.into_inner()) = Some(std::thread::current().id());
    }
    pub fn finish(&self) {
        *self.owner.lock().unwrap_or_else(|p| p.into_inner()) = None;
        self.done.notify_all();
    }
    pub fn wait(&self) -> bool {
        let mut owner = self.owner.lock().unwrap_or_else(|p| p.into_inner());
        if *owner == Some(std::thread::current().id()) {
            return false;
        }
        while owner.is_some() {
            owner = self.done.wait(owner).unwrap_or_else(|p| p.into_inner());
        }
        true
    }
}
