# native.mk
# Rules for building the game for your PC.

UNAMESMN:=$(shell uname -smn)
ifeq ($(UNAMESMN),Linux raspberrypi aarch64)
  # Pi 4. Use DRM only.
  PO_NATIVE_PLATFORM:=linuxguiless
  RUNARGS:=--video-device=/dev/dri/card1
else ifneq (,$(strip $(filter raspberrypi,$(UNAMESMN))))
  # Other Pi. Use BCM only.
  PO_NATIVE_PLATFORM:=raspi
else ifneq (,$(strip $(filter vcs,$(UNAMESMN))))
  # Atari VCS, another bespoke game console i use. DRM only.
  PO_NATIVE_PLATFORM:=linuxguiless
else ifneq (,$(strip $(filter Linux,$(UNAMESMN))))
  # Linux in general, use both DRM and GLX
  PO_NATIVE_PLATFORM:=linux
else
  $(error Unable to detect host configuration)
endif

ifeq ($(PO_NATIVE_PLATFORM),linux) #-----------------------------------------------

  CC_NATIVE:=gcc -c -MMD -O2 -Isrc -Isrc/main -Werror -Wimplicit -DPO_NATIVE=1 -I/usr/include/libdrm
  LD_NATIVE:=gcc
  LDPOST_NATIVE:=-lm -lz -lX11 -ldrm -lgbm -lGLESv2 -lEGL
  OPT_ENABLE_NATIVE:=genioc x11 evdev drmgx
  OPT_ENABLE_TOOL:=alsa ossmidi inotify
  EXE_NATIVE:=out/native/ivand

else ifeq ($(PO_NATIVE_PLATFORM),raspi) #-----------------------------------------------

  CC_NATIVE:=gcc -c -MMD -O2 -Isrc -Isrc/main -Werror -Wimplicit -DPO_NATIVE=1 -I/opt/vc/include
  LD_NATIVE:=gcc -L/opt/vc/lib
  LDPOST_NATIVE:=-lm -lz -lbcm_host -lEGL -lGLESv2 -lGL
  OPT_ENABLE_NATIVE:=genioc evdev bcm
  OPT_ENABLE_TOOL:=alsa ossmidi inotify
  EXE_NATIVE:=out/native/ivand

else ifeq ($(PO_NATIVE_PLATFORM),linuxguiless) #-----------------------------------------------

  CC_NATIVE:=gcc -c -MMD -O2 -Isrc -Isrc/main -Werror -Wimplicit -DPO_NATIVE=1 -I/usr/include/libdrm
  LD_NATIVE:=gcc
  LDPOST_NATIVE:=-lm -lz -ldrm -lgbm -lGLESv2 -lEGL
  OPT_ENABLE_NATIVE:=genioc evdev drmgx
  OPT_ENABLE_TOOL:=alsa ossmidi inotify
  EXE_NATIVE:=out/native/ivand

else ifeq ($(PO_NATIVE_PLATFORM),macos) #------------------------------------------

  $(error TODO Build for MacOS)
  
else ifneq ($(PO_NATIVE_PLATFORM),mswin) #-----------------------------------------

  $(error TODO Build for Windows)

else ifeq ($(PO_NATIVE_PLATFORM),) #-----------------------------------------------
else
  $(error Undefined native platform '$(PO_NATIVE_PLATFORM)')
endif
