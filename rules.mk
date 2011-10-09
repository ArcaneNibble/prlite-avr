.SUFFIXES:

CC = avr-gcc
AS = avr-as
AR = avr-ar
LD = avr-gcc
OBJCOPY = avr-objcopy
OBJDUMP = avr-objdump
SED = sed

SOURCES		+= $(TOPDIR)/$(WHERE)
INCLUDES	+= $(TOPDIR)/$(WHERE) $(TOPDIR)/$(INCLUDESDIR) $(TOPDIR)/$(BUILD)/$(INCLUDESDIR)
LIBS		+= $(TOPDIR)/$(BUILD)/$(LIBSDIR)

CPPFLAGS	+= $(foreach file,$(INCLUDES),-I$(file))
LDFLAGS		+= $(foreach file,$(LIBS),-L$(file))
OBJCOPYFLAGS	+= -j .text -j .data -O ihex

VPATH = $(SOURCES)
vpath %.h $(INCLUDES)

WHERE = $(dir $@)

%.hex:	%.elf
	@echo $(notdir $@)
	$(SILENT)[ -d $(WHERE) ] || mkdir -p $(WHERE)
	$(SILENT)$(OBJCOPY) $(OBJCOPYFLAGS) $< $@

%.lst:	%.elf
	@echo $(notdir $@)
	$(SILENT)[ -d $(WHERE) ] || mkdir -p $(WHERE)
	$(SILENT)avr-objdump -h -S $< > $@

%.elf:
	@echo $(notdir $@)
	$(SILENT)[ -d $(WHERE) ] || mkdir -p $(WHERE)
	$(SILENT)[ -d $(BINDIR) ] || mkdir -p $(BINDIR)
	$(SILENT)$(LD) $(LDFLAGS) -o $@ $+ $(EXTRA_LIBS)

%.o:	%.c
	@echo $(notdir $@)
	$(SILENT)[ -d $(WHERE) ] || mkdir -p $(WHERE)
	$(SILENT)$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

%.o:	%.S
	@echo $(notdir $@)
	$(SILENT)[ -d $(WHERE) ] || mkdir -p $(WHERE)
	$(SILENT)$(CC) $(CPPFLAGS) -x assembler-with-cpp $(ASFLAGS) -c -o $@ $<

%.o:	%.s
	@echo $(notdir $@)
	$(SILENT)[ -d $(WHERE) ] || mkdir -p $(WHERE)
	$(SILENT)$(AS) $(ASFLAGS) -o $@ $<

%.P:	%.c
	@echo $(notdir $@)
	$(SILENT)[ -d $(WHERE) ] || mkdir -p $(WHERE)
	$(SILENT)$(CC) $(CFLAGS) $(CPPFLAGS) -M $< > $@.tmp0
	$(SILENT)$(SED) 's,\($(notdir $*)\)\.o[ :]*,$(dir $*)\1.o $@ : ,g' < $@.tmp0 > $@.tmp1
	$(SILENT)$(SED) -e 's/#.*//'		\
					-e 's/^[^:]*: *//'	\
					-e 's/ *\\$$//'	\
					-e '/^$$/ d'		\
					-e 's/$$/ :/' $@.tmp1 > $@.tmp2
	$(SILENT)cat $@.tmp1 $@.tmp2 > $@
	$(SILENT)rm $@.tmp0 $@.tmp1 $@.tmp2

%.P:	%.S
	@echo $(notdir $@)
	$(SILENT)[ -d $(WHERE) ] || mkdir -p $(WHERE)
	$(SILENT)$(CC) $(CFLAGS) $(CPPFLAGS) -M $< > $@.tmp0
	$(SILENT)$(SED) 's,\($(notdir $*)\)\.o[ :]*,$(dir $*)\1.o $@ : ,g' < $@.tmp0 > $@.tmp1
	$(SILENT)$(SED) -e 's/#.*//'		\
					-e 's/^[^:]*: *//'	\
					-e 's/ *\\$$//'	\
					-e '/^$$/ d'		\
					-e 's/$$/ :/' $@.tmp1 > $@.tmp2
	$(SILENT)cat $@.tmp1 $@.tmp2 > $@
	$(SILENT)rm $@.tmp0 $@.tmp1 $@.tmp2
