override CFLAGS += $(shell pkg-config --cflags gtk+-3.0 json-glib-1.0) -Wall -Wextra -Wpedantic -Wconversion -Wno-keyword-macro -Wno-gnu-auto-type
override LDLIBS += $(shell pkg-config --libs gtk+-3.0 json-glib-1.0 libcurl)

all: main
.PHONY: all

run: main
	./main
.PHONY: run

compile_flags.txt: makefile
	$(file >$@)
	$(foreach O,$(CFLAGS),$(file >>$@,$O))
