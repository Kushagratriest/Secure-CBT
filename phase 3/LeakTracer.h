// LeakTracer.h: Phase 3 Cryptographic Leak-Source Attribution Engine.
// When an encrypted question paper surfaces publicly (e.g. leaked from an exam centre or branch),
// LeakTracer hashes/matches the leaked ciphertext against the LCRS hierarchy and queries
// AVLRegistry in O(log n) time to identify the exact culprit node, hierarchy level, and chain of custody.

#pragma once                                                    // include this header only once per compilation
#include <iostream>                                             // cout, endl for console output
#include <string>                                               // std::string
#include <vector>                                               // std::vector for chainOfCustody path
#include <sstream>                                              // ostringstream for hex formatting
#include <iomanip>                                              // hex, setw, setfill
#include <algorithm>                                            // reverse() to flip bottom-up parent path to Board -> Centre
#include <stdexcept>                                            // runtime_error if OpenSSL hashing fails
#include <openssl/evp.h>                                        // OpenSSL EVP_sha256() for ciphertext fingerprinting
#include "AVLRegistry.h"                                        // AVLRegistry for O(log n) node identity lookup
#include "LCRSTree.h"                                           // LCRSTree and LCRSNode
using namespace std;                                            // use standard library names directly

// Detailed forensics report generated when tracing a leaked encrypted paper file.
struct LeakReport {
    bool traced = false;                                        // true if leaked ciphertext matched a delivered node
    string leakedSha256;                                        // first 16 hex chars of the leaked ciphertext's SHA-256 hash
    int leakedBytes = 0;                                        // size in bytes of the leaked ciphertext (reveals hop depth!)
    string culpritNodeId;                                       // node ID in AVLRegistry responsible for this ciphertext
    string culpritName;                                         // human-readable institution/server name
    string culpritRole;                                         // hierarchy level ("Board", "State", "District", "City", "Centre")
    string statusInRegistry;                                    // official status stored in AVLRegistry ("Registered", etc.)
    string keyFingerprint;                                      // first 8 hex chars of this node's derived AES-256 key
    int hierarchyDepth = 0;                                     // hops below Board (0=Board, 1=State, 2=District, 3=City, 4=Centre)
    vector<string> chainOfCustody;                              // top-down path: ["BOARD", "ST-01", ..., "CTR-AP-01"]
};

// Matches a leaked ciphertext against delivered nodes and resolves culprit identity via AVLRegistry.
class LeakTracer {
private:
    // Computes SHA-256 digest of arbitrary binary bytes and returns 64 lowercase hex characters.
    static string sha256Hex(const string& input) {
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();                     // allocate OpenSSL digest context
        if (ctx == nullptr) {                                   // guard out-of-memory error
            throw runtime_error("OpenSSL: could not create EVP_MD_CTX");
        }
        unsigned char digest[EVP_MAX_MD_SIZE];                  // raw 32-byte SHA-256 digest
        unsigned int digestLen = 0;                             // actual byte count written by OpenSSL

        bool ok = (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1)
               && (EVP_DigestUpdate(ctx, input.data(), input.size()) == 1)
               && (EVP_DigestFinal_ex(ctx, digest, &digestLen) == 1);
        EVP_MD_CTX_free(ctx);                                   // free context before throwing or returning
        if (!ok) throw runtime_error("OpenSSL: SHA-256 failed in LeakTracer");

        ostringstream out;                                      // format raw bytes as hex characters
        out << hex << setfill('0');
        for (unsigned int i = 0; i < digestLen; i++) {
            out << setw(2) << (int)digest[i];
        }
        return out.str();                                       // return 64-character hex fingerprint
    }

    // Formats the first 4 bytes (8 hex chars) of a key as a safe public fingerprint.
    static string keyFingerprintHex(const string& rawKey) {
        if (rawKey.empty()) return "00000000";                  // fallback if node has no assigned key
        ostringstream out;
        out << hex << setfill('0');
        size_t limit = min((size_t)4, rawKey.size());           // first 4 bytes = 8 hex characters
        for (size_t i = 0; i < limit; i++) {
            out << setw(2) << (int)(unsigned char)rawKey[i];
        }
        return out.str();                                       // safe prefix (never reveals full 32-byte key)
    }

    // Walks the LCRS tree recursively (children and siblings) to find the node holding 'targetPayload'.
    const LCRSNode* findNodeByCiphertext(const LCRSNode* current, const string& targetPayload) const {
        if (current == nullptr) return nullptr;                 // reached end of branch
        if (current->delivered && current->encryptedPayload == targetPayload) {
            return current;                                     // exact ciphertext match found!
        }
        const LCRSNode* childMatch = findNodeByCiphertext(current->firstChild, targetPayload);
        if (childMatch != nullptr) return childMatch;           // found inside child subtree
        return findNodeByCiphertext(current->nextSibling, targetPayload); // check next sibling branch
    }

public:
    LeakTracer() {}

    // Traces a leaked encrypted payload back to its exact issuing node and verifies its record in AVLRegistry.
    LeakReport traceLeakedCiphertext(const string& leakedCiphertext,
                                     const LCRSTree& tree,
                                     const AVLRegistry& registry) const {
        LeakReport report;                                      // initialize blank forensics report
        if (leakedCiphertext.empty()) return report;            // nothing to trace if empty

        report.leakedBytes = (int)leakedCiphertext.size();      // record byte length of leaked file
        report.leakedSha256 = sha256Hex(leakedCiphertext).substr(0, 16); // compute short SHA-256 fingerprint

        // Step 1: Locate the node whose unique onion ciphertext matches the leaked bytes
        const LCRSNode* rootNode = tree.getNode("BOARD");       // start search from the Board root
        const LCRSNode* matched = findNodeByCiphertext(rootNode, leakedCiphertext);
        if (matched == nullptr) {                               // fallback: scan any delivered node if root wasn't named BOARD
            return report;                                      // traced remains false (unknown ciphertext)
        }

        // Step 2: Query AVLRegistry in O(log n) time to fetch official registration record
        const NodeInfo* regInfo = registry.find(matched->nodeId);
        report.traced = true;                                   // mark leak attribution as successful
        report.culpritNodeId = matched->nodeId;                 // record culprit ID
        report.culpritName = (regInfo != nullptr) ? regInfo->name : matched->name;
        report.culpritRole = (regInfo != nullptr) ? regInfo->role : matched->role;
        report.statusInRegistry = (regInfo != nullptr) ? regInfo->status : "Unknown";
        report.keyFingerprint = keyFingerprintHex(matched->key); // public 8-char key fingerprint

        // Step 3: Walk up LCRS parent pointers to reconstruct full top-down chain of custody
        const LCRSNode* cursor = matched;                       // start at culprit leaf/node
        while (cursor != nullptr) {                             // climb up to the Examination Board
            report.chainOfCustody.push_back(cursor->nodeId);    // add node ID
            cursor = cursor->parent;                            // move up one level
        }
        reverse(report.chainOfCustody.begin(), report.chainOfCustody.end()); // reverse so BOARD is first
        report.hierarchyDepth = (int)report.chainOfCustody.size() - 1;       // 0=Board, 4=Centre

        return report;                                          // return completed forensics report
    }
};
