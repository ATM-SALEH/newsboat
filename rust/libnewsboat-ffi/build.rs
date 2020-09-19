fn main() {
    cxx_build::bridge("src/utils.rs").compile("utils");

    println!("cargo:rerun-if-changed=src/utils.rs");
}
