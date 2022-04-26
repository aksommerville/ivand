# commands.mk

run:$(EXE_NATIVE) $(INCLUDE_FILES_NATIVE);$(EXE_NATIVE)

launch:$(TINY_BIN_SOLO); \
  stty -F /dev/$(TINY_PORT) 1200 ; \
  sleep 2 ; \
  $(TINY_PKGROOT)/arduino/tools/bossac/1.7.0-arduino3/bossac -i -d --port=$(TINY_PORT) -U true -i -e -w $(TINY_BIN_SOLO) -R

sdcard:$(TINY_PACKAGE);etc/tool/sdcard.sh $(TINY_PACKAGE)

clean:;rm -rf mid out

test:;echo "TODO: make $@" ; exit 1

TA_MENU_BIN:=etc/ArcadeMenu.ino.bin
ifneq (,$(TA_MENU_BIN))
  deploy-menu:; \
    stty -F /dev/$(TINY_PORT) 1200 ; \
    sleep 2 ; \
    $(TINY_PKGROOT)/arduino/tools/bossac/1.7.0-arduino3/bossac -i -d --port=$(TINY_PORT) -U true -i -e -w $(TA_MENU_BIN) -R
endif

ifneq (,$(OUT_WASM))
  serve:$(OUT_WASM) $(OUTFILES_WWW) $(TOOL_http);$(TOOL_http) --htdocs=\$(realpath src/www)
endif

