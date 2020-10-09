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

// RsScopeMeasure is opaque to C++, so we can only pass it by pointer. Reference won't work as it
// would prevent us from dropping the object. That leaves Box as the only option.
#[allow(clippy::boxed_local)]
fn destroy_scopemeasure(_object: Box<RsScopeMeasure>) {
    // We simply drop the object.
}

fn scopemeasure_stopover(object: &RsScopeMeasure, stopover_name: &str) {
    object.stopover(stopover_name);
}
