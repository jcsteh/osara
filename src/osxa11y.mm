#import <AppKit/AppKit.h>
#include "osxa11y_wrapper.h"

@interface OSXA11y : NSObject
- (void)announce:(NSString*)message;
@end

@implementation OSXA11y
namespace NSA11yWrapper {
  struct osxa11yImpl
  {
    OSXA11y* pWrapper;
  };

  void init()
  {
    impl = new osxa11yImpl;
    impl->pWrapper = [[OSXA11y alloc] init];
  }

  void osxa11y_announce(const std::string& message)
  {
    NSString* param = [NSString stringWithUTF8String:message.c_str()];
    [impl->pWrapper announce:param];
  }

  void destroy()
  {
    if (impl != NULL)
      delete impl;
  }    
}

- (void)announce:(NSString*)message {
  NSDictionary *announcementInfo = @{NSAccessibilityAnnouncementKey : message,
				     NSAccessibilityPriorityKey : @(NSAccessibilityPriorityHigh)};

  NSAccessibilityPostNotificationWithUserInfo([NSApp keyWindow], NSAccessibilityAnnouncementRequestedNotification, announcementInfo);
}
@end
