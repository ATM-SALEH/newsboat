use libnewsboat::utils::*;

#[cxx::bridge(namespace = newsboat::utils)]
pub mod ffi {
    extern "Rust" {
        fn get_random_value(rs_max: u32) -> u32;
    }
}
