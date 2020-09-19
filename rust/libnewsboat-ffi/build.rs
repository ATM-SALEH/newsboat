fn main() {
    cxx_build::bridge("src/utils.rs").compile("cxxbridge-demo");

    println!("cargo:rerun-if-changed=src/utils.rs");
}
