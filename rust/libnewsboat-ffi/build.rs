fn main() {
    cxx_build::bridge("src/utils.rs")
        .flag("-std=c++11")
        .compile("utils");
    cxx_build::bridge("src/scopemeasure.rs")
        .flag("-std=c++11")
        .compile("scopemeasure");

    println!("cargo:rerun-if-changed=src/utils.rs");
    println!("cargo:rerun-if-changed=src/scopemeasure.rs");
}
