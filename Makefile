# Set NATIVE_ONLY nonzero to skip the Tiny and WebAssembly builds.

NATIVE_ONLY:=1

all:
.SILENT:
.SECONDARY:
PRECMD=echo "  $(@F)" ; mkdir -p $(@D) ;

include etc/make/configure.mk
include etc/make/build.mk
include etc/make/tiny.mk
include etc/make/commands.mk
