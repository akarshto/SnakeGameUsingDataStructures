CC     ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -O2

ifeq ($(OS),Windows_NT)
  TARGET  = snake.exe
  LDLIBS  =
  RM      = del /Q /F
else
  TARGET  = snake
  LDLIBS  = -lncurses
  RM      = rm -f
endif

$(TARGET): snake.c
	$(CC) $(CFLAGS) -o $(TARGET) snake.c $(LDLIBS)

run: $(TARGET)
	./$(TARGET)

clean:
	$(RM) $(TARGET)

.PHONY: run clean
