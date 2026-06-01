#ifndef linuxa11y_wrapper_h
#define linuxa11y_wrapper_h

#include <string>

namespace LinuxA11y {
	bool init();
	void announce(const std::string& message, bool interrupt);
	void destroy();
}

#endif
