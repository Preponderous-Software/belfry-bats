// A simple pilot that guides the flock through a night: over the rooftops,
// clear of the owls, into the belfry. The tests use it to show that every
// night can be won; the page can use it for a demo.
#pragma once

#include "sim/world.h"

namespace belfry {

Input autopilot(const World& w);

}  // namespace belfry
