CFLAGS = $(shell pkg-config --cflags gtk+-3.0 json-glib-1.0) -O3 -Wall -Wextra
LDFLAGS = $(shell pkg-config --libs gtk+-3.0 json-glib-1.0 libcurl)

all: main
.PHONY: all

run: main
	./main
.PHONY: all

compile_flags.txt:
	$(file >$@)
	$(foreach O,$(CFLAGS),$(file >>$@,$O))
