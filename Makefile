ifndef EP
$(error Cosmo Episode not specified: use make EP=1, 2 or 3)
endif

SOURCE_DIR = $(CURDIR)
BUILD_DIR = build
COSMO_DIR = cosmo-engine/src
include $(N64_INST)/include/n64.mk

CFLAGS += -I$(COSMO_DIR) -In64/SDL -I$(N64_ROOTDIR)/include -DEP$(EP) -In64/ugfx
#100ms per frame is the original game speed. If you find this too slow and you can decrease this here
#This is speed up the game
CFLAGS += -DCOSMO_INTERVAL=100
LDFLAGS += -L$(N64_ROOTDIR)/lib -L$(CURDIR) 

SRCS = \
	n64_main.c \
	n64_audio.c \
	n64_input.c \
	n64_music.c \
	n64_sfx.c \
	n64_video.c \
	n64_save.c \
	n64/ugfx/ugfx.c \
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
	$(COSMO_DIR)/sound/opl.c \
	$(COSMO_DIR)/files/file.c \
	$(COSMO_DIR)/files/vol.c

ED64ROMCONFIGFLAGS = --savetype sram256k

PROG_NAME = cosmo64_ep$(EP)
all: $(PROG_NAME).z64

$(BUILD_DIR)/$(PROG_NAME).dfs: filesystem/COSMO.STN filesystem/COSMO$(EP).VOL
$(BUILD_DIR)/$(PROG_NAME).elf: $(SRCS:%.c=$(BUILD_DIR)/%.o) $(BUILD_DIR)/n64/ugfx/rsp_ugfx.o


$(PROG_NAME).z64: N64_ROM_TITLE="$(PROG_NAME)"
$(PROG_NAME).z64: $(BUILD_DIR)/$(PROG_NAME).dfs

clean:
	rm -rf $(BUILD_DIR) $(PROG_NAME).z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean
