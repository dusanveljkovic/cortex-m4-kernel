PROGRAM = program
BENCH = 0
BENCH_IRQ = 0
OPT = -O0
BUILD_DIR = build$(if $(filter 1,$(BENCH)),-bench)
DEBUG_ENABLED = 1

SOURCE_S_LIST = \
source/kernel/startup_code.S \
source/kernel/task_stack_init.S \
source/kernel/pend_sv_handler.S \
source/kernel/handlers.S \
source/kernel/sv_call_handler.S 

SOURCE_C_LIST = \
source/kernel/arm.c \
source/kernel/bench.c \
source/kernel/main.c \
source/kernel/flash.c \
source/kernel/debug_handlers.c \
source/kernel/systick.c \
source/kernel/scheduler.c \
source/kernel/tcb.c \
source/kernel/syscall.c \
source/kernel/static_memory.c \
source/kernel/semaphore.c \
source/kernel/heap.c \
source/kernel/clock.c \
source/kernel/ring_buffer.c \
source/kernel/mpu.c \
source/kernel/sleep.c \
source/kernel/cli.c \
source/kernel/console.c \
source/kernel/kprintf.c \
source/kernel/driver.c \
source/kernel/net.c \
source/user/user_main.c \
source/user/uprintf.c \
source/user/dhcp.c \
source/drivers/spi.c \
source/drivers/usart.c \
source/drivers/usart_driver.c \
source/drivers/w5500.c \
source/utils.c

FLAGS_INCLUDE = \
-Iinclude \

OBJECTS_LIST = 
OBJECTS_LIST += $(addprefix $(BUILD_DIR)/, $(notdir $(SOURCE_S_LIST:.S=.o)))
OBJECTS_LIST += $(addprefix $(BUILD_DIR)/, $(notdir $(SOURCE_C_LIST:.c=.o)))
vpath %.S $(sort $(dir $(SOURCE_S_LIST)))
vpath %.c $(sort $(dir $(SOURCE_C_LIST)))

CC = arm-none-eabi-gcc -c 
AS = arm-none-eabi-gcc -c -x assembler-with-cpp
LD = arm-none-eabi-ld 
HX = arm-none-eabi-objcopy -O ihex

MCU = -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard

FLAGS_DEBUG = -g3 $(OPT) -fdebug-prefix-map=$(CURDIR)=.

FLAGS_AS =
FLAGS_AS += $(MCU)
ifeq ($(DEBUG_ENABLED), 1)
	FLAGS_AS += $(FLAGS_DEBUG)
endif
ifeq ($(BENCH), 1) 
	FLAGS_AS += -DBENCH=1
endif

FLAGS_CC =
FLAGS_CC += $(MCU)
FLAGS_CC += -MMD -MP
FLAGS_CC += -fno-builtin
FLAGS_CC += -DBENCH_TAG='"$(OPT) bench=$(BENCH) irq=$(BENCH_IRQ)"'
FLAGS_CC += -Wall -Wextra -std=gnu11 -ffreestanding
FLAGS_CC += $(FLAGS_INCLUDE)
ifeq ($(DEBUG_ENABLED), 1)
	FLAGS_CC += $(FLAGS_DEBUG)
endif
ifeq ($(BENCH), 1)
	FLAGS_CC += -DBENCH=1 -fno-omit-frame-pointer
endif
ifeq ($(BENCH_IRQ), 1)
	FLAGS_CC += -DBENCH_IRQ=1
endif

LINKER_SCRIPT = linker_script.ld

all : $(BUILD_DIR)/$(PROGRAM).hex $(BUILD_DIR)/$(PROGRAM).elf

$(BUILD_DIR)/$(PROGRAM).hex : $(BUILD_DIR)/$(PROGRAM).elf
	$(HX) $(<) $(@)

$(BUILD_DIR)/$(PROGRAM).elf : $(OBJECTS_LIST) makefile
	$(LD) -T $(LINKER_SCRIPT) -o $(@) $(OBJECTS_LIST)

$(BUILD_DIR)/%.o : %.S makefile | $(BUILD_DIR)
	$(AS) $(FLAGS_AS) -o $(@) $(<)

$(BUILD_DIR)/%.o : %.c makefile | $(BUILD_DIR)
	$(CC) $(FLAGS_CC) -o $(@) $(<)

$(BUILD_DIR) :
	mkdir $(@)

-include $(wildcard $(BUILD_DIR)/*.d)

clean : 
	rm -rf $(BUILD_DIR)
