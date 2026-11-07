use std::{env, fs, path::Path};

fn main() {
    let manifest = env::var("A2FO_DLL_LINK_MANIFEST")
        .expect("A2FO_DLL_LINK_MANIFEST must name the ordered native link manifest");
    println!("cargo:rerun-if-env-changed=A2FO_DLL_LINK_MANIFEST");
    println!("cargo:rerun-if-changed={manifest}");
    let contents = fs::read_to_string(&manifest).expect("read native link manifest");
    for (index, line) in contents.lines().enumerate() {
        assert!(
            !line.is_empty(),
            "empty link argument at line {}",
            index + 1
        );
        if line.starts_with('-') {
            assert!(
                line.starts_with("-l") || line.starts_with("-L"),
                "unexpected link option at line {}",
                index + 1
            );
        } else {
            assert!(
                Path::new(line).is_file(),
                "missing link input at line {}: {}",
                index + 1,
                line
            );
            println!("cargo:rerun-if-changed={line}");
        }
        println!("cargo:rustc-link-arg={line}");
    }
    let contract = env::var("A2FO_DLL_EXPORT_CONTRACT")
        .expect("A2FO_DLL_EXPORT_CONTRACT must name the pinned export DEF");
    assert!(Path::new(&contract).is_file(), "missing pinned export DEF");
    println!("cargo:rerun-if-env-changed=A2FO_DLL_EXPORT_CONTRACT");
    println!("cargo:rerun-if-changed={contract}");
    println!("cargo:rustc-link-arg={contract}");
    // rustc passes -nodefaultlibs to the GCC driver. Restore the static
    // MinGW C++/CRT tail observed with `g++ -### -shared -static`.
    println!("cargo:rustc-link-arg=-Wl,-Bstatic");
    for library in [
        "-lstdc++",
        "-lmingw32",
        "-lgcc",
        "-lgcc_eh",
        "-lmingwex",
        "-lmsvcrt",
        "-lkernel32",
        "-lpthread",
        "-ladvapi32",
        "-lshell32",
        "-luser32",
        "-lkernel32",
        "-lmingw32",
        "-lgcc",
        "-lgcc_eh",
        "-lmingwex",
        "-lmsvcrt",
        "-lkernel32",
    ] {
        println!("cargo:rustc-link-arg={library}");
    }
}
