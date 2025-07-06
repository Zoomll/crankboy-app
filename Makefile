HEAP_SIZE      = 8388208
STACK_SIZE     = 61800

PRODUCT = CrankBoy.pdx

SDK = ${PLAYDATE_SDK_PATH}
ifeq ($(SDK),)
	SDK = $(shell egrep '^\s*SDKRoot' ~/.Playdate/config | head -n 1 | cut -c9-)
endif

ifeq ($(SDK),)
	$(error SDK path not found; set ENV value PLAYDATE_SDK_PATH)
endif

# --- COPY LUA SCRIPTS ---
COPY_LUA_SCRIPTS := $(shell \
	mkdir -p Source/scripts; \
	for file in src/scripts/*.lua; do \
		if [ -e "$$file" ]; then \
			base=$$(basename "$$file" .lua); \
			cp "$$file" "Source/scripts/$${base}.l"; \
		fi; \
	done; \
)

VPATH += src
VPATH += peanut_gb
VPATH += minigb_apu
VPATH += lz4

# List C source files here
SRC += lz4/lz4.c
SRC += lz4/lz4hc.c
SRC += src/pgmusic.c
SRC += src/info_scene.c
SRC += src/cover_cache_scene.c
SRC += src/image_conversion_scene.c
SRC += src/http.c
SRC += src/version.c
SRC += src/credits_scene.c
SRC += src/modal.c
SRC += src/game_scanning_scene.c
SRC += src/game_scene.c
SRC += src/userstack.c
SRC += src/jparse.c
SRC += src/script.c
SRC += src/dtcm.c
SRC += src/revcheck.c
SRC += src/app.c
SRC += src/utility.c
SRC += src/scene.c
SRC += src/library_scene.c
SRC += src/array.c
SRC += src/listview.c
SRC += src/preferences.c
SRC += src/settings_scene.c

SRC += minigb_apu/minigb_apu.c
SRC += main.c
SRC += pdnewlib.c

SRC += lua-5.4.7/onelua.c

ASRC = setup.s

# List all user directories here
UINCDIR += src
UINCDIR += peanut_gb
UINCDIR += minigb_apu
UINCDIR += lcd
UINCDIR += lz4
UINCDIR += lua-5.4.7


# Note: if there are unexplained crashes, try disabling these.
# LUA: enable lua scripting (uses embedded lua engine)
# DTCM_ALLOC: allow allocating variables in DTCM at the low-address end of the region reserved for the stack.
# ITCM_CORE (requires DTCM_ALLOC, and special link_map.ld): run core interpreter from ITCM.
# NOLUA: disable lua support
# Note: DTCM only active on Rev A regardless.
UDEFS = -DDTCM_ALLOC -DITCM_CORE -DDTCM_DEBUG=0 -falign-loops=32 -fprefetch-loop-arrays

# Define ASM defines here
UADEFS =

# List the user directory to look for the libraries here
ULIBDIR =

# List all user libraries here
ULIBS =

override LDSCRIPT=./link_map.ld

include $(SDK)/C_API/buildsupport/common.mk

# Update pdxinfo from version.json (unless bundle.json present)
VERSION_JSON := Source/version.json
PDXINFO := Source/pdxinfo
PYTHON := $(shell command -v python3b 2>/dev/null)

ifneq ("$(wildcard Source/bundle.json)","")
ifdef PYTHON
    $(shell python3 ./pdxinfo.py Source/pdxinfo Source/version.json Source/pdxinfo)
else
    $(info WARNING: python3 required to update pdxinfo from version.json)
endif
endif

PDCFLAGS += --quiet

# --- CUSTOM CLEANUP ---
.PHONY: clean-scripts
clean-scripts:
	@echo "Cleaning up Lua scripts..."
	@rm -rf Source/scripts

clean: clean-scripts
