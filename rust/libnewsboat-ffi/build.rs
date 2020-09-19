fn main() {
    cxx_build::bridge("src/utils.rs").compile("utils");
    cxx_build::bridge("src/scopemeasure.rs").compile("scopemeasure");

    println!("cargo:rerun-if-changed=src/utils.rs");
    println!("cargo:rerun-if-changed=src/scopemeasure.rs");
}
