#[cxx::bridge]
mod ffi {
    extern "Rust" {
        type RsScopeMeasure;

        fn create_scopemeasure(scope_name: &str) -> Box<RsScopeMeasure>;
        fn destroy_scopemeasure(object: Box<RsScopeMeasure>);
        fn scopemeasure_stopover(object: &RsScopeMeasure, stopover_name: &str);
    }
}
pub use ffi::*;

use libnewsboat::scopemeasure;

type RsScopeMeasure = scopemeasure::ScopeMeasure;

fn create_scopemeasure(scope_name: &str) -> Box<RsScopeMeasure> {
    Box::new(RsScopeMeasure::new(scope_name.to_string()))
}

fn destroy_scopemeasure(_object: Box<RsScopeMeasure>) {
    // We simply drop the object.
}

fn scopemeasure_stopover(object: &RsScopeMeasure, stopover_name: &str) {
    object.stopover(stopover_name);
}
