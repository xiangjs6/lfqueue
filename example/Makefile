LIBS = -L ../$(DIST)/lib -l$(LIB_NAME)

SRCS := $(wildcard *.c)
TARGET  := $(patsubst %.c, %, $(SRCS))

.c:
	$(CC) -o $@ $< $(LIBS) $(CFLAGS) -I../$(DIST)/$(INCLUDE)

all: $(TARGET) 

clean:
	rm -rf $(TARGET)
