# NeoBESMS

NeoBESMS is a modified version of ptr2besms, a tool for modifying lines on PaRappa the Rapper 2. This project adds some features and improves on what it originally did. I made it because I felt the need to and for my own use.

## Changes

* Added support for NTSC-J and PAL
* Loads pcsx2-parappa.exe without renaming
* Playback of lines until the end & immediate audio stop
* SFX (Boxxy) line now visible (used on some stages)
* Functional .OLM file injection
* Shift+F9 for manual PCSX2 base address
* Shift+P to play without ticks
* X to cut buttons, B to pick a soundboard
* Changes in Error/info/input messages
* Additional info displayed in editor
* Crash handling, tries to prevent loss of projects
* Some attempts to refactor

## Todo

* Make improvements in some areas
* Check for any audio issues

## Notes

* Expect some bugs for now as this project is very early.
* You will need a 64 bit build to load 64 bit PCSX2 (by using SHIFT+F9 and entering eemem address)

## Credits

posesix is the original author of the tool. I only made some changes.