#include "scopemeasure.h"


namespace newsboat {

ScopeMeasure::ScopeMeasure(const std::string& func)
	: rs_object(create_scopemeasure(func))
{
}

void ScopeMeasure::stopover(const std::string& son)
{
	scopemeasure_stopover(*rs_object, son);
}

ScopeMeasure::~ScopeMeasure()
{
	destroy_scopemeasure(std::move(rs_object));
}

} // namespace newsboat
