    .text
    .global _start
    .type _start, @function
    .extern test_signal_main

_start:
    j test_signal_main
