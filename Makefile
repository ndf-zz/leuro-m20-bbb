TARGET = leuro-m20-bbb

CFLAGS = -Wall -Werror -O3

.PHONY: all
all:	$(TARGET)

.PHONY: clean
clean:
	-rm -f $(TARGET)
