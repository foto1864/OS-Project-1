# Makefile

CC := gcc
CFLAGS := -std=c11 -O2 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wformat=2 -Wundef -Iinclude
LDFLAGS := -pthread

TARGET := dialog

SRC_DIR := src
OBJ_DIR := obj
INC_DIR := include

SRCS := $(SRC_DIR)/dialog.c $(SRC_DIR)/utils.c $(SRC_DIR)/threads.c
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

.PHONY: all clean dirs

all: dirs $(TARGET)

dirs:
	mkdir -p $(OBJ_DIR)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(INC_DIR)/utils.h $(INC_DIR)/threads.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(TARGET)
