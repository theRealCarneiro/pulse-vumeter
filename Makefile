include config.mk

PREFIX = /usr/local
SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)

all: options $(EXEC)

options: config.mk
	@echo $(EXEC) build options:
	@echo "CFLAGS   = $(CFLAGS)"
	@echo "LDFLAGS  = $(LDFLAGS)"
	@echo "CC       = $(CC)"

$(EXEC): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

install: $(EXEC)
	install -Dm755 pulse-vumeter $(DESTDIR)/$(PREFIX)/bin/pulse-vumeter

uninstall:
	rm $(DEST_DIR)/$(PREFIX)/bin/pulse-vumeter

clean:
	rm -fv *.o $(EXEC)

mrproper: clean
	rm -fv $(EXEC)

