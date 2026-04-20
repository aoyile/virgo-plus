TEMPLATE = app
CONFIG -= qt

CFLAGS += -std=c89 -pedantic -Wall -O3 -nostdlib -fno-asynchronous-unwind-tables -fno-builtin -fno-ident -ffunction-sections -fdata-sections -I$(INCLUDES)
LIBS   += -lgdi32 -luser32 -lshell32 -lkernel32
LDFLAGS += -static -nostdlib -fno-builtin -s -Wl,-e,__main,--gc-sections,-subsystem,windows $(LIBS)

SOURCES += \
        virgo.c
