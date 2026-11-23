// test_phase3.cpp: Unit & integration test suite for Phase 3 modules:
//   - HuffmanCompressor::decompress() bit-exact round-trip
//   - TimeLock Min-Heap ordering + early-access gate
//   - ExamCentre 5-layer onion unwrap + deterministic Fisher-Yates student shuffle
//   - LeakTracer culprit attribution via AVLRegistry

#include <iostream>
#include <cassert>
#include <ctime>
#include "HuffmanCompressor.h"
#include "KeyManager.h"
#include "AuditLog.h"
#include "AVLRegistry.h"
#include "LCRSTree.h"
#include "IndiaNetwork.h"
#include "TimeLock.h"
#include "ExamCentre.h"
#include "LeakTracer.h"
using namespace std;

int main() {
    cout << "===== PHASE 3 UNIT TESTS =====" << endl;

    // Test 1: Huffman compress + bit-packed decompress round-trip (including padding bits)
    HuffmanCompressor comp;
    string samplePaper =
        "1. [DS01] (Data Structures / Trees, 2 marks)\nWhat is an AVL tree?\n\n"
        "2. [DB02] (DBMS / SQL, 5 marks)\nExplain ACID properties with examples.\n\n"
        "3. [OS03] (Operating Systems / Deadlocks, 10 marks)\nExplain Banker's algorithm.\n\n"
        "Total marks: 17\n";
    string packed = comp.compress(samplePaper);
    string restored = comp.decompress(packed, comp.getBitCount(), comp.getCodes());
    assert(restored == samplePaper);
    cout << "[PASS] Huffman bit-packed compress/decompress round-trip exact match" << endl;

    // Test 2: TimeLock Min-Heap ordering
    time_t now = 1700000000;
    TimeLock tl;
    tl.addSlot("S3", "C3", "Late", now + 7200);
    tl.addSlot("S1", "C1", "Early", now + 60);
    tl.addSlot("S2", "C2", "Mid", now + 3600);
    assert(tl.popEarliest().slotId == "S1");
    assert(tl.popEarliest().slotId == "S2");
    assert(tl.popEarliest().slotId == "S3");
    assert(!tl.canUnlock(now + 60, now));
    assert(tl.canUnlock(now + 60, now + 60));
    cout << "[PASS] TimeLock Min-Heap Extract-Min and lock gate verified" << endl;

    // Test 3: End-to-end Phase 2 broadcast + Phase 3 unlock + student shuffle + leak tracing
    KeyManager km;
    string rootKey = km.generateRootKey();
    string l0Encrypted = km.encrypt(packed, rootKey);
    PaperPackage pkg{comp.getBitCount(), (int64_t)(now + 300), comp.getCodes(), l0Encrypted};

    // Verify v3 PaperStorage round-trip preserves unlockTimestamp
    string tmpScbt = "/tmp/test_v3_paper.scbt";
    string ioErr;
    assert(savePaperPackage(tmpScbt, pkg, ioErr));
    PaperPackage loadedPkg;
    assert(loadPaperPackage(tmpScbt, loadedPkg, ioErr));
    assert(loadedPkg.unlockTimestamp == now + 300);
    cout << "[PASS] PaperStorage v3 unlockTimestamp round-trip verified" << endl;

    AVLRegistry reg;
    LCRSTree tree;
    AuditLog log;
    reg.registerNode("BOARD", "Board", "Board", "", "Registered");
    reg.registerNode("ST-01", "State 1", "State", "BOARD", "Registered");
    reg.registerNode("C-01", "Centre 1", "Centre", "ST-01", "Registered");
    reg.registerNode("C-02", "Centre 2", "Centre", "ST-01", "Registered");
    tree.setRoot("BOARD", "Board", "Board");
    tree.addChild("BOARD", "ST-01", "State 1", "State");
    tree.addChild("ST-01", "C-01", "Centre 1", "Centre");
    tree.addChild("ST-01", "C-02", "Centre 2", "Centre");
    tree.bfsBroadcast(l0Encrypted, rootKey, reg, km, log);

    ExamCentre ec;
    string plain, err;
    assert(!ec.unlockAndDecompress("C-01", now + 300, now, tl, tree, km, comp, pkg, plain, err));
    assert(ec.unlockAndDecompress("C-01", now + 300, now + 300, tl, tree, km, comp, pkg, plain, err));
    assert(plain == samplePaper);
    cout << "[PASS] ExamCentre time-lock enforcement + onion unwrap + decompress verified" << endl;

    StudentView vA1 = ec.buildStudentTerminalPaper("C-01", "STU-A", plain);
    StudentView vA2 = ec.buildStudentTerminalPaper("C-01", "STU-A", plain);
    StudentView vB  = ec.buildStudentTerminalPaper("C-01", "STU-B", plain);
    assert(vA1.shuffledQuestions.size() == 3);
    assert(vA1.formattedPaper == vA2.formattedPaper); // deterministic on refresh
    assert(vA1.shuffleSeed != vB.shuffleSeed);
    cout << "[PASS] Per-student deterministic shuffle verified" << endl;

    LeakTracer lt;
    LeakReport rep = lt.traceLeakedCiphertext(tree.getNode("C-02")->encryptedPayload, tree, reg);
    assert(rep.traced && rep.culpritNodeId == "C-02");
    assert(rep.chainOfCustody.size() == 3 && rep.chainOfCustody[0] == "BOARD");
    cout << "[PASS] LeakTracer matched C-02 via AVLRegistry" << endl;

    // Test 4: All-India Network + Centre-to-Student L5 forwarding (6-layer onion unwrap)
    AVLRegistry indiaReg;
    LCRSTree indiaTree;
    AuditLog indiaLog;
    populateAllIndiaNetwork(indiaReg, indiaTree);
    indiaTree.bfsBroadcast(l0Encrypted, rootKey, indiaReg, km, indiaLog);
    vector<StudentTerminalReceipt> receipts = ec.forwardPaperToStudents(
        "CTR-UK-01", {"2510013438", "2510370185"}, indiaReg, indiaTree, km, indiaLog);
    assert(receipts.size() == 2 && receipts[0].forwarded && receipts[1].forwarded);
    string peeledStu;
    assert(indiaTree.unwrapAtNode("STU-2510013438", km, peeledStu));
    assert(comp.decompress(peeledStu, loadedPkg.bitCount, loadedPkg.codes) == samplePaper);
    cout << "[PASS] All-India Network + Centre-to-Student L5 6-layer unwrap verified" << endl;

    cout << "ALL PHASE 3 UNIT TESTS PASSED" << endl;
    return 0;
}
