#output dir
BUILD ?= build
LIBSDIR = libs
INCLUDESDIR = includes
BINDIR = programs

DEVICE=atmega328p

ifneq ($(BUILD),$(notdir $(CURDIR)))
#when make in run in toplevel directory

#export SILENT = 
export SILENT = @

export EXTRA_LIBS = -lm

export TOPDIR = $(CURDIR)

export CFLAGS += -gdwarf-2 -Wall -Os -mmcu=$(DEVICE) -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums -std=gnu99
export ASFLAGS += -gdwarf-2 -Wall -mmcu=$(DEVICE)
export LDFLAGS += -gdwarf-2 -Wall -mmcu=$(DEVICE)

.PHONY:	all clean $(BUILD)

all:	$(BUILD)

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $@ -f $(CURDIR)/Makefile

clean:
	@rm -rf $(BUILD)

else
#when make is run in the build directory

# $(eval $(call build-library,name))
# this tries to auto search for all the needed source files
define build-library
$(LIBSDIR):	$(LIBSDIR)/$1.a
$(LIBSDIR)/$1.a:	$(LIBSDIR)/$1/$1.a
	$$(SILENT)[ -d $$(WHERE) ] || mkdir -p $$(WHERE)
	$$(SILENT)cp $$< $$@
$(LIBSDIR)/$1/$1.a:	$$(subst .asm,.o,$$(subst $$(TOPDIR)/,,$$(wildcard $$(TOPDIR)/$$(LIBSDIR)/$1/*.asm))) $$(subst .c,.o,$$(subst $$(TOPDIR)/,,$$(wildcard $$(TOPDIR)/$$(LIBSDIR)/$1/*.c))) $$(subst .S,.o,$$(subst $$(TOPDIR)/,,$$(wildcard $$(TOPDIR)/$$(LIBSDIR)/$1/*.S))) $$(subst .s,.o,$$(subst $$(TOPDIR)/,,$$(wildcard $$(TOPDIR)/$$(LIBSDIR)/$1/*.s)))
	@echo $$(notdir $$@)
	$$(SILENT)[ -d $$(WHERE) ] || mkdir -p $$(WHERE)
	$$(SILENT)rm -f $$@
	$$(SILENT)$$(AR) cr $$@ $$^
-include $$(subst .asm,.P,$$(subst $$(TOPDIR)/,,$$(wildcard $$(TOPDIR)/$$(LIBSDIR)/$1/*.asm))) $$(subst .c,.P,$$(subst $$(TOPDIR)/,,$$(wildcard $$(TOPDIR)/$$(LIBSDIR)/$1/*.c))) $$(subst .S,.P,$$(subst $$(TOPDIR)/,,$$(wildcard $$(TOPDIR)/$$(LIBSDIR)/$1/*.S)))
endef

# $(eval $(call build-program,name,deps))
# this tries to auto search for all the needed source files
define build-program
$1/$1.elf : LDFLAGS += -Wl,-Map=$(BINDIR)/$1.map
$(BINDIR):	$(BINDIR)/$1.hex $(BINDIR)/$1.lst
$(BINDIR)/$1.hex:	$1/$1.hex
	$$(SILENT)[ -d $$(WHERE) ] || mkdir -p $$(WHERE)
	$$(SILENT)cp $$< $$@
$(BINDIR)/$1.lst:	$1/$1.lst
	$$(SILENT)[ -d $$(WHERE) ] || mkdir -p $$(WHERE)
	$$(SILENT)cp $$< $$@
$1/$1.elf:	$$(subst .asm,.o,$$(subst $$(TOPDIR)/,,$$(wildcard $$(TOPDIR)/$1/*.asm))) $$(subst .c,.o,$$(subst $$(TOPDIR)/,,$$(wildcard $$(TOPDIR)/$1/*.c))) $$(subst .S,.o,$$(subst $$(TOPDIR)/,,$$(wildcard $$(TOPDIR)/$1/*.S))) $$(subst .s,.o,$$(subst $$(TOPDIR)/,,$$(wildcard $$(TOPDIR)/$1/*.s)))
$1/$1.lst:	$1/$1.elf
$1/$1.elf:	$2
-include $$(subst .asm,.P,$$(subst $$(TOPDIR)/,,$$(wildcard $$(TOPDIR)/$1/*.asm))) $$(subst .c,.P,$$(subst $$(TOPDIR)/,,$$(wildcard $$(TOPDIR)/$1/*.c))) $$(subst .S,.P,$$(subst $$(TOPDIR)/,,$$(wildcard $$(TOPDIR)/$1/*.S)))
endef

# $(eval $(call copy-file,path)
# this copies a file at relative path "path" to the build directory
define copy-file
$(BINDIR):	$(BINDIR)/$(notdir $1)
$(BINDIR)/$(notdir $1):	$1
	@echo $$(notdir $$@)
	$$(SILENT)[ -d $$(WHERE) ] || mkdir -p $$(WHERE)
	$$(SILENT)cp $$< $$@
endef

# $(eval $(call copy-exe,path)
# this copies a file at relative path "path" to the build directory and make it executable
# it will auto-strip .sh
define copy-exe
$(BINDIR):	$(subst .sh,,$(BINDIR)/$(notdir $1))
$(BINDIR)/$(subst .sh,,$(notdir $1)):	$1
	@echo $$(notdir $$@)
	$$(SILENT)[ -d $$(WHERE) ] || mkdir -p $$(WHERE)
	$$(SILENT)cp $$< $$@
	$$(SILENT)chmod +x $$@
endef

# $(eval $(call touch-file,path)
# this creates a file at build/"path"
define touch-file
all:	$1
$1:
	@echo $$(notdir $$@)
	$$(SILENT)[ -d $$(WHERE) ] || mkdir -p $$(WHERE)
	$$(SILENT)touch $$@
endef



.PHONY:	all $(LIBSDIR) $(INCLUDESDIR) $(BINDIR)

all:	$(LIBSDIR) $(INCLUDESDIR) $(BINDIR)

$(INCLUDESDIR):
$(LIBSDIR):
$(BINDIR):

$(BINDIR):	$(LIBSDIR) $(INCLUDESDIR)

$(eval $(call build-program,pid-wheel,libs/lib485net_lib.a libs/bl_support.a libs/avrfixedmath.a))
$(eval $(call build-program,linact,libs/lib485net_lib.a libs/bl_support.a))
$(eval $(call build-program,485net-bootloader,))
$(eval $(call build-program,lib485net,))
$(eval $(call build-library,lib485net_lib))
$(eval $(call build-library,bl_support))
$(eval $(call build-library,avrfixedmath))

test485net/test485net.elf : LDFLAGS += -Wl,--defsym=__stack=0x800500
pid-wheel/pid-wheel.elf : LDFLAGS += -Wl,--defsym=__stack=0x800500
linact/linact.elf : LDFLAGS += -Wl,--defsym=__stack=0x800500

485net-bootloader/485net-bootloader.elf : LDFLAGS += -Wl,--undefined=_jumptable
485net-bootloader/485net-bootloader.hex : OBJCOPYFLAGS += -j .jumps

#section-start is because avr-gcc is weird
lib485net/lib485net.elf : LDFLAGS += -nostartfiles -Wl,--section-start,.data=0x800500

#hack
485net-bootloader/485net-bootloader.elf:
	@echo $(notdir $@)
	@echo NOTE: bootloader
	$(SILENT)[ -d $(WHERE) ] || mkdir -p $(WHERE)
	$(SILENT)[ -d $(BINDIR) ] || mkdir -p $(BINDIR)
	$(SILENT)$(LD) $(LDFLAGS) -Wl,-T$(TOPDIR)/485net-bootloader/avr5.x -o $@ $+ $(EXTRA_LIBS)

lib485net/lib485net.elf:
	@echo $(notdir $@)
	@echo NOTE: library
	$(SILENT)[ -d $(WHERE) ] || mkdir -p $(WHERE)
	$(SILENT)[ -d $(BINDIR) ] || mkdir -p $(BINDIR)
	$(SILENT)$(LD) $(LDFLAGS) -Wl,-T$(TOPDIR)/lib485net/avr5.x -o $@ $+ $(EXTRA_LIBS)

include $(TOPDIR)/rules.mk

endif
