// phase2-distribution/main.cpp: End-to-end runner for Phase 2 (Secure Distribution).
// Takes the locked paper package (paper.scbt) and root key (paper.key) created in Phase 1,
// registers the distribution network in the self-balancing AVLRegistry, builds the
// Left-Child Right-Sibling (LCRS) hierarchy tree, broadcasts the paper level-by-level
// using BFS (adding one AES-256-GCM onion layer per hop), verifies the SHA-256 AuditLog,
// proves a leaf centre can unwrap all onion layers, and demonstrates tamper detection.
//
// Usage: ./secure_cbt_phase2 [path-to-paper.scbt] [path-to-paper.key]

#include <iostream>                                             // cout, endl for console output
#include <fstream>                                              // ifstream for reading paper.key
#include <string>                                               // std::string
#include <vector>                                               // std::vector
#include "PaperStorage.h"                                       // PaperPackage, loadPaperPackage()
#include "KeyManager.h"                                         // KeyManager (AES-256-GCM + HKDF-SHA256)
#include "AuditLog.h"                                           // AuditLog (SHA-256 hash chain)
#include "AVLRegistry.h"                                        // AVLRegistry (self-balancing AVL tree)
#include "LCRSTree.h"                                           // LCRSTree (LCRS hierarchy + BFS broadcast)
using namespace std;                                            // use standard library names directly

// Reads the 32-byte root key from 'path' into 'outKey'. Returns true on success.
bool loadRootKeyFile(const string& path, string& outKey, string& error) {
    ifstream in(path, ios::binary);                             // open the key file in binary mode
    if (!in.is_open()) {                                        // file missing or unreadable
        error = "Could not open " + path + " (run Phase 1 first)";
        return false;                                           // report failure
    }
    outKey.assign((istreambuf_iterator<char>(in)),              // read every byte of the file into outKey
                  istreambuf_iterator<char>());
    if ((int)outKey.size() != AES_KEY_BYTES) {                  // AES-256 root key must be exactly 32 bytes
        error = path + " must be exactly " + to_string(AES_KEY_BYTES) + " bytes";
        return false;                                           // reject wrong-sized key file
    }
    return true;                                                // 32-byte key loaded cleanly
}

// Helper to add a node to BOTH the AVLRegistry and the LCRSTree in one step.
void addNetworkNode(AVLRegistry& reg, LCRSTree& tree, const string& parentId,
                    const string& nodeId, const string& name,
                    const string& role, const string& status = "Registered") {
    reg.registerNode(nodeId, name, role, parentId, status);     // insert into the self-balancing AVL registry
    if (parentId.empty()) {                                     // empty parentId means this is the top-level Board
        tree.setRoot(nodeId, name, role);                       // set as root of the LCRS hierarchy tree
    } else {                                                    // every other node is a child of parentId
        tree.addChild(parentId, nodeId, name, role);            // attach under parentId using firstChild / nextSibling
    }
}

int main(int argc, char* argv[]) {
    cout << "===== PHASE 2: SECURE DISTRIBUTION =====" << endl;

    // Step 1: Load the locked paper package (paper.scbt) and root key (paper.key) from Phase 1
    string pkgPath = (argc > 1) ? argv[1] : "paper.scbt";       // default to paper.scbt in current directory
    string keyPath = (argc > 2) ? argv[2] : "paper.key";        // default to paper.key in current directory

    PaperPackage pkg;                                           // holds bitCount, Huffman table, and L0 encrypted bytes
    string rootKey;                                             // holds the 32-byte Board root key
    string error;                                               // holds any file error message

    if (!loadPaperPackage(pkgPath, pkg, error)) {               // read and validate paper.scbt
        cout << "Error loading paper package: " << error << endl;
        return 1;                                               // stop if Phase 1 output is missing or damaged
    }
    if (!loadRootKeyFile(keyPath, rootKey, error)) {            // read and validate paper.key
        cout << "Error loading key file: " << error << endl;
        return 1;                                               // stop if the 32-byte root key is missing
    }

    cout << "\nLoaded Phase 1 package: " << pkgPath
         << " (" << pkg.encrypted.size() << " encrypted bytes, "
         << pkg.bitCount << " compressed bits)" << endl;

    // Step 2: Register the distribution network in the AVLRegistry and build the LCRSTree
    // Hierarchy demonstrated: Board -> States -> Districts -> Cities -> Exam Centres
    // (Includes 1 Pending centre and 1 Rejected centre to prove unauthorized nodes are blocked)
    AVLRegistry registry;                                       // self-balancing AVL tree for O(log n) node verification
    LCRSTree tree;                                              // Left-Child Right-Sibling hierarchy tree

    // Level 0: Examination Board (Root)
    addNetworkNode(registry, tree, "",          "BOARD",        "Examination Board",      "Board",    "Registered");

    // Level 1: States
    addNetworkNode(registry, tree, "BOARD",     "ST-01",        "Andhra Pradesh",         "State",    "Registered");
    addNetworkNode(registry, tree, "BOARD",     "ST-02",        "Assam",                  "State",    "Registered");
    addNetworkNode(registry, tree, "BOARD",     "ST-08",        "Haryana",                "State",    "Registered");

    // Level 2: Districts under States
    addNetworkNode(registry, tree, "ST-01",     "ST-01-D01",    "Visakhapatnam District", "District", "Registered");
    addNetworkNode(registry, tree, "ST-02",     "ST-02-D01",    "Kamrup District",        "District", "Registered");
    addNetworkNode(registry, tree, "ST-08",     "ST-08-D01",    "Gurugram District",      "District", "Registered");

    // Level 3: Cities under Districts
    addNetworkNode(registry, tree, "ST-01-D01", "ST-01-D01-CT", "Visakhapatnam City",     "City",     "Registered");
    addNetworkNode(registry, tree, "ST-02-D01", "ST-02-D01-CT", "Guwahati City",          "City",     "Registered");
    addNetworkNode(registry, tree, "ST-08-D01", "ST-08-D01-CT", "Gurugram City",          "City",     "Registered");

    // Level 4: Exam Centres under Cities (2 Registered, 1 Pending, 1 Rejected)
    addNetworkNode(registry, tree, "ST-01-D01-CT", "CTR-AP-01", "GITAM Exam Centre 1",    "Centre",   "Registered");
    addNetworkNode(registry, tree, "ST-01-D01-CT", "CTR-AP-02", "Unverified AP Centre 2", "Centre",   "Pending");    // blocked!
    addNetworkNode(registry, tree, "ST-02-D01-CT", "CTR-AS-01", "AEC Exam Centre 1",      "Centre",   "Registered");
    addNetworkNode(registry, tree, "ST-08-D01-CT", "CTR-HR-01", "NHCE Exam Centre 1",     "Centre",   "Registered");
    addNetworkNode(registry, tree, "ST-08-D01-CT", "CTR-HR-99", "Revoked Rogue Centre",   "Centre",   "Rejected");   // blocked!

    cout << "\n===== STEP 1: AVL NODE REGISTRY & LCRS TREE =====" << endl;
    cout << "Total nodes registered in AVL tree : " << registry.size() << endl;
    cout << "AVL tree height                    : " << registry.height()
         << " (balanced: " << (registry.isBalanced() ? "YES" : "NO") << ")" << endl;
    cout << "Total nodes in LCRS hierarchy tree : " << tree.size() << endl;

    // Step 3: Broadcast the encrypted paper down the LCRS tree using BFS
    KeyManager keyMgr;                                          // handles HKDF child key derivation and AES-256-GCM layering
    AuditLog auditLog;                                          // records every verified hop in a SHA-256 hash chain

    cout << "\n===== STEP 2: BFS ONION BROADCAST =====" << endl;
    vector<HopReport> hops = tree.bfsBroadcast(pkg.encrypted, rootKey, registry, keyMgr, auditLog);

    cout << "\n--- Live Hop Table (Level-Order BFS) ---" << endl;
    int deliveredCount = 0;                                     // how many nodes received their onion-wrapped copy
    int blockedCount = 0;                                       // how many nodes were blocked by the AVL registry
    for (const HopReport& h : hops) {                           // print each hop attempt
        cout << "  Hop " << (h.hopNumber < 10 ? "0" : "") << h.hopNumber << " | "
             << h.fromId << " -> " << h.toId
             << " | " << h.status;
        if (h.payloadBytes > 0) {                               // show the growing onion payload size (+28 bytes per level)
            cout << " (" << h.payloadBytes << " bytes)";
            deliveredCount++;
        } else {
            blockedCount++;
        }
        cout << endl;
    }
    cout << "\nBroadcast summary: " << deliveredCount << " delivered, "
         << blockedCount << " blocked (Pending/Rejected)." << endl;

    // Step 4: Verify that each Exam Centre holds a distinct onion ciphertext and can unwrap back to L0
    cout << "\n===== STEP 3: ONION LAYER VERIFICATION AT EXAM CENTRES =====" << endl;
    const LCRSNode* cAp = tree.getNode("CTR-AP-01");            // Visakhapatnam Centre 1
    const LCRSNode* cHr = tree.getNode("CTR-HR-01");            // Gurugram Centre 1
    if (cAp != nullptr && cHr != nullptr) {
        cout << "CTR-AP-01 onion payload : " << cAp->encryptedPayload.size() << " bytes" << endl;
        cout << "CTR-HR-01 onion payload : " << cHr->encryptedPayload.size() << " bytes" << endl;
        cout << "Ciphertexts are unique  : "
             << (cAp->encryptedPayload != cHr->encryptedPayload ? "YES (different keys along each state/district/city/centre path)" : "NO")
             << endl;
    }

    // Peel all 5 layers (Centre -> City -> District -> State -> Board) at CTR-HR-01 and compare with Phase 1 compressed bytes
    string originalCompressed;
    if (!keyMgr.decryptChecked(pkg.encrypted, rootKey, originalCompressed)) { // decrypt Phase 1 L0 to get the reference compressed bytes
        cout << "Error: rootKey could not unlock Phase 1 package" << endl;
        return 1;
    }

    string peeledAtCentre;
    bool unwrapOk = tree.unwrapAtNode("CTR-HR-01", keyMgr, peeledAtCentre);
    cout << "Peel all 5 layers at CTR-HR-01 (Centre -> City -> District -> State -> Board): "
         << (unwrapOk && peeledAtCentre == originalCompressed ? "VERIFIED (matches Phase 1 compressed paper)" : "FAILED")
         << endl;
    if (!unwrapOk || peeledAtCentre != originalCompressed) {
        return 1;
    }

    // Step 5: Print and verify the SHA-256 Hash-Chained Audit Log, then run tamper detection demo
    cout << "\n===== STEP 4: SHA-256 HASH-CHAINED AUDIT LOG =====" << endl;
    for (const LogEntry& e : auditLog.getEntries()) {           // print every recorded hop and its 8-char short hashes
        cout << "  [" << (e.index < 10 ? "0" : "") << e.index << "] "
             << e.fromNode << " -> " << e.toNode
             << " | time: " << e.timestamp
             << " | prev: " << e.prevHash.substr(0, 8)
             << " | hash: " << e.hash.substr(0, 8) << endl;
    }

    int chainCheck = auditLog.verify();                         // recompute all SHA-256 hashes across the chain
    cout << "\nAudit chain verification : "
         << (chainCheck == -1 ? "CHAIN INTACT (all SHA-256 hashes verified)" : "BROKEN") << endl;
    if (chainCheck != -1) {
        return 1;
    }

    // Simulate an attacker altering entry 3 in the log and prove verify() catches the exact entry
    cout << "\n--- Simulating Tampering on Audit Entry 3 ---" << endl;
    auditLog.getMutableEntries()[3].toNode = "ROGUE-NODE";      // alter entry 3's recipient
    int brokenAt = auditLog.verify();                           // re-verify the hash chain
    cout << "Audit chain after tamper : CHAIN BROKEN AT ENTRY " << brokenAt << " (tamper detected!)" << endl;
    if (brokenAt != 3) {
        return 1;
    }

    cout << "\n===== PHASE 2 COMPLETE =====" << endl;
    return 0;                                                   // 0 = full Phase 2 pipeline succeeded
}
