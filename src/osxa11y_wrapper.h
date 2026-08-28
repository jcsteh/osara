#ifndef osxa11y_wrapper_h
#define osxa11y_wrapper_h
#include <iostream>

namespace NSA11yWrapper {
struct osxa11yImpl;
static osxa11yImpl* impl;

void init();
void osxa11y_announce(const std::string& param);
void destroy();
} // namespace NSA11yWrapper

#endif
