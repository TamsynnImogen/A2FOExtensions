This is the frozen pre-stage14 A2FOMissions lifecycle implementation used only
by `tests/mmm_runtime_native.cpp` with fake framework interfaces. It is not a
production build input. Its SHA-256 is
`32830532d404d1235867101be79f5d4f974202151739ca5d0a136f1f31fed9e1`.
The preserved loader reference remains `missions/mmm_loader/shim.cpp`.

The native fixture compares real compiled C++ lifecycle behavior against the
Rust controller and C++ exception firewall. `synthetic-direct-input` explicitly
exercises a resume branch without claiming native callback re-registration.
The edit-mode case allocates controlled memory in its own synthetic process.
