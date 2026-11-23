# Secure CBT: Task List and Frontend Plan

Status key: **Done** = in the repo and checked by compiling/running code ·
**Partial** = exists but has a known problem · **Not started** = nothing in
the repo yet · **Can't verify** = lives outside the repo (Figma, report, PPT)

---

## Part 1: Task list

### Phase 1: Paper Setting

| # | Task | Status |
|---|------|--------|
| 1.1 | Question, QuestionBank, TopicIndex, PaperGenerator classes | Done (`main.cpp` compiles and runs through Phase 1) |
| 1.2 | Huffman compression | Partial (works, but output is '0'/'1' text, see 1.9) |
| 1.3 | Key manager (XOR key derivation, encrypt/decrypt) | Done (XOR only, see C.4) |
| 1.4 | Question-bank upload screen (Figma design) | Can't verify design; not built in code |
| 1.5 | CSV checker: `checkCsv()` in `csvchecker.h` | Done (passes scratch test program) |
| 1.6 | Add `subject` to `Question` | Done (`main.cpp` still compiles) |
| 1.7 | Wire `main.cpp` to call `checkCsv()` on a real file | Not started |
| 1.8 | Formal test logging for CSV checker (in the repo) | Not started (scratch test exists outside repo) |
| 1.9 | Huffman: pack bits instead of '0'/'1' text | Not started |
| 1.10 | Huffman: single-character input gets an empty code | Not started |
| 1.11 | Phase 1 end-to-end test (CSV to encrypted paper) | Not started |

### Phase 2: Secure Distribution

| # | Task | Status |
|---|------|--------|
| 2.1 | LCRS tree and BFS broadcast | Not started |
| 2.2 | AVL node registry | Not started |
| 2.3 | XOR key-chaining with AES-256 per node | Not started (XOR `deriveChildKey` exists, no AES) |
| 2.4 | SHA-256 hash-chained transfer log | Not started |
| 2.5 | Transfer layer between nodes (simulated in-process first) | Not started |
| 2.6 | Phase 2 tests (broadcast, key chain, tamper detection) | Not started |

### Phase 3: Exam Centre

| # | Task | Status |
|---|------|--------|
| 3.1 | Time-locked decryption at the centre | Not started |
| 3.2 | Per-student shuffle | Not started |
| 3.3 | Multi-centre simulation | Not started |
| 3.4 | Leak-tracing demonstration (AVL lookup) | Not started |

### Report and presentation

| # | Task | Status |
|---|------|--------|
| R.1 | Phase 2 report (LaTeX): compile check and plagiarism/AI check | Can't verify (not in repo) |
| R.2 | Milestone table update once 1.5 to 1.8 pass | Waiting on 1.7 and 1.8 |
| R.3 | Commit to git: `AGENTS.md`, `TODO_AND_FRONTEND.md`, `csvchecker.h`, `Question.h` change | Not started (all uncommitted) |
| R.4 | Presentation (PPT) | Can't verify (`System_Architecture_Deck.pptx` exists, not reviewed) |
| R.5 | Final documentation | Not started |

### Cleanup (low priority)

| # | Task | Status |
|---|------|--------|
| C.1 | Delete `temp.cpp` (scratch file, never committed to git) | Not started |
| C.2 | Settle the `csvchecker.h` filename casing before anyone includes it | Not started |
| C.3 | Free Huffman tree nodes (memory leak) | Not started |
| C.4 | Replace XOR "encryption" with AES-256 (OpenSSL) | Not started |
| C.5 | Decide whether the compiled `demo` binary belongs in the repo (untracked now) | Not started |

---

## Part 2: Frontend plan (Figma to code)

### How the UI is built

1. **Design in Figma first.** Each screen is a frame at desktop width (1440px). The Phase 1 upload screen has four states: empty, file selected, success, and error (plus the header-only case).
2. **Name the states clearly in Figma** (`Upload / Empty`, `Upload / Selected`, `Upload / Success`, `Upload / Error`), since the code will map to these.
3. **Reuse components.** The button, card, and drop zone should be Figma components, so a change in one place updates every screen.
4. **Developer handoff.** Use Figma's Dev Mode to read exact spacing, colours, and font sizes. Keep the 8px spacing grid.
5. **Build in code.** Turn each Figma component into HTML/CSS (or a React component). Start with the static layout, then add the logic.
6. **Connect to the backend.** The frontend uploads the CSV, and the backend (`checkCsv`) returns the result. The UI then shows the state that matches the result:
   - `!isValid()` → error table
   - `isEmptyBank()` → "No questions found"
   - `canContinue()` → success with the count

### Open decision: how the frontend talks to C++

The web page cannot call a C++ function directly. Options:

- **A. Simple:** the frontend runs the CSV check in JavaScript for quick feedback, and the C++ `checkCsv()` is the authority when the file reaches the backend.
- **B. Full:** a small C++ HTTP server (for example cpp-httplib) exposes `/upload`, which returns JSON from `CheckResult`. More work, but the real architecture.
- **C. Demo only:** the C++ program reads a file path from the command line, and the UI shows the same states using a static JSON copy of the result.

Decide between A, B and C before building the upload flow. The report describes the system as C++ core with a web frontend, so B is the honest match if there's time; A or C is fine if you clearly state what the frontend does.

### Next steps (in order)

1. Huffman fixes: 1.9 and 1.10.
2. Wire `main.cpp` (1.7) and add formal tests (1.8).
3. Pick the frontend option (A, B or C) and build the upload screen.
4. Commit everything in R.3.
5. Phase 2 starts only after Phase 1 tests pass.
