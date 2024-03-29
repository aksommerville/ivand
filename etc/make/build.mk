# build.mk

OPT_AVAILABLE:=$(notdir $(wildcard src/opt/*))
OPT_IGNORE_NATIVE:=$(filter-out $(OPT_ENABLE_NATIVE),$(OPT_AVAILABLE))
OPT_IGNORE_TOOL:=$(filter-out $(OPT_ENABLE_TOOL),$(OPT_AVAILABLE))

SRCFILES:=$(shell find src -type f)

# "embed" data files use one of our tools to go from binary to C source.
# Doing these three times is probably overkill, but there could be differences (eg byte order).
EMBED_SRCFILES:=$(filter src/data/embed/%,$(SRCFILES))
EMBED_CFILES_NATIVE:=$(patsubst src/data/embed/%,mid/native/data/embed/%.c,$(EMBED_SRCFILES))
EMBED_CFILES_TINY:=$(patsubst src/data/embed/%,mid/tiny/data/embed/%.c,$(EMBED_SRCFILES))
EMBED_CFILES_WASM:=$(patsubst src/data/embed/%,mid/wasm/data/embed/%.c,$(EMBED_SRCFILES))

CFILES:=$(filter %.c,$(SRCFILES))
CXXFILES_MAIN:=$(filter src/main/%.cpp,$(SRCFILES))
OFILES_NATIVE:=$(filter-out \
  $(addprefix mid/native/opt/,$(addsuffix /%,$(OPT_IGNORE_NATIVE))), \
  $(patsubst src/%.c,mid/native/%.o,$(CFILES)) \
) $(EMBED_CFILES_NATIVE:.c=.o)
OFILES_TOOL_COMMON:=$(filter mid/native/tool/common/%,$(OFILES_NATIVE)) \
  $(filter-out $(addprefix mid/native/opt/,$(addsuffix /%,$(OPT_IGNORE_TOOL))), \
    $(patsubst src/%.c,mid/native/%.o,$(filter src/opt/%,$(CFILES))) \
  ) \
  mid/native/main/synth.o
OFILES_GAME:=$(filter mid/native/main/% mid/native/opt/% mid/native/data/embed/%,$(OFILES_NATIVE))
ifneq ($(MAKECMDGOALS),clean)
  -include $(OFILES_NATIVE:.o=.d)
endif
mid/native/%.o:src/%.c;$(PRECMD) $(CC_NATIVE) -o $@ $<
mid/native/%.o:mid/native/%.c;$(PRECMD) $(CC_NATIVE) -o $@ $<
mid/native/tool/%.o:src/tool/%.c;$(PRECMD) $(CC_TOOL) -o $@ $<

ifndef NATIVE_ONLY
  # WebAssembly binary...
  OFILES_WASM:= \
    $(filter-out mid/wasm/opt/% mid/wasm/tool/%,$(patsubst src/%,mid/wasm/%,$(CFILES:.c=.o))) \
    $(patsubst src/opt/wasm/%.c,mid/wasm/opt/wasm/%.o,$(filter src/opt/wasm/%,$(CFILES))) \
    $(EMBED_CFILES_WASM:.c=.o)
  ifneq ($(MAKECMDGOALS),clean)
    -include $(OFILES_WASM:.o=.d)
  endif
  mid/wasm/%.o:src/%.c;$(PRECMD) $(CC_WASM) -o $@ $<
  mid/wasm/%.o:mid/wasm/%.c;$(PRECMD) $(CC_WASM) -o $@ $<
  OUT_WASM:=out/www/ivand.wasm
  all:$(OUT_WASM)
  $(OUT_WASM):$(OFILES_WASM);$(PRECMD) $(LD_WASM) -o $@ $^ $(LDPOST_WASM)
  
  # Copy all the static htdocs too, so they're in one place...
  SRCFILES_WWW:=$(filter src/www/%,$(SRCFILES))
  OUTFILES_WWW:=$(patsubst src/www/%,out/www/%,$(SRCFILES_WWW))
  out/www/%:src/www/%;$(PRECMD) cp $< $@
  all:$(OUTFILES_WWW)
endif

define TOOL_RULES
  OFILES_TOOL_$1:=$(filter mid/native/tool/$1/%,$(OFILES_NATIVE)) $(OFILES_TOOL_COMMON)
  TOOL_$1:=out/tool/$1
  all:$$(TOOL_$1)
  $$(TOOL_$1):$$(OFILES_TOOL_$1);$$(PRECMD) $(LD_NATIVE) -o $$@ $$^ $(LDPOST_NATIVE)
endef
TOOLS:=$(filter-out common,$(notdir $(wildcard src/tool/*)))
$(foreach T,$(TOOLS),$(eval $(call TOOL_RULES,$T)))

# "include" data files get included verbatim, for the most part.
INCLUDE_SRCFILES:=$(filter src/data/include/%,$(SRCFILES))
INCLUDE_FILES_NATIVE:=$(patsubst src/data/include/%,out/native/data/%,$(INCLUDE_SRCFILES))
INCLUDE_FILES_TINY:=$(patsubst %/title.png,%/ivand.tsv, \
  $(patsubst src/data/include/%,out/tiny/data/%,$(INCLUDE_SRCFILES)) \
)
all:$(INCLUDE_FILES_NATIVE) $(INCLUDE_FILES_TINY)
out/native/data/%:src/data/include/%;$(PRECMD) cp $< $@
out/tiny/data/%:src/data/include/%;$(PRECMD) cp $< $@
out/tiny/data/ivand.tsv:src/data/include/title.png $(TOOL_cvtimg);$(PRECMD) $(TOOL_cvtimg) -o$@ $< --tiny

define EMBED_RULES
  mid/$1/data/embed/%.c:src/data/embed/% $(TOOL_cvtraw);$$(PRECMD) $(TOOL_cvtraw) -o$$@ $$< $2
  mid/$1/data/embed/%.png.c:src/data/embed/%.png $(TOOL_cvtimg);$$(PRECMD) $(TOOL_cvtimg) -o$$@ $$< $2
  mid/$1/data/embed/%.wave.c:src/data/embed/%.wave $(TOOL_mkwave);$$(PRECMD) $(TOOL_mkwave) -o$$@ $$< $2
  mid/$1/data/embed/%.mid.c:src/data/embed/%.mid $(TOOL_mksong);$$(PRECMD) $(TOOL_mksong) -o$$@ $$< $2
  mid/$1/data/embed/font.png.c:src/data/embed/font.png $(TOOL_mkfont);$$(PRECMD) $(TOOL_mkfont) -o$$@ $$< $2
endef
$(eval $(call EMBED_RULES,native,))
$(eval $(call EMBED_RULES,tiny,--tiny))
$(eval $(call EMBED_RULES,wasm,))

all:$(EXE_NATIVE)
$(EXE_NATIVE):$(OFILES_GAME);$(PRECMD) $(LD_NATIVE) -o $@ $(OFILES_GAME) $(LDPOST_NATIVE)

