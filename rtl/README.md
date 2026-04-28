# RTL

This directory contains a 64-bit AXI4-Stream packet formatter.

`axis_packet_formatter_64b.sv` emits fixed 32-byte records:

- beat 0: timestamp
- beat 1: payload bytes 0–7
- beat 2: payload bytes 8–15
- beat 3: payload bytes 16–23

The output stream feeds a DMA controller that writes records into memory.

This module does not implement the DMA controller. It defines the formatter stage and record layout that match the C `packet_t` used by the SPSC benchmark.
