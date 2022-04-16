# tiny.mk
# Your Arduino build environment.

ifeq (,) # greyskull
  TINY_PORT:=ttyACM0
  TINY_IDEROOT:=/opt/arduino-1.8.16
  TINY_IDEVERSION:=10816
  TINY_BUILDER_OPTS:=
  TINY_PKGROOT:=$(wildcard ~/.arduino15/packages)
  TINY_LIBSROOT:=$(wildcard ~/Arduino/libraries)
endif

ifeq (,no) # MacBook Air
  TINY_PORT:=tty.usbmodemFA131
  TINY_IDEROOT:=/Applications/Arduino.app/Contents/Java
  TINY_IDEVERSION:=10819
  TINY_BUILDER_OPTS:=
  TINY_PKGROOT:=$(wildcard ~/Library/Arduino15/packages)
  TINY_LIBSROOT:=$(wildcard ~/Documents/Arduino/libraries)
endif

TINY_BUILDER:=$(TINY_IDEROOT)/arduino-builder
