Cnvrtsfx
========

Cnvrtsfx allows you to convert existing PSG sound effects into a format suitable for use with MBMSFXPlay. Naturally, the result will not be the same, but it will be a fair approximation. I suggest raising the volume of the original SFX to maximum, otherwise the FM version will be too quiet.


Workflow:

- Create your sound effect in a tracker of your choice which targets the SN76489 (e.g. MOD2PSG2, DefleMask, Sneventracker). 
  Make sure you use channel 2 exclusively, as data on other channels will be lost. 
  Export your SFX as a vgm file.

- Use sverx's [vgm2psg](https://github.com/sverx/PSGlib/tree/master/tools) to convert the vgm file to a psg file.
  Usage: vgm2psg inputfile.VGM outputfile.PSG 2
  Always use the attribute 2. Do not compress the PSG file.

- Use cnvrtsfx to convert the PSG SFX to an FM SFX.
  Usage: cnvrtsfx inputfile.PSG outputfile.BIN
