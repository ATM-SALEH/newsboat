#ifndef NEWSBOAT_SCOPEMEASURE_H_
#define NEWSBOAT_SCOPEMEASURE_H_

#include <string>

#include "target/cxxbridge/libnewsboat-ffi/src/scopemeasure.rs.h"

namespace newsboat {

class ScopeMeasure {
public:
	ScopeMeasure(const std::string& func);
	~ScopeMeasure();
	void stopover(const std::string& son = "");

private:
	rust::Box<RsScopeMeasure> rs_object;
};

} // namespace newsboat

#endif /* NEWSBOAT_SCOPEMEASURE_H_ */
