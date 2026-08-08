/// @file NSObject+SPInvocationGrabbing.h
/// @brief Deferred invocation helpers for Objective-C (macOS media key plugin).
/// @details Provides NSObject categories that "grab" a method call so it can be
/// invoked later on the current run loop, on the main thread, in the
/// background, or after a delay. Used by SPMediaKeyTap to schedule media key
/// handling safely.

#import <Foundation/Foundation.h>

/** @brief Captures an Objective-C invocation for deferred execution.
 *  @details Records the target object and NSInvocation so a method call can be
 *  replayed later, optionally on a specific thread or after a delay. */
@interface SPInvocationGrabber : NSObject {
    id _object;
    NSInvocation *_invocation;
    int frameCount;
    char **frameStrings;
    BOOL backgroundAfterForward;
    BOOL onMainAfterForward;
    BOOL waitUntilDone;
}
/** @brief Creates a grabber wrapping the given object.
 *  @param obj The object whose next message will be captured. */
-(id)initWithObject:(id)obj;
/** @brief Creates a grabber, optionally saving a stack backtrace.
 *  @param obj The object whose next message will be captured.
 *  @param saveStack Whether to capture a backtrace for debugging. */
-(id)initWithObject:(id)obj stacktraceSaving:(BOOL)saveStack;
/** @brief The object whose invocation was captured. */
@property (readonly, retain, nonatomic) id object;
/** @brief The captured invocation. */
@property (readonly, retain, nonatomic) NSInvocation *invocation;
/** @brief When set, the invocation runs on a background thread. */
@property BOOL backgroundAfterForward;
/** @brief When set, the invocation runs on the main thread. */
@property BOOL onMainAfterForward;
/** @brief When set, the caller blocks until the invocation completes. */
@property BOOL waitUntilDone;
/** @brief Executes the captured invocation (releases object and invocation). */
-(void)invoke; // will release object and invocation
/** @brief Prints the saved stack backtrace to stderr. */
-(void)printBacktrace;
/** @brief Saves a stack backtrace for later printing. */
-(void)saveBacktrace;
@end

/** @brief NSObject category for grabbing and deferring method calls. */
@interface NSObject (SPInvocationGrabbing)
/** @brief Returns a grabber for the next message sent to this object.
 *  @return An SPInvocationGrabber wrapping this object. */
-(id)grab;
/** @brief Invokes the grabbed call after a delay.
 *  @param delta Delay in seconds.
 *  @return The grabber performing the deferred invocation. */
-(id)invokeAfter:(NSTimeInterval)delta;
/** @brief Invokes the grabbed call on the next run loop pass.
 *  @return The grabber performing the deferred invocation. */
-(id)nextRunloop;
/** @brief Invokes the grabbed call on a background thread.
 *  @return The grabber performing the deferred invocation. */
-(id)inBackground;
/** @brief Invokes the grabbed call on the main thread.
 *  @param async True to dispatch asynchronously.
 *  @return The grabber performing the deferred invocation. */
-(id)onMainAsync:(BOOL)async;
@end
