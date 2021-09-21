BUILD_DIR=build
COSMO_DIR = cosmo-engine/src

include n64.mk

CFLAGS += -I$(COSMO_DIR) -In64/SDL -I$(N64_ROOTDIR)/include
LDFLAGS += -L$(N64_ROOTDIR)/lib -lmikmod

SRCS = \
	n64_main.c \
	n64_audio.c \
	n64_input.c \
	n64_music.c \
	n64_sfx.c \
	n64_video.c \
	n64/rdl.c \
	$(COSMO_DIR)/actor_collision.c \
	$(COSMO_DIR)/actor_toss.c \
	$(COSMO_DIR)/actor_worktype.c \
	$(COSMO_DIR)/actor.c \
	$(COSMO_DIR)/b800.c \
	$(COSMO_DIR)/backdrop.c \
	$(COSMO_DIR)/cartoon.c \
	$(COSMO_DIR)/config.c \
	$(COSMO_DIR)/demo.c \
	$(COSMO_DIR)/dialog.c \
	$(COSMO_DIR)/effects.c \
	$(COSMO_DIR)/font.c \
	$(COSMO_DIR)/fullscreen_image.c \
	$(COSMO_DIR)/game.c \
	$(COSMO_DIR)/high_scores.c \
	$(COSMO_DIR)/map.c \
	$(COSMO_DIR)/palette.c \
	$(COSMO_DIR)/platforms.c \
	$(COSMO_DIR)/player.c \
	$(COSMO_DIR)/save.c \
	$(COSMO_DIR)/status.c \
	$(COSMO_DIR)/tile.c \
	$(COSMO_DIR)/util.c \
	$(COSMO_DIR)/files/file.c \
	$(COSMO_DIR)/files/vol.c

all: cosmo64.z64

$(BUILD_DIR)/cosmo64.dfs: $(wildcard filesystem/*)
$(BUILD_DIR)/cosmo64.elf: $(SRCS:%.c=$(BUILD_DIR)/%.o)

cosmo64.z64: N64_ROM_TITLE="cosmo64"
cosmo64.z64: $(BUILD_DIR)/cosmo64.dfs

clean:
	rm -rf $(BUILD_DIR) cosmo64.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
