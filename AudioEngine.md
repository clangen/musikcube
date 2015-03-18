## Audio engine ##
The audio engine is based on Tuniac.  The low level stuff (output to the wave device, packetizer and decoders of various formats) is practically unchanged.  Higher level concepts such as AudioStream and the IAudioSourceInterface are recycled but also modified.  The CoreAudio class is not reused because of tight coupling with the UI.  The main interface to the audio engine is the Transport class.  The entire engine runs in separate threads from the UI.  It's possible to play multiple files simultaneously for mixing purposes.

## Short class descriptions ##
Classes are in the musik::core::audio namespace, except for the decoders.
  * Transport: interface to the audio engine.  It hides the engine's details the UI.  Starts and stops files, manages open audio streams and controls most audio related commands.  It also takes care of finding the right decoder for a given file (or something else).  The UI doesn't need to supply this information.
  * AudioStream: represents audio information, independent of the source (file, stream, CD, ...).  The stream sits between the low level audio source and the low level output.
  * AudioOuput: puts bits on the audio device.  Uses the Windows WaveOut API.
  * AudioPacketizer: chops up audio data for AudioOutput.
  * IAudioSource: interface that must be implemented for each audio source (MP3 files, Ogg, MP3 streams, CD, ...).
  * IAudioSourceSupplier: sits between IAudioSource and Transport.  Suppliers are registered with Transport.  It offers an IAudioSource to the Transport if it supports the format.  Each decoder needs an implementation of IAudioSource and IAudioSourceSupplier.
  * Plugins (decoders) for MP3, Ogg, audio CD and APE.  AAC (buggy), FLAC and WMA to come.

## Sigslot ##
The audio engine will use sigslot to communicate with the UI.  This is partially implemented with a simple console UI.  Should be finalized after merging with the real GUI.