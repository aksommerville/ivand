# configure.mk

include etc/config/native.mk
include etc/config/tiny.mk

# Beware: Many object files are shared between NATIVE and TOOL.
# Only those sources under src/tool/ actually see the declarations for OPT_ENABLE_TOOL.
CC_TOOL:=$(CC_NATIVE)
CC_NATIVE+=$(foreach U,$(OPT_ENABLE_NATIVE),-DPO_USE_$U=1)
CC_TOOL+=$(foreach U,$(OPT_ENABLE_TOOL),-DPO_USE_$U=1)
