// Copyright (c) 2010 Spotify AB
#import "SPMediaKeyTap.h"
#import "NSObject+SPInvocationGrabbing.h"

@interface SPMediaKeyTap ()
-(BOOL)shouldInterceptMediaKeyEvents;
-(void)setShouldInterceptMediaKeyEvents:(BOOL)newSetting;
-(void)startWatchingAppSwitching;
-(void)stopWatchingAppSwitching;
-(void)eventTapThread;
@end
static SPMediaKeyTap *singleton = nil;

static CGEventRef tapEventCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon);


// Inspired by http://gist.github.com/546311

@implementation SPMediaKeyTap

#pragma mark -
#pragma mark Setup and teardown
-(id)initWithDelegate:(id)delegate;
{
	_delegate = delegate;
	_mediaKeyAppList = [NSMutableArray new];
	[self startWatchingAppSwitching];
	singleton = self;
    _tapThreadRL=nil;
    _eventPort=nil;
    _eventPortSource=nil;
	return self;
}
-(void)dealloc;
{
	[self stopWatchingMediaKeys];
	[self stopWatchingAppSwitching];
	[_mediaKeyAppList release];
	[super dealloc];
}

-(void)startWatchingAppSwitching;
{
    NSNotificationCenter *center = [[NSWorkspace sharedWorkspace] notificationCenter];
    [center addObserver:self selector:@selector(appActivated:)
        name:NSWorkspaceDidActivateApplicationNotification object:nil];
    [center addObserver:self selector:@selector(appTerminated:)
        name:NSWorkspaceDidTerminateApplicationNotification object:nil];
}
-(void)stopWatchingAppSwitching;
{
    [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self];
}

-(BOOL)startWatchingMediaKeys;{
    // Prevent having multiple mediaKeys threads
    [self stopWatchingMediaKeys];

	[self setShouldInterceptMediaKeyEvents:YES];

	// Add an event tap to intercept the system defined media key events
	_eventPort = CGEventTapCreate(kCGSessionEventTap,
								  kCGHeadInsertEventTap,
								  kCGEventTapOptionDefault,
								  CGEventMaskBit(NX_SYSDEFINED),
								  tapEventCallback,
								  self);

	if (_eventPort) {
		_eventPortSource = CFMachPortCreateRunLoopSource(kCFAllocatorSystemDefault, _eventPort, 0);
		if (_eventPortSource) {
			// Let's do this in a separate thread so that a slow app doesn't lag the event tap
			[NSThread detachNewThreadSelector:@selector(eventTapThread) toTarget:self withObject:nil];
			return YES;
		}
	}

	return NO;
}
-(void)stopWatchingMediaKeys;
{
	// TODO<nevyn>: Shut down thread, remove event tap port and source

    if(_tapThreadRL){
        CFRunLoopStop(_tapThreadRL);
        _tapThreadRL=nil;
    }

    if(_eventPort){
        CFMachPortInvalidate(_eventPort);
        CFRelease(_eventPort);
        _eventPort=nil;
    }

    if(_eventPortSource){
        CFRelease(_eventPortSource);
        _eventPortSource=nil;
    }
}

#pragma mark -
#pragma mark Accessors

+(BOOL)usesGlobalMediaKeyTap
{
#ifdef _DEBUG
	// breaking in gdb with a key tap inserted sometimes locks up all mouse and keyboard input forever, forcing reboot
	return NO;
#else
	// XXX(nevyn): MediaKey event tap doesn't work on 10.4, feel free to figure out why if you have the energy.
	return
		![[NSUserDefaults standardUserDefaults] boolForKey:kIgnoreMediaKeysDefaultsKey]
		&& floor(NSAppKitVersionNumber) >= 949/*NSAppKitVersionNumber10_5*/;
#endif
}

+ (NSArray*)defaultMediaKeyUserBundleIdentifiers;
{
	return [NSArray arrayWithObjects:
		[[NSBundle mainBundle] bundleIdentifier], // your app
		@"com.spotify.client",
		@"com.apple.iTunes",
		@"com.apple.QuickTimePlayerX",
		@"com.apple.quicktimeplayer",
		@"com.apple.iWork.Keynote",
		@"com.apple.iPhoto",
		@"org.videolan.vlc",
		@"com.apple.Aperture",
		@"com.plexsquared.Plex",
		@"com.soundcloud.desktop",
		@"org.niltsh.MPlayerX",
		@"com.ilabs.PandorasHelper",
		@"com.mahasoftware.pandabar",
		@"com.bitcartel.pandorajam",
		@"org.clementine-player.clementine",
		@"fm.last.Last.fm",
		@"fm.last.Scrobbler",
		@"com.beatport.BeatportPro",
		@"com.Timenut.SongKey",
		@"com.macromedia.fireworks", // the tap messes up their mouse input
		@"at.justp.Theremin",
		@"ru.ya.themblsha.YandexMusic",
		@"com.jriver.MediaCenter18",
		@"com.jriver.MediaCenter19",
		@"com.jriver.MediaCenter20",
		@"co.rackit.mate",
		@"com.ttitt.b-music",
		@"com.beardedspice.BeardedSpice",
		@"com.plug.Plug",
		@"com.plug.Plug2",
    	@"com.netease.163music",
		@"org.quodlibet.quodlibet",
		nil
	];
}


-(BOOL)shouldInterceptMediaKeyEvents;
{
	BOOL shouldIntercept = NO;
	@synchronized(self) {
		shouldIntercept = _shouldInterceptMediaKeyEvents;
	}
	return shouldIntercept;
}

-(void)pauseTapOnTapThread:(BOOL)yeahno;
{
	CGEventTapEnable(self->_eventPort, yeahno);
}
-(void)setShouldInterceptMediaKeyEvents:(BOOL)newSetting;
{
	BOOL oldSetting;
	@synchronized(self) {
		oldSetting = _shouldInterceptMediaKeyEvents;
		_shouldInterceptMediaKeyEvents = newSetting;
	}
	if(_tapThreadRL && oldSetting != newSetting) {
		id grab = [self grab];
		[grab pauseTapOnTapThread:newSetting];
		NSTimer *timer = [NSTimer timerWithTimeInterval:0 invocation:[grab invocation] repeats:NO];
		CFRunLoopAddTimer(_tapThreadRL, (CFRunLoopTimerRef)timer, kCFRunLoopCommonModes);
	}
}

#pragma mark
#pragma mark -
#pragma mark Event tap callbacks

// Note: method called on background thread

static CGEventRef tapEventCallback2(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon)
{
	SPMediaKeyTap *self = refcon;

    if(type == kCGEventTapDisabledByTimeout) {
		NSLog(@"Media key event tap was disabled by timeout");
		CGEventTapEnable(self->_eventPort, TRUE);
		return event;
	} else if(type == kCGEventTapDisabledByUserInput) {
		// Was disabled manually by -[pauseTapOnTapThread]
		return event;
	}
	NSEvent *nsEvent = nil;
	@try {
		nsEvent = [NSEvent eventWithCGEvent:event];
	}
	@catch (NSException * e) {
		NSLog(@"Strange CGEventType: %d: %@", type, e);
		assert(0);
		return event;
	}

	if (type != NX_SYSDEFINED || [nsEvent subtype] != SPSystemDefinedEventMediaKeys)
		return event;

	int keyCode = (([nsEvent data1] & 0xFFFF0000) >> 16);
    if (keyCode != NX_KEYTYPE_PLAY && keyCode != NX_KEYTYPE_FAST && keyCode != NX_KEYTYPE_REWIND && keyCode != NX_KEYTYPE_PREVIOUS && keyCode != NX_KEYTYPE_NEXT)
		return event;

	if (![self shouldInterceptMediaKeyEvents])
		return event;

	[nsEvent retain]; // matched in handleAndReleaseMediaKeyEvent:
	[self handleAndReleaseMediaKeyEvent: nsEvent];
	return NULL;
}

static CGEventRef tapEventCallback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon)
{
	NSAutoreleasePool *pool = [NSAutoreleasePool new];
	CGEventRef ret = tapEventCallback2(proxy, type, event, refcon);
	[pool drain];
	return ret;
}

// event will have been retained in the other thread
-(void)handleAndReleaseMediaKeyEvent:(NSEvent *)event
{
	[event autorelease];
	[_delegate mediaKeyTap:self receivedMediaKeyEvent:event];
}

-(void)eventTapThread;
{
	_tapThreadRL = CFRunLoopGetCurrent();
	CFRunLoopAddSource(_tapThreadRL, _eventPortSource, kCFRunLoopCommonModes);
	CFRunLoopRun();
}

#pragma mark Task switching callbacks

NSString *kMediaKeyUsingBundleIdentifiersDefaultsKey = @"SPApplicationsNeedingMediaKeys";
NSString *kIgnoreMediaKeysDefaultsKey = @"SPIgnoreMediaKeys";



-(void)mediaKeyAppListChanged;
{
    if ([_mediaKeyAppList count] == 0) {
        return;
    }
    NSNumber *current = [NSNumber numberWithInt:[[NSProcessInfo processInfo] processIdentifier]];
    [self setShouldInterceptMediaKeyEvents:[[_mediaKeyAppList objectAtIndex:0] isEqual:current]];
}

-(void)appActivated:(NSNotification *)notification;
{
    NSRunningApplication *app = [[notification userInfo] objectForKey:NSWorkspaceApplicationKey];
    NSString *bundleIdentifier = [app bundleIdentifier];
    NSArray *whitelist = [[NSUserDefaults standardUserDefaults] arrayForKey:kMediaKeyUsingBundleIdentifiersDefaultsKey];
    if (!bundleIdentifier || ![whitelist containsObject:bundleIdentifier]) {
        return;
    }

    NSNumber *pid = [NSNumber numberWithInt:[app processIdentifier]];
    [_mediaKeyAppList removeObject:pid];
    [_mediaKeyAppList insertObject:pid atIndex:0];
    [self mediaKeyAppListChanged];
}

-(void)appTerminated:(NSNotification *)notification;
{
    NSRunningApplication *app = [[notification userInfo] objectForKey:NSWorkspaceApplicationKey];
    NSNumber *pid = [NSNumber numberWithInt:[app processIdentifier]];
    [_mediaKeyAppList removeObject:pid];
    [self mediaKeyAppListChanged];
}

@end
