CC=gcc
CFLAGS=-std=c11 -O2 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wformat=2 -Wundef -Iinclude
LDFLAGS=-pthread -lrt

OBJDIR=obj
SRCDIR=src
TARGET=dialog

OBJS=$(OBJDIR)/dialog.o $(OBJDIR)/utils.o $(OBJDIR)/threads.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(TARGET)

.PHONY: all clean
