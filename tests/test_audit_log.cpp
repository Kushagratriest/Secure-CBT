// test_audit_log.cpp: Verifies common/AuditLog.h.
// Appends 5 transfer entries, checks that verify() returns -1 (chain intact),
// tampers with entry 2, and checks that verify() returns 2 (first broken entry).

#include <iostream>                                             // cout, endl for printing test output
#include <string>                                               // std::string for text manipulation
#include "AuditLog.h"                                           // the AuditLog class and LogEntry struct under test
using namespace std;                                            // standard library names without std:: prefix

// Helper that prints every entry in the audit log in a readable table.
void printLog(const AuditLog& log) {
    for (const LogEntry& e : log.getEntries()) {                // loop through each stored LogEntry via the const getter
        cout << "  [" << e.index << "] "                        // print the 0-based index of the entry
             << e.fromNode << " -> " << e.toNode                // print sender and receiver node IDs
             << " | time: " << e.timestamp                      // print when the transfer happened
             << " | prev: " << e.prevHash.substr(0, 8) << "..." // print first 8 hex chars of previous entry's hash
             << " | hash: " << e.hash.substr(0, 8) << "..."     // print first 8 hex chars of this entry's hash
             << endl;
    }
}

int main() {
    cout << "===== AUDIT LOG TEST =====" << endl;               // banner for the test run

    AuditLog log;                                               // create a fresh, empty hash-chained audit log

    // Step 1: append 5 transfer hops across the distribution hierarchy
    log.append("BOARD", "ST-01");                               // entry 0: Board sends to State 1 (prevHash = 64 zeros)
    log.append("BOARD", "ST-02");                               // entry 1: Board sends to State 2
    log.append("ST-01", "ST-01-C01");                           // entry 2: State 1 sends to Centre 1
    log.append("ST-01", "ST-01-C02");                           // entry 3: State 1 sends to Centre 2
    log.append("ST-02", "ST-02-C01");                           // entry 4: State 2 sends to Centre 1

    cout << "\nAppended " << log.getEntries().size() << " entries:" << endl;
    printLog(log);                                              // display all 5 entries and their short hashes

    // Step 2: verify the untampered chain (must return -1)
    int statusBefore = log.verify();                            // recompute all 5 SHA-256 hashes and check links
    cout << "\nVerify before tampering: " << statusBefore       // print the return value (-1 means intact)
         << (statusBefore == -1 ? " (OK - chain intact)" : " (FAIL)") << endl;
    if (statusBefore != -1) {                                   // if an untampered chain failed verification
        cout << "TEST FAILED: expected -1 on intact chain" << endl;
        return 1;                                               // exit with non-zero code to signal failure
    }

    // Step 3: tamper with entry 2 by changing its receiver node without knowing how to forge the chain
    cout << "\nTampering with entry 2 (changing toNode from \"ST-01-C01\" to \"ST-99-ROGUE\")..." << endl;
    log.getMutableEntries()[2].toNode = "ST-99-ROGUE";          // modify entry 2 in place

    // Step 4: verify the tampered chain (must report index 2)
    int statusAfter = log.verify();                             // recompute hashes; entry 2's hash will no longer match
    cout << "Verify after tampering:  " << statusAfter          // print the detected broken index
         << (statusAfter == 2 ? " (OK - detected tamper at entry 2)" : " (FAIL)") << endl;
    if (statusAfter != 2) {                                     // if verify() did not return 2
        cout << "TEST FAILED: expected 2 after tampering with entry 2" << endl;
        return 1;                                               // exit with non-zero code to signal failure
    }

    cout << "\nALL AUDIT LOG TESTS PASSED" << endl;             // confirm both checks succeeded
    return 0;                                                   // 0 = success
}
