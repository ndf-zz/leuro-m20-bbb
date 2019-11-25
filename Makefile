TARGET = leuro-m20-bbb

CFLAGS = -pedantic -Wall -Werror -g -O9

.PHONY: all
all:	$(TARGET)

.PHONY: clean
clean:
	-rm -f $(TARGET)
