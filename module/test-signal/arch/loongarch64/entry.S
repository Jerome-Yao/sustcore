    .text
    .global _start
    .type _start, @function
    .extern test_signal_main

_start:
    b test_signal_main
