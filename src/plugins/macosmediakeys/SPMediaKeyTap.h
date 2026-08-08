/// @file SPMediaKeyTap.h
/// @brief Global media key interception for macOS.
/// @details Taps the system-defined media key events (play/pause, next/prev,
/// etc.) through a Core Foundation event tap so the media keys work even when
/// the app is not frontmost. Delegates handle the received NSEvents. macOS only.

#include <Cocoa/Cocoa.h>
#import <IOKit/hidsystem/ev_keymap.h>
#import <Carbon/Carbon.h>

// http://overooped.com/post/2593597587/mediakeys

/** @brief NSEvent subtype constant for system-defined media key events. */
#define SPSystemDefinedEventMediaKeys 8

/** @brief Listens for global media key presses on macOS.
 *  @details Installs an event tap on the system-defined event source and a
 *  dedicated run loop thread. Media key ownership follows the frontmost app
 *  from the media-key app list. Received events are forwarded to the delegate
 *  through the SPMediaKeyTapDelegate informal protocol. */
@interface SPMediaKeyTap : NSObject {
	/** @brief Handler reference for app-switch events. */
	EventHandlerRef _app_switching_ref;
	/** @brief Handler reference for app-terminate events. */
	EventHandlerRef _app_terminating_ref;
	/** @brief Mach port for the media key event tap. */
	CFMachPortRef _eventPort;
	/** @brief Run loop source wrapping the event port. */
	CFRunLoopSourceRef _eventPortSource;
	/** @brief Run loop of the tap thread. */
	CFRunLoopRef _tapThreadRL;
	/** @brief Whether media key events are currently intercepted. */
	BOOL _shouldInterceptMediaKeyEvents;
	/** @brief Delegate receiving media key events. */
	id _delegate;
	// The app that is frontmost in this list owns media keys
	/** @brief Bundle identifiers that currently own the media keys. */
	NSMutableArray *_mediaKeyAppList;
}
/** @brief Returns the default set of bundle ids that own the media keys.
 *  @return An array of bundle identifier strings. */
+ (NSArray*)defaultMediaKeyUserBundleIdentifiers;

/** @brief Creates a tap with the given delegate.
 *  @param delegate Object receiving media key events. */
-(id)initWithDelegate:(id)delegate;

/** @brief Returns whether the global media key tap is available.
 *  @return True on supported macOS versions. */
+(BOOL)usesGlobalMediaKeyTap;
/** @brief Starts watching media keys.
 *  @return True if the event tap was installed. */
-(BOOL)startWatchingMediaKeys;
/** @brief Stops watching media keys. */
-(void)stopWatchingMediaKeys;
/** @brief Handles a received media key event and releases it.
 *  @param event The media key event. */
-(void)handleAndReleaseMediaKeyEvent:(NSEvent *)event;
@end

/** @brief Delegate protocol for media key events. */
@interface NSObject (SPMediaKeyTapDelegate)
/** @brief Called when a media key event is received.
 *  @param keyTap The media key tap that received the event.
 *  @param event The media key event. */
-(void)mediaKeyTap:(SPMediaKeyTap*)keyTap receivedMediaKeyEvent:(NSEvent*)event;
@end

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Defaults key listing bundle identifiers that own the media keys. */
extern NSString *kMediaKeyUsingBundleIdentifiersDefaultsKey;
/** @brief Defaults key that disables media key interception. */
extern NSString *kIgnoreMediaKeysDefaultsKey;

#ifdef __cplusplus
}
#endif