#!/bin/bash
set -eo pipefail
cd "$(dirname "$0")"

echo "Compiling and running test_wire_encode..."
gcc -Wall -Werror -I include -o tests/test_wire_encode tests/test_wire_encode.c src/pl_encode.c
./tests/test_wire_encode

echo "Compiling and running test_wire_decode..."
gcc -Wall -Werror -I include -o tests/test_wire_decode tests/test_wire_decode.c src/pl_decode.c src/pl_encode.c
./tests/test_wire_decode

echo "All tests passed."
