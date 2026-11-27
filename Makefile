# ===== GoldHEN Plugin Manager - OpenOrbis Makefile =====
# Package metadata.
TITLE       := GoldHEN Plugin Manager
VERSION     := 1.00
TITLE_ID    := BREW00777
CONTENT_ID  := IV0000-BREW00777_00-PLUGINMGR0000000

# Libraries linked into the ELF.
LIBS        := -lc -lkernel -lc++ -lSceNet -lSceHttp -lSceSsl -lSceUserService -lSceVideoOut -lSceAudioOut -lScePad -lSceSysmodule -lSceSysUtil -lSceFreeType -lSDL2 -lSDL2_image -lSceSystemService -lSceImeDialog -lSceBgft

# Additional compile flags.
EXTRAFLAGS  := -O2

# Folders. Source code lives in src/ (change SRCDIR if needed).
SRCDIR      := src
ASSETS      := $(wildcard assets/*/*)
LIBMODULES  := $(wildcard sce_module/*)

# You likely won't need to touch anything below this point.
TOOLCHAIN   := $(OO_PS4_TOOLCHAIN)
INTDIR      := build

ifeq ($(TOOLCHAIN),)
$(error OO_PS4_TOOLCHAIN is not set. Example: export OO_PS4_TOOLCHAIN=$$HOME/Downloads/OpenOrbis-PS4-Toolchain)
endif

CFILES      := $(wildcard $(SRCDIR)/*.c) $(wildcard $(SRCDIR)/third_party/miniz/*.c) $(wildcard $(SRCDIR)/jbc/*.c)
CPPFILES    := $(wildcard $(SRCDIR)/*.cpp)
OBJS        := $(patsubst $(SRCDIR)/%.c, $(INTDIR)/%.o, $(CFILES)) $(patsubst $(SRCDIR)/%.cpp, $(INTDIR)/%.o, $(CPPFILES))

CFLAGS      := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c $(EXTRAFLAGS) -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include
CXXFLAGS    := $(CFLAGS) -isystem $(TOOLCHAIN)/include/c++/v1 -I$(SRCDIR)
LDFLAGS     := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o

_unused     := $(shell mkdir -p $(INTDIR))

UNAME_S     := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
	CC      := clang
	CCX     := clang++
	LD      := ld.lld
	CDIR    := linux
endif
ifeq ($(UNAME_S),Darwin)
	CC      := /usr/local/opt/llvm/bin/clang
	CCX     := /usr/local/opt/llvm/bin/clang++
	LD      := /usr/local/opt/llvm/bin/ld.lld
	CDIR    := macos
endif

all: $(CONTENT_ID).pkg

$(CONTENT_ID).pkg: pkg.gp4
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core pkg_build $< .

pkg.gp4: eboot.bin sce_sys/about/right.sprx sce_sys/param.sfo sce_sys/icon0.png $(LIBMODULES) $(ASSETS)
	$(TOOLCHAIN)/bin/$(CDIR)/create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "$^"

sce_sys/param.sfo: Makefile
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_new $@
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gde'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'

eboot.bin: $(OBJS)
	$(LD) $(OBJS) -o $(INTDIR)/app.elf $(LDFLAGS)
	$(TOOLCHAIN)/bin/$(CDIR)/create-fself -in=$(INTDIR)/app.elf -out=$(INTDIR)/app.oelf --eboot "eboot.bin" --paid 0x3800000000000011

$(INTDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ $<

$(INTDIR)/%.o: $(SRCDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CCX) $(CXXFLAGS) -o $@ $<

clean:
	rm -rf $(CONTENT_ID).pkg pkg.gp4 eboot.bin $(INTDIR)

.PHONY: all clean
