# Phase 2: Secure Distribution

Takes the locked `paper.scbt` from Phase 1 and sends it down Board -> States -> Centres.

## Planned structure

```
common/
  Crypto.h              SHA-256, AES-256, combineKeys()          (not started)
phase2-distribution/
  LCRSTree.h            Board/State/Centre tree + BFS broadcast  (not started)
  AVLRegistry.h         registry of authorised nodes             (not started)
  AuditLog.h            SHA-256 hash-chained transfer log        (not started)
  main.cpp              build tree, broadcast, print log, tamper test (not started)
```

## Workflow

1. Load `paper.scbt` and the root key from Phase 1.
2. Register every state and centre in the AVL registry.
3. Build the LCRS tree (Board -> states -> centres).
4. BFS broadcast. At each hop:
   - check the receiver is in the registry
   - derive the receiver's key from its parent's key
   - wrap the package with that key
   - append an entry to the hash-chained log
5. Each centre builds its decrypt key from state key + centre key.
6. Run `verifyChain()` on the log. A tampered entry must be detected.

## Build order

1. `Crypto.h` (SHA-256 first)
2. `AuditLog.h`
3. `AVLRegistry.h`
4. `LCRSTree.h`
5. `main.cpp` and tamper test
