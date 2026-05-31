#import <AppKit/AppKit.h>
#include "osxa11y_wrapper.h"

@interface OSXA11y : NSObject
{
  NSString* pendingInterruptMessage;
  NSString* pendingPassiveMessage;
  NSTimer* flushTimer;
  NSTimeInterval lastWindowUpdateTime;
  NSTimeInterval pendingInterruptStartTime;
  NSTimeInterval pendingPassiveStartTime;
}
- (void)announce:(NSString*)message interrupt:(BOOL)interrupt;
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

  void osxa11y_announce(const std::string& message, bool interrupt)
  {
    NSString* param = [NSString stringWithUTF8String:message.c_str()];
    [impl->pWrapper announce:param interrupt:interrupt];
  }

  void destroy()
  {
    if (impl != nullptr) {
      [impl->pWrapper release];
      delete impl;
      impl = nullptr;
    }
  }    
}

static const NSTimeInterval COALESCE_DELAY = 0.05;
static const NSTimeInterval REDRAW_QUIET_DELAY = 0.075;
static const NSTimeInterval INTERRUPT_MAX_DELAY = 0.15;
static const NSTimeInterval PASSIVE_MAX_DELAY = 0.4;

- (id)init {
  if ((self = [super init])) {
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(windowDidUpdate:)
                                                 name:NSWindowDidUpdateNotification
                                               object:nil];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [flushTimer invalidate];
  [pendingInterruptMessage release];
  [pendingPassiveMessage release];
  [super dealloc];
}

- (NSTimeInterval)now {
  return [NSDate timeIntervalSinceReferenceDate];
}

- (void)windowDidUpdate:(NSNotification*)notification {
  lastWindowUpdateTime = [self now];
  if (pendingInterruptMessage || pendingPassiveMessage) {
    [self scheduleFlush];
  }
}

- (void)announceRequest:(NSDictionary*)request {
  [self announce:[request objectForKey:@"message"]
       interrupt:[[request objectForKey:@"interrupt"] boolValue]];
}

- (void)announce:(NSString*)message interrupt:(BOOL)interrupt {
  if (![NSThread isMainThread]) {
    NSDictionary* request = @{
      @"message": message ? message : @"",
      @"interrupt": [NSNumber numberWithBool:interrupt]
    };
    [self performSelectorOnMainThread:@selector(announceRequest:)
                           withObject:request
                        waitUntilDone:NO];
    return;
  }
  if (!message || ![message length]) {
    return;
  }

  NSTimeInterval now = [self now];
  if (interrupt) {
    if (!pendingInterruptMessage) {
      pendingInterruptStartTime = now;
    }
    [pendingInterruptMessage release];
    pendingInterruptMessage = [message copy];
    [pendingPassiveMessage release];
    pendingPassiveMessage = nil;
  } else {
    if (pendingInterruptMessage) {
      return;
    }
    if (!pendingPassiveMessage) {
      pendingPassiveStartTime = now;
    }
    [pendingPassiveMessage release];
    pendingPassiveMessage = [message copy];
  }
  [self scheduleFlush];
}

- (NSTimeInterval)delayForPendingMessage {
  NSTimeInterval now = [self now];
  NSTimeInterval startTime;
  NSTimeInterval maxDelay;
  if (pendingInterruptMessage) {
    startTime = pendingInterruptStartTime;
    maxDelay = INTERRUPT_MAX_DELAY;
  } else if (pendingPassiveMessage) {
    startTime = pendingPassiveStartTime;
    maxDelay = PASSIVE_MAX_DELAY;
  } else {
    return 0;
  }

  NSTimeInterval elapsed = now - startTime;
  NSTimeInterval delay = 0;
  if (elapsed < COALESCE_DELAY) {
    delay = COALESCE_DELAY - elapsed;
  }
  if (lastWindowUpdateTime > 0) {
    NSTimeInterval sinceUpdate = now - lastWindowUpdateTime;
    if (sinceUpdate < REDRAW_QUIET_DELAY) {
      NSTimeInterval quietDelay = REDRAW_QUIET_DELAY - sinceUpdate;
      if (delay < quietDelay) {
        delay = quietDelay;
      }
    }
  }

  NSTimeInterval remaining = maxDelay - elapsed;
  if (remaining <= 0) {
    return 0;
  }
  if (delay > remaining) {
    return remaining;
  }
  return delay;
}

- (void)scheduleFlush {
  [flushTimer invalidate];
  flushTimer = nil;
  NSTimeInterval delay = [self delayForPendingMessage];
  flushTimer = [NSTimer scheduledTimerWithTimeInterval:delay
                                                target:self
                                              selector:@selector(flushTimerFired:)
                                              userInfo:nil
                                               repeats:NO];
}

- (void)flushTimerFired:(NSTimer*)timer {
  flushTimer = nil;
  if (!pendingInterruptMessage && !pendingPassiveMessage) {
    return;
  }
  if ([self delayForPendingMessage] > 0) {
    [self scheduleFlush];
    return;
  }

  NSString* message = nil;
  NSNumber* priority = nil;
  if (pendingInterruptMessage) {
    message = [pendingInterruptMessage retain];
    [pendingInterruptMessage release];
    pendingInterruptMessage = nil;
    priority = [NSNumber numberWithInt:NSAccessibilityPriorityHigh];
  } else {
    message = [pendingPassiveMessage retain];
    [pendingPassiveMessage release];
    pendingPassiveMessage = nil;
    priority = [NSNumber numberWithInt:NSAccessibilityPriorityMedium];
  }

  NSDictionary *announcementInfo = @{NSAccessibilityAnnouncementKey : message,
				     NSAccessibilityPriorityKey : priority};

  id target = [NSApp keyWindow];
  if (!target) {
    target = [NSApp mainWindow];
  }
  if (!target) {
    target = NSApp;
  }
  NSAccessibilityPostNotificationWithUserInfo(target, NSAccessibilityAnnouncementRequestedNotification, announcementInfo);
  [message release];
}
@end
