#!/bin/sh

# Change this script to meet your needs and/or environment.

TARGET=x86_64-w64-mingw32
PREFIX=/opt/cross_win64

PATH="$PREFIX/bin:$PATH"
export PATH

MAKE_CMD=make

CC="$TARGET-gcc"
CXX="$TARGET-g++"
AS="$TARGET-as"
RANLIB="$TARGET-ranlib"
AR="$TARGET-ar"
WINDRES="$TARGET-windres"
STRIP="$TARGET-strip"
export PATH CC CXX AS AR RANLIB WINDRES STRIP

exec $MAKE_CMD CC=$CC CXX=$CXX AS=$AS RANLIB=$RANLIB AR=$AR WINDRES=$WINDRES STRIP=$STRIP -f Makefile.w64 $*
