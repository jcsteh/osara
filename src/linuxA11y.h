#ifndef linuxA11y_h
#define linuxA11y_h

#include <string>

namespace linuxA11y {
	bool init();
	void announce(const std::string& message, bool interrupt);
	void destroy();
}

#endif
