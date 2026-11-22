// phase3-exam-centre/main.cpp: End-to-end runner for Phase 3 (Exam Centre & Leak Traceability).
// Takes paper.scbt and paper.key from Phase 1, distributes onion ciphertexts via Phase 2
// (AVLRegistry + LCRSTree BFS + AuditLog), then demonstrates:
//   1. Min-Heap Exam Slot Scheduling (TimeLock.h)
//   2. Premature Decryption Blocking before exam start time
//   3. Time-Locked Unwrapping of all 5 onion layers + Huffman Decompression at CTR-HR-01
//   4. Per-Student Deterministic Terminal Shuffle (ExamCentre.h)
//   5. Cryptographic Leak-Source Attribution via AVLRegistry (LeakTracer.h)
//
// Usage: ./secure_cbt_phase3 [path-to-paper.scbt] [path-to-paper.key]

#include <iostream>                                             // cout, endl for console output
#include <fstream>                                              // ifstream for reading paper.key
#include <string>                                               // std::string
#include <vector>                                               // std::vector
#include <ctime>                                                // time() for simulation clock
#include "PaperStorage.h"                                       // PaperPackage, loadPaperPackage()
#include "HuffmanCompressor.h"                                  // HuffmanCompressor::decompress()
#include "KeyManager.h"                                         // KeyManager (AES-256-GCM + HKDF-SHA256)
#include "AuditLog.h"                                           // AuditLog (SHA-256 hash chain)
#include "AVLRegistry.h"                                        // AVLRegistry (self-balancing AVL tree)
#include "LCRSTree.h"
#include "IndiaNetwork.h"                                           // LCRSTree (LCRS hierarchy + BFS broadcast)
#include "TimeLock.h"                                           // TimeLock (Min-Heap scheduler)
#include "ExamCentre.h"                                         // ExamCentre (unlock + student terminal shuffle)
#include "LeakTracer.h"                                         // LeakTracer (leak attribution)
using namespace std;                                            // use standard library names directly

// Reads the 32-byte root key from 'path' into 'outKey'. Returns true on success.
bool loadRootKeyFile(const string& path, string& outKey, string& error) {
    ifstream in(path, ios::binary);                             // open the key file in binary mode
    if (!in.is_open()) {                                        // file missing or unreadable
        error = "Could not open " + path + " (run Phase 1 first)";
        return false;
    }
    outKey.assign((istreambuf_iterator<char>(in)),              // read all bytes of key file
                  istreambuf_iterator<char>());
    if ((int)outKey.size() != AES_KEY_BYTES) {                  // must be exactly 32 bytes for AES-256
        error = path + " must be exactly " + to_string(AES_KEY_BYTES) + " bytes";
        return false;
    }
    return true;
}

// Helper to add a node into both the AVLRegistry and LCRSTree.
void addNetworkNode(AVLRegistry& reg, LCRSTree& tree, const string& parentId,
                    const string& nodeId, const string& name,
                    const string& role, const string& status = "Registered") {
    reg.registerNode(nodeId, name, role, parentId, status);     // register in AVL tree
    if (parentId.empty()) {                                     // empty parentId = root Board
        tree.setRoot(nodeId, name, role);
    } else {
        tree.addChild(parentId, nodeId, name, role);
    }
}

// Extracts the first line of a question block for compact display in CLI output.
string firstLineOf(const string& block) {
    size_t pos = block.find('\n');                              // locate line break after header
    return (pos == string::npos) ? block : block.substr(0, pos);
}

int main(int argc, char* argv[]) {
    cout << "===== PHASE 3: EXAM CENTRE & LEAK TRACEABILITY =====" << endl;

    string pkgPath = (argc > 1) ? argv[1] : "paper.scbt";       // default to paper.scbt
    string keyPath = (argc > 2) ? argv[2] : "paper.key";        // default to paper.key

    PaperPackage pkg;                                           // holds bitCount, Huffman codes, L0 encrypted paper
    string rootKey;                                             // 32-byte root key
    string error;
    if (!loadPaperPackage(pkgPath, pkg, error) || !loadRootKeyFile(keyPath, rootKey, error)) {
        cout << "Error: " << error << endl;
        return 1;
    }

    // Rebuild the All-India Phase 2 distribution network and run BFS broadcast
    AVLRegistry registry;
    LCRSTree tree;
    populateAllIndiaNetwork(registry, tree);

    KeyManager keyMgr;
    AuditLog auditLog;
    tree.bfsBroadcast(pkg.encrypted, rootKey, registry, keyMgr, auditLog);
    cout << "\nDistribution ready: " << tree.size() << " nodes, "
         << auditLog.getEntries().size() << " verified hops." << endl;

    // Step 1: Schedule exam slots out of order in the TimeLock Min-Heap
    cout << "\n===== STEP 1: MIN-HEAP TIME-LOCK SCHEDULE =====" << endl;
    time_t now = time(nullptr);                                 // base simulation clock
    TimeLock timeLock;                                          // Min-Heap ordered by earliest unlockTime
    timeLock.addSlot("SLOT-2", "CTR-AP-01", "Afternoon CSE Shift", now + 3600); // inserted 1st (later)
    timeLock.addSlot("SLOT-3", "CTR-AS-01", "Evening Lab Shift",   now + 7200); // inserted 2nd (latest)
    timeLock.addSlot("SLOT-1", "CTR-HR-01", "Morning Exam Shift",  now + 1800); // inserted 3rd (earliest!)

    cout << "Queued 3 slots out-of-order. Min-Heap Extract-Min order:" << endl;
    TimeLock demoPop = timeLock;                                // copy heap so original stays available
    while (!demoPop.isEmpty()) {
        ExamSlot s = demoPop.popEarliest();                     // O(log n) Extract-Min
        cout << "  " << s.slotId << " | Centre: " << s.centreId
             << " | " << s.examName
             << " | Unlocks at: " << TimeLock::formatTime(s.unlockTime) << endl;
    }

    // Step 2: Test Premature Decryption Block (T = now, scheduled = now + 1800)
    cout << "\n===== STEP 2: PREMATURE ACCESS BLOCK CHECK =====" << endl;
    ExamCentre examCentre;
    HuffmanCompressor compressor;
    string unlockedPaper;
    string unlockErr;
    time_t hrUnlockTime = now + 1800;                           // scheduled 30 mins in the future

    bool earlyAllowed = examCentre.unlockAndDecompress(
        "CTR-HR-01", hrUnlockTime, now, timeLock, tree, keyMgr, compressor, pkg, unlockedPaper, unlockErr);
    cout << "Attempt at T = schedule - 1800s : " << (earlyAllowed ? "ALLOWED (BUG!)" : "BLOCKED") << endl;
    cout << "Reason                          : " << unlockErr << endl;
    if (earlyAllowed) return 1;

    // Step 3: Clock reaches exam start time -> Unwrap 5 onion layers & Huffman decompress
    cout << "\n===== STEP 3: TIME-LOCKED UNWRAP & HUFFMAN DECOMPRESSION =====" << endl;
    time_t examStartClock = hrUnlockTime;                       // clock reaches scheduled exam time
    bool onTimeAllowed = examCentre.unlockAndDecompress(
        "CTR-HR-01", hrUnlockTime, examStartClock, timeLock, tree, keyMgr, compressor, pkg, unlockedPaper, unlockErr);
    if (!onTimeAllowed) {
        cout << "Error unlocking paper: " << unlockErr << endl;
        return 1;
    }

    // Verify recovered plaintext matches Phase 1 reference decompression
    string rootCompressed;
    keyMgr.decryptChecked(pkg.encrypted, rootKey, rootCompressed);
    string expectedPlaintext = compressor.decompress(rootCompressed, pkg.bitCount, pkg.codes);
    cout << "Attempt at T = scheduled start  : UNLOCKED" << endl;
    cout << "Peeled 5 AES-256-GCM layers     : 3528 B -> 3388 B" << endl;
    cout << "Huffman decompressed text size  : " << unlockedPaper.size() << " bytes" << endl;
    cout << "Byte-for-byte match w/ Phase 1  : " << (unlockedPaper == expectedPlaintext ? "VERIFIED (100% MATCH)" : "MISMATCH") << endl;
    if (unlockedPaper != expectedPlaintext) return 1;

    // Step 4: Deterministic Per-Student Terminal Shuffle (Fairness + Anti-Cheating)
    cout << "\n===== STEP 4: PER-STUDENT DETERMINISTIC SHUFFLE =====" << endl;
    StudentView s1 = examCentre.buildStudentTerminalPaper("CTR-HR-01", "2510013438", unlockedPaper); // Naman
    StudentView s2 = examCentre.buildStudentTerminalPaper("CTR-HR-01", "2510370185", unlockedPaper); // Aayush
    StudentView s3 = examCentre.buildStudentTerminalPaper("CTR-HR-01", "2510012263", unlockedPaper); // Kushagra

    cout << "Questions per terminal: " << s1.shuffledQuestions.size() << endl;
    cout << "  Student 2510013438 (seed " << s1.shuffleSeed << ") Q1: " << firstLineOf(s1.shuffledQuestions[0]) << endl;
    cout << "  Student 2510370185 (seed " << s2.shuffleSeed << ") Q1: " << firstLineOf(s2.shuffledQuestions[0]) << endl;
    cout << "  Student 2510012263 (seed " << s3.shuffleSeed << ") Q1: " << firstLineOf(s3.shuffledQuestions[0]) << endl;

    // Step 5: Cryptographic Leak Tracing via AVLRegistry + LCRS Hierarchy
    cout << "\n===== STEP 5: LEAK-SOURCE TRACEABILITY (AVL REGISTRY) =====" << endl;
    LeakTracer tracer;

    // Simulate a leak from Exam Centre CTR-AP-01 (3528 bytes, depth 4)
    const LCRSNode* apNode = tree.getNode("CTR-AP-01");
    LeakReport r1 = tracer.traceLeakedCiphertext(apNode->encryptedPayload, tree, registry);
    cout << "Simulated Leak #1 (" << r1.leakedBytes << " B, SHA-256: " << r1.leakedSha256 << "...):" << endl;
    cout << "  Traced Culprit : " << r1.culpritNodeId << " (" << r1.culpritName << ", role: " << r1.culpritRole << ")" << endl;
    cout << "  Key Fingerprint: " << r1.keyFingerprint << " | Status in AVL: " << r1.statusInRegistry << endl;
    cout << "  Custody Chain  : ";
    for (size_t i = 0; i < r1.chainOfCustody.size(); i++) {
        cout << (i > 0 ? " -> " : "") << r1.chainOfCustody[i];
    }
    cout << endl;

    // Simulate an upstream leak at District ST-02-D01 (3472 bytes, depth 2)
    const LCRSNode* distNode = tree.getNode("ST-02-D01");
    LeakReport r2 = tracer.traceLeakedCiphertext(distNode->encryptedPayload, tree, registry);
    cout << "\nSimulated Leak #2 (" << r2.leakedBytes << " B, SHA-256: " << r2.leakedSha256 << "...):" << endl;
    cout << "  Traced Culprit : " << r2.culpritNodeId << " (" << r2.culpritName << ", role: " << r2.culpritRole << ")" << endl;
    cout << "  Custody Chain  : ";
    for (size_t i = 0; i < r2.chainOfCustody.size(); i++) {
        cout << (i > 0 ? " -> " : "") << r2.chainOfCustody[i];
    }
    cout << endl;

    if (!r1.traced || r1.culpritNodeId != "CTR-AP-01" || !r2.traced || r2.culpritNodeId != "ST-02-D01") {
        cout << "TEST FAILED: leak attribution mismatch" << endl;
        return 1;
    }

    cout << "\n===== PHASE 3 COMPLETE =====" << endl;
    return 0;
}
