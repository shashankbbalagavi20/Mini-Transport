# UDP Mini-Transport Layer

A reliable, low-latency UDP transport protocol implemented in C++17. Designed to provide TCP-like reliability (in-order delivery, retransmissions, data integrity) without the latency overhead of a 3-way handshake or strict congestion control.

## Systems Architecture

This project implements a custom reliable transport layer directly over raw POSIX UDP sockets. 

*   **Zero-Allocation Hot Path:** Packet construction and transmission utilize stack-allocated memory and zero-cost copies, avoiding slow heap allocations in the transmission loop.
*   **In-Order Reassembly Engine:** The server utilizes a state machine and a buffer map to catch future packets and seamlessly reassemble shattered data streams.
*   **Sliding Window Protocol:** The client implements an O(1) circular array for managing packet states, sequence numbers, and unacknowledged payloads.
*   **Deterministic Timeout Sweeps:** Non-blocking temporal loops sweep the unacknowledged window and automatically retransmit packets lost in the network.
*   **Hardware-Agnostic Endianness:** Strict adherence to network byte-order translations (`htons`/`ntohs`, `htonl`/`ntohl`) guarantees safe payload execution across different CPU architectures.

## Fault Injection & Chaos Testing

To empirically demonstrate the reliability of the protocol, the system features a built-in deterministic chaos engine:

*   **Client-Side Corruption (20% Probability):** The client intentionally flips bits in the mathematical checksum.
*   **Server-Side Packet Loss (30% Probability):** The server intentionally drops incoming packets.

*Under maximum fault injection, the server successfully rejects all corrupted data via checksum validation, buffers out-of-order sequences, and achieves perfect data reassembly via client timeout retransmissions.*

## Tooling & Memory Safety

*   **Language:** C++17
*   **Compiler:** Clang / Apple LLVM
*   **Memory Profiling:** Hardened and profiled using LLVM AddressSanitizer (`-fsanitize=address`). The hot path and out-of-order buffer are confirmed leak-free.