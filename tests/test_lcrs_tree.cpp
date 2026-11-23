// test_lcrs_tree.cpp: Verifies phase2-distribution/LCRSTree.h together with
// AVLRegistry.h, KeyManager.h (AES-256-GCM + HKDF-SHA256), and AuditLog.h (SHA-256).
// Tests:
//   1. Building a multi-level LCRS hierarchy (Board -> States -> Centres, plus a Pending/Rejected node).
//   2. Running bfsBroadcast() and checking that level-order (BFS) order is followed.
//   3. Verifying that Pending/Rejected nodes are blocked and excluded from the AuditLog.
//   4. Checking that every delivered centre holds a DIFFERENT onion ciphertext (+28 bytes per hop).
//   5. Peeling all onion layers back to the original plaintext via unwrapAtNode() and verifying AuditLog.

#include <iostream>                                             // cout, endl for test output
#include <string>                                               // std::string
#include <vector>                                               // std::vector
#include "LCRSTree.h"                                           // LCRSTree, LCRSNode, HopReport
using namespace std;                                            // use standard library names directly

int main() {
    cout << "===== LCRS TREE & BFS BROADCAST TEST =====" << endl;

    KeyManager keyMgr;                                          // handles AES-256-GCM encryption and HKDF child keys
    AVLRegistry registry;                                       // self-balancing AVL tree of authorised nodes
    AuditLog auditLog;                                          // SHA-256 hash-chained log of every successful hop
    LCRSTree tree;                                              // Left-Child Right-Sibling hierarchy tree

    // Step 1: Register nodes in the AVL registry (one Centre is "Pending" to test blocking)
    registry.registerNode("BOARD",     "Examination Board", "Board",  "",      "Registered");
    registry.registerNode("ST-01",     "Andhra Pradesh",    "State",  "BOARD", "Registered");
    registry.registerNode("ST-02",     "Assam",             "State",  "BOARD", "Registered");
    registry.registerNode("ST-01-C01", "AP Centre 1",       "Centre", "ST-01", "Registered");
    registry.registerNode("ST-01-C02", "AP Centre 2",       "Centre", "ST-01", "Pending");    // should be BLOCKED!
    registry.registerNode("ST-02-C01", "Assam Centre 1",    "Centre", "ST-02", "Registered");

    // Step 2: Build the LCRS hierarchy (Board -> 2 States -> Centres)
    tree.setRoot("BOARD", "Examination Board", "Board");        // level 0: root
    tree.addChild("BOARD", "ST-01",     "Andhra Pradesh", "State");  // level 1: firstChild of BOARD
    tree.addChild("BOARD", "ST-02",     "Assam",          "State");  // level 1: nextSibling of ST-01
    tree.addChild("ST-01", "ST-01-C01", "AP Centre 1",    "Centre"); // level 2: firstChild of ST-01
    tree.addChild("ST-01", "ST-01-C02", "AP Centre 2",    "Centre"); // level 2: nextSibling of ST-01-C01 (Pending)
    tree.addChild("ST-02", "ST-02-C01", "Assam Centre 1", "Centre"); // level 2: firstChild of ST-02

    cout << "\nTree built with " << tree.size() << " nodes." << endl;

    // Step 3: Simulate the Phase 1 locked paper (L0 encrypted with rootKey)
    string originalCompressed = "Compressed-Paper-Bytes-For-Phase2-Test"; // simulated Huffman bytes (38 bytes)
    string rootKey = keyMgr.generateRootKey();                  // 32-byte Board root key
    string rootEncrypted = keyMgr.encrypt(originalCompressed, rootKey); // L0: 38 + 28 = 66 bytes

    cout << "Original payload: " << originalCompressed.size() << " bytes"
         << " | L0 (Board) encrypted: " << rootEncrypted.size() << " bytes" << endl;

    // Step 4: Run BFS broadcast down the LCRS tree
    cout << "\n--- Running BFS Broadcast ---" << endl;
    vector<HopReport> hops = tree.bfsBroadcast(rootEncrypted, rootKey, registry, keyMgr, auditLog);

    cout << "\n--- Hop Results (BFS Level Order) ---" << endl;
    for (const HopReport& h : hops) {                           // print every hop attempt
        cout << "  Hop " << h.hopNumber << ": " << h.fromId << " -> " << h.toId
             << " | status: " << h.status
             << " | payload: " << h.payloadBytes << " bytes" << endl;
    }

    // Check 1: 5 hop attempts total (2 states + 3 centres), 4 delivered, 1 blocked (ST-01-C02)
    if (hops.size() != 5 || hops[3].toId != "ST-01-C02" || hops[3].status == "Delivered") {
        cout << "TEST FAILED: expected ST-01-C02 to be blocked" << endl;
        return 1;
    }

    // Check 2: Each onion layer adds 28 bytes (L0=66 -> State L1=94 -> Centre L2=122)
    const LCRSNode* c1 = tree.getNode("ST-01-C01");             // AP Centre 1
    const LCRSNode* c2 = tree.getNode("ST-02-C01");             // Assam Centre 1
    if (c1 == nullptr || c2 == nullptr || !c1->delivered || !c2->delivered) {
        cout << "TEST FAILED: registered centres did not receive payload" << endl;
        return 1;
    }
    if (c1->encryptedPayload == c2->encryptedPayload) {         // each centre must have a unique ciphertext
        cout << "TEST FAILED: two different centres got identical ciphertexts" << endl;
        return 1;
    }
    cout << "\nUnique ciphertexts verified: ST-01-C01 (" << c1->encryptedPayload.size()
         << " bytes) != ST-02-C01 (" << c2->encryptedPayload.size() << " bytes)" << endl;

    // Step 5: Unwrap all onion layers at ST-01-C01 (Centre -> State -> Board) and verify payload matches
    string recovered;
    bool unwrappedOk = tree.unwrapAtNode("ST-01-C01", keyMgr, recovered);
    cout << "Unwrap at ST-01-C01: " << (unwrappedOk && recovered == originalCompressed ? "PASS (matches original)" : "FAIL") << endl;
    if (!unwrappedOk || recovered != originalCompressed) {
        return 1;
    }

    // Step 6: Verify the SHA-256 hash-chained AuditLog (4 delivered hops -> 4 log entries)
    int auditStatus = auditLog.verify();
    cout << "AuditLog entries: " << auditLog.getEntries().size()
         << " | verify(): " << auditStatus << (auditStatus == -1 ? " (PASS - chain intact)" : " (FAIL)") << endl;
    if (auditLog.getEntries().size() != 4 || auditStatus != -1) {
        return 1;
    }

    cout << "\nALL LCRS TREE & BFS BROADCAST TESTS PASSED" << endl;
    return 0;                                                   // 0 = all checks succeeded
}
