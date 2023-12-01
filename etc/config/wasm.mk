# wasm.mk
# Rules to build the thing for WebAssembly.

WASM_MICRO_RUNTIME:=/home/andy/proj/thirdparty/wasm-micro-runtime
WASI_SDK:=/home/andy/proj/thirdparty/wasi-sdk-16.0

LIBVMLIB=$(WASM_MICRO_RUNTIME)/product-mini/platforms/linux/build/libvmlib.a
IWASMHDR=$(WASM_MICRO_RUNTIME)/core/iwasm/include

WASI_CCOPT:=-nostdlib -c -MMD -Isrc -Isrc/main -Wno-comment -Wno-parentheses -Wno-constant-conversion
WASI_LDOPT:=-nostdlib -Xlinker --no-entry -Xlinker --import-undefined -Xlinker --export-all
CC_WASM=$(WASI_SDK)/bin/clang $(WASI_CCOPT)
CXX_WASM=$(WASI_SDK)/bin/clang $(WASI_CCOPT)
OBJC_WASM=$(WASI_SDK)/bin/clang $(WASI_CCOPT)
LD_WASM=$(WASI_SDK)/bin/clang $(WASI_LDOPT)
LDPOST_WASM:=

