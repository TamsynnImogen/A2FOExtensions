//! Rust-owned Bink compatibility runtime under development. No automatic hooks.
#[cfg(windows)]
pub mod ffi;
pub mod pixels;
pub mod state;
pub mod surface;
#[cfg(windows)]
mod windows_audio;
