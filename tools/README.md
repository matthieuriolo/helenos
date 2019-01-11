# Tools

This directory contains a set of different tools for building or working with HelenOS. Many of those tools should not be called directly but will invoked through the root Makefile. The tools which should not be used manually are marked with an **[I]**. Tools which must be called from the root directory are marked with **[R]**. Files which gets included by other tools and are not supposed to be executed at all are marked **[A]**.


## autocheck.awk [IR]



## autotool.py [I]

Detect important prerequisites and parameters for building HelenOS

## ccheck-build.sh [I]

Builds the newest version of ccheck tool

## ccheck.sh [IR]

Tests if the C syntax if formatted properly

## cc.sh [R]

Collects and displays all copyright owners

## check.sh [IR]

Perform pre-integration hands-off build of all profiles.

## config.py [IR]

HelenOS configuration system

## dest_build.py

HelenOS out-of-source-tree build utility

## ew.py [R]

Emulator wrapper for running HelenOS

## gentestfile.py

Generate a file to be used by app/testread

## imgutil.py [A]

Utilities for filesystem image creators

## jobfile.py [I]

Add a source/object file pair to a checker jobfile

## mkarray.py [I]

Binary inline data packer

## mkext2.py [I]

EXT2 creator

## mkext4.py [I]

EXT4 creator - alias for mkext2.py

## mkfat.py [I]

FAT creator

## mkuimage.py [I]

Create legacy uImage (U-Boot image)

## srepl [I]

String replacement per line

## toolchain.sh [I]

Builds the toolchain for a specific architecture

## travis.sh [I]

This is wrapper script for testing build of HelenOS under [Travis CI](https://travis-ci.org/HelenOS/helenos/)

## xstruct.py [A]

Convert descriptive structure definitions to structure object

## xtui.py [I]

Text User Interface wrapper
