fn main() {
    println!("cargo:rerun-if-changed=native/fire_arc_bridge.cpp");
    println!("cargo:rerun-if-changed=../../modules/A2FOFireArcs/fire_arc.cpp");
    println!("cargo:rerun-if-changed=../../modules/A2FOFireArcs/fire_arc.hpp");

    cc::Build::new()
        .cpp(true)
        .file("native/fire_arc_bridge.cpp")
        .file("../../modules/A2FOFireArcs/fire_arc.cpp")
        .include("../../modules/A2FOFireArcs")
        // Let cc-rs select the compiler-specific spelling: `-std=c++17` for
        // GCC/Clang and `/std:c++17` for MSVC.
        .std("c++17")
        .warnings(true)
        .compile("a2fo_fire_arc_geometry");
}
