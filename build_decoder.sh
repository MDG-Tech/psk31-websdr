#!/bin/sh
set -eu
cd "$(dirname "$0")"
emcc decoder.c -O3 -std=c11 -Wall -Wextra \
  -s STANDALONE_WASM=1 -s INITIAL_MEMORY=67108864 -s STACK_SIZE=1048576 \
  -Wl,--no-entry -Wl,--export=malloc -Wl,--export=free \
  -Wl,--export=psk_decode -Wl,--export=psk_text -Wl,--export=psk_error \
  -Wl,--export=psk_text_length -Wl,--export=psk_metric -o decoder.wasm
