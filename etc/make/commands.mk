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
  
  -include etc/secret
  etc/secret:;echo "Please create and populate etc/secret to enable web deploy" ; exit 1
  ifneq (,$(strip $(WEB_SSH_HOST)))
    deploy-web:$(OUT_WASM) $(OUTFILES_WWW); \
      scp -r out/www/* $(WEB_SSH_USER)@$(WEB_SSH_HOST):$(WEB_SSH_PATH) || exit 1 ; \
      echo "Deployed to $(WEB_SSH_HOST)."
  else
    deploy-web:;echo "etc/secret must define WEB_SSH_USER, WEB_SSH_HOST, and WEB_SSH_PATH" ; exit 1
  endif
endif

