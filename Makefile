CROSS=powerpc64-unknown-linux-gnu-
CC=$(CROSS)gcc
OBJCOPY=$(CROSS)objcopy
LD=$(CROSS)ld
AS=$(CROSS)as
STRIP=$(CROSS)strip

SRC_DIR=source

# Configuration
CFLAGS = -Wall -Os -I$(SRC_DIR) -ffunction-sections -fdata-sections \
	-m64 -mno-toc -DBYTE_ORDER=BIG_ENDIAN -mno-altivec -D$(CURRENT_TARGET) $(CYGNOS_DEF)

AFLAGS = -Iinclude -m64
LDFLAGS = -nostdlib -n -m64 -Wl,--gc-sections

OBJS =	$(SRC_DIR)/startup.o \
	$(SRC_DIR)/main.o \
	$(SRC_DIR)/cache.o \
	$(SRC_DIR)/ctype.o \
	$(SRC_DIR)/string.o \
	$(SRC_DIR)/time.o \
	$(SRC_DIR)/vsprintf.o \
	$(SRC_DIR)/puff/puff.o

TARGETS = nax-1f nax-2f nax-gggggg nax-gggggg_cygnos_demon nax-1f_cygnos_demon nax-2f_cygnos_demon

# Build rules
all: $(foreach name,$(TARGETS),$(addprefix $(name).,build))

.PHONY: clean %.build

clean:
	@echo Cleaning...
	@rm -rf $(OBJS) $(foreach name,$(TARGETS),$(addprefix $(name).,bin elf))

%.build:
	@echo Building $* ...
	@$(MAKE) --no-print-directory $*.bin

.c.o:
	@echo [$(notdir $<)]
	@$(CC) $(CFLAGS) -c -o $@ $*.c

.S.o:
	@echo [$(notdir $<)]
	@$(CC) $(AFLAGS) -c -o $@ $*.S

nax-gggggg.elf: CURRENT_TARGET = HACK_GGGGGG
nax-1f.elf nax-2f.elf: CURRENT_TARGET = HACK_JTAG

nax-gggggg_cygnos_demon.elf: CURRENT_TARGET = HACK_GGGGGG
nax-gggggg_cygnos_demon.elf: CYGNOS_DEF = -DCYGNOS
nax-1f_cygnos_demon.elf nax-2f_cygnos_demon.elf: CURRENT_TARGET = HACK_JTAG
nax-1f_cygnos_demon.elf nax-2f_cygnos_demon.elf: CYGNOS_DEF = -DCYGNOS

%.elf: $(SRC_DIR)/%.lds $(OBJS)
	@$(CC) -n -T $< $(LDFLAGS) -o $@ $(OBJS)

%.bin: %.elf
	@$(OBJCOPY) -O binary $< $@
	@truncate --size=262128 $@ # 256k - footer size
	@echo -n "xxxxxxxxxxxxxxxx" >> $@ # add footer

FORCE:
