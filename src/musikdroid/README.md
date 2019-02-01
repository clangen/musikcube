# musikdroid

`musikdroid` is an android app that is used to stream music from, or remote control, existing `musikcube` installations (windows, macos, or linux clients). it is written in `kotlin`.

# building

because `musikdroid` is not available in the Google Play store, it uses [fabric.io](https://fabric.io) for crash reporting. this makes it slightly difficult to build out of the box, because the project's API keys are not checked in. if you are not a member of the dev team, but still want to play around with the code, you can do the following:

1. remove `Fabric.with(this, Crashlytics())` from `Application.kt`
2. remove `apply plugin: 'io.fabric'` from `app/build.gradle`

this should allow you to build and test locally without special keys.

the project is currently built using `Android Studio 3`

# attribution

the following icons were taken from [the noun project](https://thenounproject.com) under the [creative commons 3.0 license](https://creativecommons.org/licenses/by/3.0/)
- https://thenounproject.com/search/?q=remote&i=658529
- https://thenounproject.com/iconsguru/collection/audio/?oq=audio&cidx=0&i=1052418
