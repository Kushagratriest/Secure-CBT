# Secure CBT

### Hierarchical Key-Chained Question Paper Delivery & Leak-Traceability System

Secure CBT is a Computer-Based Test (CBT) security system designed to protect question papers throughout their lifecycle — from paper creation and distribution to the final exam centre.

The system combines cryptography, data structures, compression algorithms, graph algorithms, and tamper-evident logging to make unauthorized question-paper access difficult and enable the source of a leaked encrypted paper to be traced back to its assigned node.

---

## Problem Statement

Question-paper leaks can result in cancelled or delayed examinations, financial losses, legal complications, and reduced trust in examination systems.

Existing CBT security mechanisms already use techniques such as encrypted storage, candidate authentication, time-locked decryption, and controlled distribution.

However, when the same encrypted question-paper copy is distributed to multiple locations, identifying the origin of a leaked ciphertext can require reconstructing information from external access logs and records.

Secure CBT approaches this problem by giving each node in the distribution hierarchy a uniquely derived encrypted version of the question paper.

This makes traceability part of the encryption and distribution architecture rather than an entirely separate post-incident process.

---

## Key Objectives

The system is designed around three primary objectives:

- **Prevention** — Protect question papers during creation, distribution, and examination.
- **Fairness** — Ensure every authorized examination node receives the same set of questions.
- **Traceability** — Identify the centre or terminal associated with a leaked encrypted paper.

---

## System Overview

Secure CBT is divided into three major phases:

### Phase 1 — Paper Creation

1. Teachers submit questions to the trusted central server.
2. The `PaperGenerator` selects questions based on parameters such as:
   - Topic
   - Difficulty
   - Marks weightage
3. The selected questions are assembled into a byte stream.
4. Huffman coding is used to compress the assembled paper.
5. Compression is performed before encryption to reduce the size of data replicated throughout the distribution hierarchy.

### Phase 2 — Secure Distribution

The compressed question paper is distributed through a hierarchical network.

Each node receives a uniquely encrypted version generated through hierarchical key chaining.

The distribution layer uses:

- LCRS tree representation
- Breadth-First Search (BFS)
- Hierarchical key chaining
- AES encryption
- AVL tree node registry
- SHA-256 hash-chained audit logging
- Dijkstra's shortest-path algorithm
- Minimum Spanning Tree (MST)

The hierarchical structure ensures that data moves from parent nodes toward child nodes without reverse flow.

### Phase 3 — Examination Centre

At the examination centre:

1. The centre stores its encrypted question paper.
2. Decryption remains locked until the scheduled examination time.
3. Time-based unlocking prevents premature access.
4. If an encrypted paper is leaked, its ciphertext can be matched against the node registry to determine its assigned centre or terminal.

---

## Architecture

```text
                    ┌─────────────────────────┐
                    │     Central Server      │
                    │                         │
                    │ Question Bank           │
                    │ Paper Generator         │
                    │ Paper Assembly          │
                    └────────────┬────────────┘
                                 │
                                 ▼
                    ┌─────────────────────────┐
                    │   Huffman Compression   │
                    └────────────┬────────────┘
                                 │
                                 ▼
                 ┌───────────────────────────────┐
                 │   Hierarchical Distribution   │
                 │                               │
                 │       Root Server             │
                 │            │                  │
                 │      ┌─────┴─────┐            │
                 │      ▼           ▼            │
                 │   State A     State B         │
                 │      │           │            │
                 │   Districts   Districts       │
                 │      │           │            │
                 │   Centres     Centres         │
                 └──────┬───────────┬────────────┘
                        │           │
                        ▼           ▼
                 ┌───────────────────────┐
                 │   Exam Centre/        │
                 │   Student Terminal    │
                 │                       │
                 │   Time-Locked Access  │
                 └───────────┬───────────┘
                             │
                             ▼
                  ┌──────────────────────┐
                  │   Leak Traceability  │
                  │                      │
                  │   AVL Node Registry  │
                  │   Ciphertext Match   │
                  └──────────────────────┘
```

---

## Core Technologies

| Technology / Algorithm | Purpose |
|---|---|
| **C++17** | Core implementation |
| **AES-256** | Question-paper encryption |
| **SHA-256** | Tamper-evident audit chain |
| **Huffman Coding** | Question-paper compression |
| **LCRS Tree** | Representation of the N-ary distribution hierarchy |
| **BFS** | Level-by-level paper distribution |
| **AVL Tree** | Fast node and secret lookup |
| **Dijkstra's Algorithm** | Secure route selection |
| **Minimum Spanning Tree** | Efficient network backbone design |
| **Min-Heap** | Huffman construction and time-slot management |
| **Git/GitHub** | Version control and collaboration |

---

## Data Structures

### 1. LCRS Tree

The distribution hierarchy is logically an N-ary tree.

It is represented using the **Left-Child Right-Sibling (LCRS)** technique:

```text
Node
 ├── firstChild
 └── nextSibling
```

This allows a node to have an arbitrary number of children while using only two structural pointers per node.

---

### 2. AVL Tree

An AVL tree acts as the node registry.

Each registered node can be associated with information such as:

- Node ID
- Secret
- Assigned key
- Current status

Because the tree remains balanced, node lookup operates in:

```text
O(log n)
```

---

### 3. Hash-Chained Audit Log

Every transfer event is recorded in a linked structure.

Conceptually:

```text
Entry 1
   │
   ▼
Entry 2
   │
   ▼
Entry 3
   │
   ▼
Entry 4
```

Each entry contains the SHA-256 hash of the previous entry.

If an earlier record is modified, subsequent hashes no longer match, allowing tampering to be detected.

---

## Cryptographic Model

Secure CBT uses a hierarchical key-chaining approach.

A node's encrypted paper depends on:

```text
Parent Key + Node Secret
```

This creates a dependency between successive levels of the distribution hierarchy.

Conceptually:

```text
Root
 │
 ├── State Key
 │      │
 │      ├── District Key
 │      │       │
 │      │       └── Centre Key
 │      │
 │      └── District Key
 │
 └── State Key
        │
        └── ...
```

The design prevents a lower-level node from independently reconstructing the encryption state without the required parent-level information.

---

## Leak Traceability

One of the main features of Secure CBT is that every distribution node receives a uniquely derived encrypted copy.

If a ciphertext becomes publicly available:

```text
Leaked Ciphertext
        │
        ▼
   Node Registry
        │
        ▼
   Matching Node ID
        │
        ▼
Centre / Terminal
```

This allows the system to determine which registered node the leaked ciphertext was assigned to.

The design aims to perform this lookup through the AVL registry rather than relying entirely on external access logs.

---

## Time-Locked Decryption

Question papers remain encrypted at examination centres until the scheduled examination time.

The centre can therefore possess the encrypted paper without being able to access the plaintext prematurely.

For multiple examination slots, a min-heap can be used to manage the earliest scheduled unlocking event.

```text
          Exam Slots
              │
          Min-Heap
              │
       ┌──────┴──────┐
       ▼             ▼
   Earliest       Later Slots
     Slot
       │
       ▼
   Decryption
```

---

## Network Layer

The project also models the physical network connecting distribution nodes using a weighted graph.

### Dijkstra's Algorithm

Dijkstra's algorithm is used for route selection between adjacent nodes in the hierarchy.

### Minimum Spanning Tree

An MST is used to design an efficient backbone connecting state-level servers while minimizing the overall connection cost.

This provides an algorithmic representation of both routing and network infrastructure considerations.

---

## Project Structure

```text
Secure-CBT/
│
├── include/
│   ├── QuestionBank.h
│   ├── PaperGenerator.h
│   ├── Huffman.h
│   ├── LCRSTree.h
│   ├── AVLRegistry.h
│   ├── Encryption.h
│   ├── AuditLog.h
│   ├── Graph.h
│   └── TimeLock.h
│
├── src/
│   ├── QuestionBank.cpp
│   ├── PaperGenerator.cpp
│   ├── Huffman.cpp
│   ├── LCRSTree.cpp
│   ├── AVLRegistry.cpp
│   ├── Encryption.cpp
│   ├── AuditLog.cpp
│   ├── Graph.cpp
│   └── TimeLock.cpp
│
├── tests/
│   ├── test_questions.cpp
│   ├── test_huffman.cpp
│   ├── test_tree.cpp
│   ├── test_encryption.cpp
│   └── test_audit.cpp
│
├── docs/
│   ├── architecture/
│   └── diagrams/
│
├── data/
│   └── sample_questions/
│
├── CMakeLists.txt
├── README.md
└── LICENSE
```

> The directory structure above is a recommended organization for the implementation and can be adjusted to match the actual repository.

---

## Example Workflow

```text
Teacher
   │
   ▼
Question Bank
   │
   ▼
Paper Generator
   │
   ▼
Random Question Selection
   │
   ▼
Paper Assembly
   │
   ▼
Huffman Compression
   │
   ▼
Root Encryption
   │
   ▼
Hierarchical Distribution
   │
   ├── State Server
   │      ├── District
   │      │      └── Centre
   │      │             └── Terminal
   │      │
   │      └── District
   │
   ▼
Time-Locked Decryption
   │
   ▼
Examination
```

---

## Security Features

- Hierarchical question-paper distribution
- Unique ciphertext per distribution node
- AES-256 encryption
- SHA-256 integrity verification
- Hash-chained audit records
- Time-locked question-paper access
- Controlled parent-to-child distribution
- AVL-based node registry
- Leak-source identification
- Tamper detection
- Graph-based network routing

---

## Project Status

The project is currently under development.

### Completed

- [x] Hierarchical key-chaining design
- [x] Initial class/data-structure design
- [x] `QuestionBank` class
- [x] `Teacher` class
- [x] `PaperGenerator` class

### In Progress

- [ ] Huffman compression

### Planned

- [ ] LCRS distribution tree
- [ ] BFS broadcast
- [ ] AVL node registry
- [ ] Hierarchical encryption
- [ ] AES-256 integration
- [ ] SHA-256 hash-chained audit log
- [ ] Time-locked decryption
- [ ] Dijkstra-based route selection
- [ ] MST-based network backbone
- [ ] End-to-end CBT simulation
- [ ] Tamper-detection testing
- [ ] Leak-tracing demonstration

---

## Testing & Demonstration

The completed system is intended to demonstrate:

### 1. End-to-End Distribution

A question paper is generated and distributed through multiple hierarchical levels.

### 2. Tamper Detection

A transfer-log entry is modified and the hash chain is checked for integrity.

Expected behavior:

```text
Original Chain
     │
     ▼
Valid Hashes
     │
     ▼
Integrity: PASS
```

After modification:

```text
Modified Entry
     │
     ▼
Hash Mismatch
     │
     ▼
Integrity: FAIL
```

### 3. Leak Tracing

A simulated leaked ciphertext is provided to the system.

```text
Leaked Ciphertext
        │
        ▼
AVL Registry Lookup
        │
        ▼
Node Identification
        │
        ▼
Centre / Terminal
```

---

## Requirements

- C++17 or later
- Standard C++ build tools
- OpenSSL or equivalent cryptographic library
- Git
- CMake (recommended)
- Code editor / IDE
- Sample question datasets

---

## Building the Project

```bash
git clone <repository-url>
cd Secure-CBT
```

If using CMake:

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

Run:

```bash
./secure_cbt
```

> Build commands may change depending on the final implementation.

---

## Design Philosophy

Secure CBT treats question-paper security as a **systems problem rather than a single encryption problem**.

Instead of protecting only the final question-paper file, the architecture considers three stages:

```text
Paper Creation
      ↓
Distribution
      ↓
Exam Centre
```

Security and traceability mechanisms are integrated into each stage.

The central design principle is:

> **Prevent leaks where possible, and make the origin of a leak identifiable when prevention fails.**

---

## Assumptions & Limitations

The simulation assumes:

- Network connections between hierarchy levels are dependable.
- Node identities and secrets are securely provisioned before the examination cycle.
- Secret values are not transmitted alongside encrypted papers.
- The central server and root key remain trusted.
- Devices have reasonably synchronized clocks for time-based unlocking.

A compromise of the root server or root key is outside the scope of the current simulation.

---

## Team

### Secure CBT

**Team:** DSCPP-III-2026-T145  
**Semester:** 3rd  
**Mentor:** Mr. Kartikey Arora

| Member | Role |
|---|---|
| Naman | Team Lead |
| Aayush Gupta | Team Member |
| Kushagra Bhushan | Team Member |

---

## Academic Project

This project is being developed as a **BTech CSE project** focusing on:

- Data Structures
- Algorithms
- Object-Oriented Programming
- Cryptography
- Network Modeling
- System Security

---

## References

1. Newslaundry — *10 years, 89 paper leak cases, 48 retests: From centre to states, few plugs for a leaky record*
2. Wikipedia — *List of paper leaks in India*
3. The Wire — *India's Exam Fraud Bubble: 148 Cases, One Conviction in 11 Years*
4. Cormen, Leiserson, Rivest, Stein — *Introduction to Algorithms*
5. NIST FIPS 197 — Advanced Encryption Standard (AES)
6. NIST FIPS 180-4 — Secure Hash Standard (SHS)

---

## Disclaimer

Secure CBT is an academic simulation and prototype. It is intended to demonstrate the application of data structures, algorithms, cryptographic primitives, and secure distribution concepts to a CBT question-paper delivery system.

It is not intended to replace production-grade examination infrastructure or independently audited cryptographic systems.
