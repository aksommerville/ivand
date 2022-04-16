# tiny.mk
# Special rules for building and launching for the TinyArcade.

PROJECT_NAME:=ivand

TINY_TMPDIR_SOLO:=mid/tiny/build-solo
TINY_TMPDIR_HOSTED:=mid/tiny/build-hosted
TINY_CACHEDIR_SOLO:=mid/tiny/cache-solo
TINY_CACHEDIR_HOSTED:=mid/tiny/cache-hosted
$(TINY_TMPDIR_SOLO) $(TINY_TMPDIR_HOSTED) $(TINY_CACHEDIR_SOLO) $(TINY_CACHEDIR_HOSTED):;mkdir -p $@

TINY_BIN_SOLO:=out/tiny/$(PROJECT_NAME)-solo.bin
TINY_BIN_HOSTED:=out/tiny/$(PROJECT_NAME)-hosted.bin
TINY_PACKAGE:=out/$(PROJECT_NAME).zip

TINY_SRCFILES:= \
  $(EMBED_CFILES_TINY) \
  $(filter src/main/%,$(CFILES)) \
  $(CXXFILES_MAIN) \
  $(filter src/main/%.h,$(SRCFILES)) \
  $(shell find src/opt/tiny -type f)

# All the C files get copied here to simplify our request to arduino-builder.
TINY_SCRATCHDIR:=mid/tiny/scratch
TINY_SCRATCHFILES:=
define TINY_SCRATCHRULE
  TINY_SCRATCHFILES+=mid/tiny/scratch/$(notdir $1)
  mid/tiny/scratch/$(notdir $1):$1;$$(PRECMD) cp $$< $$@
endef
$(foreach F,$(TINY_SRCFILES),$(eval $(call TINY_SCRATCHRULE,$F)))
mid/tiny/scratch/dummy.cpp:;$(PRECMD) touch $@
TINY_SCRATCHFILES:=mid/tiny/scratch/dummy.cpp $(TINY_SCRATCHFILES)

define BUILD # 1=goal, 2=tmpdir, 3=cachedir, 4=BuildOption
$1:$2 $3 $(TINY_SCRATCHFILES); \
  $(TINY_BUILDER) \
  -compile \
  -logger=machine \
  -hardware $(TINY_IDEROOT)/hardware \
  $(if $(TINY_PKGROOT),-hardware $(TINY_PKGROOT)) \
  -tools $(TINY_IDEROOT)/tools-builder \
  -tools $(TINY_IDEROOT)/hardware/tools/avr \
  $(if $(TINY_PKGROOT),-tools $(TINY_PKGROOT)) \
  -built-in-libraries $(TINY_IDEROOT)/libraries \
  $(if $(TINY_LIBSROOT),-libraries $(TINY_LIBSROOT)) \
  -fqbn=TinyCircuits:samd:tinyarcade:BuildOption=$4 \
  -ide-version=$(TINY_IDEVERSION) \
  -build-path $2 \
  -warnings=none \
  -build-cache $3 \
  -prefs=build.warn_data_percentage=75 \
  $(TINY_BUILDER_OPTS) \
  $(TINY_SCRATCHFILES) \
  2>&1 | etc/tool/reportstatus.py
endef

# For inclusion in a TinyArcade SD card.
TINY_PRODUCT_HOSTED:=$(TINY_TMPDIR_HOSTED)/dummy.cpp.bin
$(TINY_BIN_HOSTED):build-hosted;$(PRECMD) cp $(TINY_PRODUCT_HOSTED) $@
$(eval $(call BUILD,build-hosted,$(TINY_TMPDIR_HOSTED),$(TINY_CACHEDIR_HOSTED),TAgame))

# For upload.
TINY_PRODUCT_SOLO:=$(TINY_TMPDIR_SOLO)/dummy.cpp.bin
$(TINY_BIN_SOLO):build-solo;$(PRECMD) cp $(TINY_PRODUCT_SOLO) $@
$(eval $(call BUILD,build-solo,$(TINY_TMPDIR_SOLO),$(TINY_CACHEDIR_SOLO),TAmenu))
  
$(TINY_PACKAGE):$(TINY_BIN_HOSTED) $(DATA_INCLUDE_TINY);$(PRECMD) \
  rm -rf out/$(PROJECT_NAME) ; \
  mkdir out/$(PROJECT_NAME) || exit 1 ; \
  cp -r out/tiny/data/* out/$(PROJECT_NAME) || exit 1 ; \
  cp $(TINY_BIN_HOSTED) out/$(PROJECT_NAME)/$(PROJECT_NAME).bin || exit 1 ; \
  cd out ; \
  zip -r $(PROJECT_NAME).zip $(PROJECT_NAME) >/dev/null || exit 1 ; \
  rm -r $(PROJECT_NAME)
  
all:$(TINY_BIN_HOSTED) $(TINY_BIN_SOLO) $(TINY_PACKAGE)
