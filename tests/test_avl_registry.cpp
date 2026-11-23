// test_avl_registry.cpp: Tests phase2-distribution/AVLRegistry.h.
// Walks through a 7-node insertion dry run (printing the root, height, and balance
// after each insertion so we can see rotations happen), then tests O(log n) lookup,
// Registered vs Pending/Rejected filtering, and scaling to all 29 states (ST-01..ST-29).

#include <iostream>                                             // cout, endl for console output
#include <string>                                               // std::string
#include "AVLRegistry.h"                                        // the AVLRegistry class under test
using namespace std;                                            // use standard library names directly

// Helper that inserts one node and prints the tree's root, height, and balance status right after.
void insertAndShow(AVLRegistry& reg, const string& id, const string& name,
                   const string& role, const string& parent, const string& status) {
    reg.registerNode(id, name, role, parent, status);           // insert into the AVL tree (triggers rotation if needed)
    cout << "  Inserted " << id << " (" << name << ", " << status << ")"
         << " -> size=" << reg.size()                           // total nodes so far
         << ", height=" << reg.height()                         // current tree height
         << ", root=" << reg.rootId()                           // who is at the top after any rotation
         << ", balanced=" << (reg.isBalanced() ? "YES" : "NO")  // check AVL rule (-1, 0, +1)
         << endl;
}

int main() {
    cout << "===== AVL REGISTRY DRY RUN & TEST =====" << endl;

    AVLRegistry reg;                                            // start with an empty AVL tree

    // Step 1: Insert 7 nodes in an order that triggers RR, LL, LR, and RL rotations
    cout << "\n--- Step-by-step insertions ---" << endl;
    insertAndShow(reg, "ST-01", "Andhra Pradesh",    "State",  "BOARD", "Registered"); // 1st node: becomes root
    insertAndShow(reg, "ST-02", "Arunachal Pradesh", "State",  "BOARD", "Registered"); // goes right of ST-01
    insertAndShow(reg, "ST-03", "Assam",             "State",  "BOARD", "Registered"); // RR imbalance at ST-01 -> rotateLeft -> ST-02 is new root!
    insertAndShow(reg, "ST-05", "Chhattisgarh",      "State",  "BOARD", "Registered"); // goes right of ST-03
    insertAndShow(reg, "ST-04", "Bihar",             "State",  "BOARD", "Registered"); // RL imbalance at ST-03 -> rotateRight(ST-05) + rotateLeft(ST-03)
    insertAndShow(reg, "BOARD", "Examination Board", "Board",  "",      "Registered"); // goes left of ST-01 -> LL/LR rebalance at root!
    insertAndShow(reg, "ST-01-C01", "AP Centre 1",   "Centre", "ST-01", "Pending");    // goes right of ST-01 -> LR rebalance!

    // Also add one Rejected centre to test status filtering
    insertAndShow(reg, "ST-02-C99", "Rogue Centre",  "Centre", "ST-02", "Rejected");

    // Step 2: Check that in-order traversal returns nodes in strict alphabetical order
    cout << "\n--- In-order traversal (alphabetical by nodeId) ---" << endl;
    for (const NodeInfo& n : reg.getAllNodes()) {               // Left -> Root -> Right traversal
        cout << "  " << n.nodeId << " | " << n.name << " | role=" << n.role
             << " | status=" << n.status << endl;
    }

    // Step 3: Verify authorization checks (Registered = allowed, Pending/Rejected/Unknown = blocked)
    cout << "\n--- Authorization checks ---" << endl;
    bool okBoard   = reg.isRegistered("BOARD");                 // Registered -> expect true
    bool okSt01    = reg.isRegistered("ST-01");                 // Registered -> expect true
    bool okPending = reg.isRegistered("ST-01-C01");             // Pending    -> expect false
    bool okReject  = reg.isRegistered("ST-02-C99");             // Rejected   -> expect false
    bool okUnknown = reg.isRegistered("ST-99-FAKE");            // Not in tree -> expect false

    cout << "  isRegistered(\"BOARD\")      = " << (okBoard   ? "true  (PASS)" : "false (FAIL)") << endl;
    cout << "  isRegistered(\"ST-01\")      = " << (okSt01    ? "true  (PASS)" : "false (FAIL)") << endl;
    cout << "  isRegistered(\"ST-01-C01\")  = " << (!okPending ? "false (PASS - Pending blocked)"  : "true (FAIL)") << endl;
    cout << "  isRegistered(\"ST-02-C99\")  = " << (!okReject  ? "false (PASS - Rejected blocked)" : "true (FAIL)") << endl;
    cout << "  isRegistered(\"ST-99-FAKE\") = " << (!okUnknown ? "false (PASS - Unknown blocked)"  : "true (FAIL)") << endl;

    if (!okBoard || !okSt01 || okPending || okReject || okUnknown || !reg.isBalanced()) {
        cout << "TEST FAILED" << endl;
        return 1;                                               // non-zero exit if any check failed
    }

    // Step 4: Insert all 29 states in sorted order ("ST-01" .. "ST-29") to prove height stays O(log n)
    AVLRegistry fullStates;                                     // second tree for the 29-state stress test
    for (int i = 1; i <= 29; i++) {                             // 1 to 29 in ascending order (worst case for a normal BST)
        string id = (i < 10) ? ("ST-0" + to_string(i)) : ("ST-" + to_string(i));
        fullStates.registerNode(id, "State " + to_string(i), "State", "BOARD", "Registered");
    }
    cout << "\n--- 29-state sorted insertion check ---" << endl;
    cout << "  Nodes: " << fullStates.size()
         << " | AVL height: " << fullStates.height() << " (normal BST would be 29)"
         << " | Balanced: " << (fullStates.isBalanced() ? "YES" : "NO") << endl;

    if (fullStates.size() != 29 || fullStates.height() > 6 || !fullStates.isBalanced()) {
        cout << "TEST FAILED: 29-state tree not properly balanced" << endl;
        return 1;
    }

    cout << "\nALL AVL REGISTRY TESTS PASSED" << endl;
    return 0;                                                   // 0 = all tests passed
}
