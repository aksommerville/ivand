# native.mk
# Rules for building the game for your PC.

PO_NATIVE_PLATFORM:=linux

ifeq ($(PO_NATIVE_PLATFORM),linux) #-----------------------------------------------

  CC_NATIVE:=gcc -c -MMD -O2 -Isrc -Isrc/main -Werror -Wimplicit -DPO_NATIVE=1
  LD_NATIVE:=gcc
  LDPOST_NATIVE:=-lm -lz -lasound -lX11 -lpthread
  OPT_ENABLE_NATIVE:=genioc alsa x11 evdev
  OPT_ENABLE_TOOL:=alsa ossmidi inotify
  EXE_NATIVE:=out/native/pokorc

else ifeq ($(PO_NATIVE_PLATFORM),macos) #------------------------------------------

  $(error TODO Build for MacOS)
  
else ifneq ($(PO_NATIVE_PLATFORM),mswin) #-----------------------------------------

  $(error TODO Build for Windows)

else ifeq ($(PO_NATIVE_PLATFORM),) #-----------------------------------------------
else
  $(error Undefined native platform '$(PO_NATIVE_PLATFORM)')
endif
