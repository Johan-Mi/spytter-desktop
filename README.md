# Spytter Desktop

A GTK 3-based desktop Spytter client written in C.

## Features

- Send messages.
- Receive messages.

## Portability

Spytter Desktop has only been tested on Linux.

## Dependencies

- make
- GTK 3
- JSON-GLib
- libcurl with WebSocket support enabled

## Build instructions

- Install the dependencies.
- Set `HOST` in `main.c` to the URL of the Spytter instance you want to use.
- Run `make`.

## Contributing

- Format the code with clang-format.
- If you're using clangd, set it up with `make compile_flags.txt`.
