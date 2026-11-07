//! Rust media decoding and playback. No native codec dependency.
#![deny(unsafe_op_in_unsafe_fn)]
pub mod audio;
pub mod codec;
pub mod pixels;

pub mod control;
mod ffi;
pub mod host;
pub mod player;
#[cfg(windows)]
mod windows;
