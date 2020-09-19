fn main() {
    cxx_build::bridge("src/cxx_ffi.rs").compile("cxxbridge-demo");

    println!("cargo:rerun-if-changed=src/cxx_ffi.rs");
}
