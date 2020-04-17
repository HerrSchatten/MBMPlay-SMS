MBMPlay - SMS
=============

A port of the MoonBlaster replayer from the MSX to the Master System

MoonBlaster 1.4 driver. 
MB1.4 assembly sources written by Remco Schrijvers. 
Extended by BiFi / The New Image 2012/2019. 
Adapted for the SMS by Kagesan with help from Zipper and Calindro.

MBMPlay allows replaying song files created with Moonblaster 1.4 as background music in SEGA Master System homebrew programs.


Typical workflow:

- Track a module in MoonBlaster 1.4. Save module in USER mode, as MBMPlay will not play files saved in EDIT mode.

- Include MBMPlay and 'incbin' the MBM file to your Z80 ASM source.

- Put the number of the ROM bank where the tune is located and the start address of the tune in *MBMBank* and *MBMStart* respectively. Load tune and start playing it by calling MBMPlay or MBMPlayNoRepeat.

- Call MBMFrame once per frame.

MBMPlay functions reference:
----------------------------

**MBMPlay** / **MBMPlayNoRepeat**: Loads and starts a tune (playing it repeatedly or only once). Needs bank number in *MBMBank* and start address in *MBMStart*.

**MBMFrame**: Processes a music frame.

**MBMStop**: Stops / pauses the music.

**MBMResume**: Resumes the previously stopped music.

**MBMCancelLoop**: Sets a looping tune to no more loops after the current one. Non-looping tunes are unaffected. Loop marker must be set in MoonBlaster.

If *MBMStatus* is $00, then no tune is playing. $FF means that a tune is playing.

Putting any value other than 0 in *MBMFading* will fade out the currently playing tune smoothly.
- A value of 1 makes the volume drop by one level every frame. The tune fades out quickly. 
- A value of 255 makes the tune fade out very slowly.
- After the fade-out is complete, the tune is stopped and *MBMStatus* is set to $00.

MBMSFXPlay
==========
by Kagesan

MBMSFXPlay is an extension to MBMPlay and adds basic support for playing sound effects on FM channel 01, even if a Moonblaster module is being played at the same time.

I strongly suggest using FM sfx only as a fallback option specifically for users with the Mark III / FM Unit combo, and use better-sounding PSG sfx on systems that support both sound chips simultaneously instead.

Caveat: Due to the way the YM2413 handles sound generation, it is impossible to suspend a note and restore it to its previous state later. This means that, after a sound effect has finished, the channel will remain silent until another note is played. Additionally, commands that usually just alter the sound of an ongoing note (like modulation, pitchbend or linked notes) will re-trigger the ADSR envelope of the current instrument. Therefore, be very careful what you put on channel 01 of your tune if you want to use the sfx feature.


Typical workflow:

- Create a sound effect. Details on the file format can be found further down at the end of this ReadMe. Alternatively, you can convert an existing PSG sound effect with the command line tool cnvrtsfx.

- Include MBMSFXPlay and 'incbin' the sfx file to your Z80 ASM source.

- Put the number of the ROM bank where the sfx is located and the start address of the sfx in *MBMSFXBank* and *MBMSFXStart* respectively. Load sfx and start playing it by calling MBMSFXPlay or MBMSFXPlayLoop.

- Call MBMSFXFrame once per frame.

MBMSFXPlay functions reference:
-------------------------------

**MBMSFXPlay** / **MBMSFXPlayLoop**: Loads and starts an sfx, playing it once or repeating it from the loop point when the end is reached. If no loop point is included, MBMSFXPLayLoop will repeat the sfx from the beginning. Needs bank number in *MBMSFXBank* and start address in *MBMSFXStart*.

**MBMSFXFrame**: Processes an sfx frame.

**MBMSFXStop**: Stops the sfx.

**MBMSFXCancelLoop**: Sets a looping sfx to no more loops after the current one.

If *MBMSFXStatus* is $00, then no sfx is playing. A value of $01 means that an sfx is playing.

FM SFX file format
------------------

No header


Register/data pairs (Never use either seperately!):

$10 %ffffffff = Register write marker, followed by data byte. 
- Data byte contains 8 least significant bits of F. 
- F is a 9-bit value that, in conjunction with the octave information, determines the pitch of the note played.
- See YM2413 Application Manual for details on how to calculate F.

$20 %00skbbbf = Register write marker, followed by data byte.
- s = sustain, should be 0.
- k = key, should be 1.
- b = octave block (0-7).
- f = Most significant bit of F.

$30 %iiiivvvv = Register write marker, followed by data byte.
- i = instrument number (0-15). See YM2413 Application manual for details.
- v = volume (0-15). 0 is the highest volume, 15 is silence.
 

Single byte commands:

$FF = Loop begin marker (optional, sfx with no loop won't have this).

$0x = End of frame, wait for x-1 frames. A value of x=1 means no wait frames are inserted.

$00 = End of data (compulsory, at the end of file).
