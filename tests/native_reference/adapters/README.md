# Retired native adapters

These are exact pre-removal C++ interfaces/implementations retained for differential
fixtures. They are not production sources. Rust owns their operations; current
consumers use the Rust C interface. Forwarding headers select the actual Rust ABI.
Before-images and hashes are in build/rust-migration-stage16/adapter-cleanup.
