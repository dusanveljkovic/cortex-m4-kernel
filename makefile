PROGRAM = program
BUILD_DIR = build
DEBUG_ENABLED = 1

SOURCE_S_LIST = \
source/startup_code.s \
source/task_stack_init.s \
source/pend_sv_handler.s \
source/sv_call_handler.s 

SOURCE_C_LIST = \
source/main.c \
source/handlers.c \
source/systick.c \
source/scheduler.c \
source/tcb.c \
source/syscall.c \
source/static_memory.c \
source/semaphore.c

FLAGS_INCLUDE = \
-Iinclude

OBJECTS_LIST = 
OBJECTS_LIST += $(addprefix $(BUILD_DIR)/, $(notdir $(SOURCE_S_LIST:.s=.o)))
OBJECTS_LIST += $(addprefix $(BUILD_DIR)/, $(notdir $(SOURCE_C_LIST:.c=.o)))
vpath %.s $(sort $(dir $(SOURCE_S_LIST)))
vpath %.c $(sort $(dir $(SOURCE_C_LIST)))

CC = arm-none-eabi-gcc -c 
AS = arm-none-eabi-gcc -c -x assembler
LD = arm-none-eabi-ld 
HX = arm-none-eabi-objcopy -O ihex

MCU = -mcpu=cortex-m4 -mthumb

FLAGS_DEBUG = -g3 -O0 -fdebug-prefix-map=$(CURDIR)=.

FLAGS_AS =
FLAGS_AS += $(MCU)
ifeq ($(DEBUG_ENABLED), 1)
	FLAGS_AS += $(FLAGS_DEBUG)
endif

FLAGS_CC =
FLAGS_CC += $(MCU)
FLAGS_CC += -MMD -MP
FLAGS_CC += $(FLAGS_INCLUDE)
ifeq ($(DEBUG_ENABLED), 1)
	FLAGS_CC += $(FLAGS_DEBUG)
endif

LINKER_SCRIPT = linker_script.ld

all : $(BUILD_DIR)/$(PROGRAM).hex $(BUILD_DIR)/$(PROGRAM).elf

$(BUILD_DIR)/$(PROGRAM).hex : $(BUILD_DIR)/$(PROGRAM).elf
	$(HX) $(<) $(@)

$(BUILD_DIR)/$(PROGRAM).elf : $(OBJECTS_LIST) makefile
	$(LD) -T $(LINKER_SCRIPT) -o $(@) $(OBJECTS_LIST)

$(BUILD_DIR)/%.o : %.s makefile | $(BUILD_DIR)
	$(AS) $(FLAGS_AS) -o $(@) $(<)

$(BUILD_DIR)/%.o : %.c makefile | $(BUILD_DIR)
	$(CC) $(FLAGS_CC) -o $(@) $(<)

$(BUILD_DIR) :
	mkdir $(@)

-include $(wildcard $(BUILD_DIR)/*.d)

clean : 
	rm -rf $(BUILD_DIR)
