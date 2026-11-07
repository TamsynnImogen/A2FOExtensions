fn main() {
    if std::env::var("TARGET").is_ok_and(|s| s == "i686-pc-windows-gnu") {
        let def = std::path::PathBuf::from(std::env::var_os("CARGO_MANIFEST_DIR").unwrap())
            .join("exports.def");
        println!("cargo:rerun-if-changed={}", def.display());
        println!("cargo:rustc-cdylib-link-arg={}", def.display());
    }
}
