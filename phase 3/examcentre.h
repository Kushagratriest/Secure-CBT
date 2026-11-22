// ExamCentre.h: Phase 3 Exam Centre Time-Locked Decryption & Student Terminal Shuffle.
// 1. Enforces the TimeLock check before decrypting any question paper at a centre.
// 2. Unwraps all AES-256-GCM onion layers down the LCRS path (Centre -> ... -> Board).
// 3. Decompresses the Huffman bit-packed paper into the full question text.
// 4. Shuffles questions deterministically per Student ID using Fisher-Yates so every
//    student receives the identical set of questions (fairness) in a unique order.

#pragma once                                                    // include this header only once per compilation
#include <iostream>                                             // cout, endl for console output
#include <string>                                               // std::string for paper text, centreId, and studentId
#include <vector>                                               // std::vector for parsed question blocks
#include <sstream>                                              // istringstream / ostringstream for parsing paper lines
#include <cstdint>                                              // uint32_t for deterministic per-student PRNG seed
#include <cctype>                                               // isdigit for detecting numbered question headers
#include "LCRSTree.h"                                           // LCRSTree::unwrapAtNode()
#include "HuffmanCompressor.h"                                  // HuffmanCompressor::decompress()
#include "TimeLock.h"                                           // TimeLock time-gate verification
#include "PaperStorage.h"                                       // PaperPackage (bitCount + Huffman codes)
using namespace std;                                            // use standard library names directly

// Output delivered to a single student's exam terminal at a verified exam centre.
struct StudentView {
    string centreId;                                            // ID of the exam centre, e.g. "CTR-HR-01"
    string studentId;                                           // student roll number / terminal ID, e.g. "STU-2510012263"
    uint32_t shuffleSeed;                                       // deterministic 32-bit seed derived from centreId + studentId
    vector<string> shuffledQuestions;                           // individual question blocks in this student's shuffled order
    string formattedPaper;                                      // complete numbered paper ready for display on student PC
};

// Receipt produced when an Exam Centre forwards the L5-wrapped paper to a Student Terminal.
struct StudentTerminalReceipt {
    string studentNodeId;                                       // LCRS/AVL node ID, e.g. "STU-2510013438"
    string studentRoll;                                         // student roll number, e.g. "2510013438"
    string centreId;                                            // parent exam centre ID, e.g. "CTR-UK-01"
    bool forwarded;                                             // true if L5 delivery succeeded
    int payloadBytes;                                           // size of the 6-layer onion payload at the student terminal
    string keyFingerprint;                                      // SHA-256 fingerprint prefix of the student's L5 key
};

// Manages time-locked decryption, Huffman decompression, and per-student terminal randomization.
class ExamCentre {
private:
    // Computes a deterministic 32-bit FNV-1a hash seed from "centreId:studentId".
    // This makes sure the same student always gets the exact same shuffle on page refresh,
    // while different students get completely different question orders.
    static uint32_t computeStudentSeed(const string& centreId, const string& studentId) {
        string combined = centreId + ":" + studentId;           // combine centre and student IDs
        uint32_t hash = 2166136261u;                            // FNV-1a 32-bit offset basis
        for (unsigned char c : combined) {                      // mix in every byte of the combined ID
            hash ^= c;                                          // XOR with current byte
            hash *= 16777619u;                                  // multiply by FNV-1a prime
        }
        return (hash == 0) ? 1u : hash;                         // xorshift requires a non-zero seed
    }

    // Advances a lightweight deterministic xorshift32 PRNG and returns the next pseudo-random uint32_t.
    static uint32_t nextRand(uint32_t& state) {
        state ^= state << 13;                                   // shift left and XOR
        state ^= state >> 17;                                   // shift right and XOR
        state ^= state << 5;                                    // final shift and XOR
        return state;                                           // return updated pseudo-random state
    }

    // Checks whether a line starts a numbered question block like "1. [DS18] ...".
    static bool isQuestionStartLine(const string& line) {
        size_t i = 0;                                           // scan leading digits
        while (i < line.size() && isdigit((unsigned char)line[i])) i++;
        if (i == 0 || i + 1 >= line.size()) return false;       // must start with at least 1 digit
        return line[i] == '.' && line[i + 1] == ' ';            // followed by ". "
    }

    // Removes the original "N. " prefix from a question block so we can re-number after shuffling.
    static string stripNumberPrefix(const string& block) {
        size_t dotPos = block.find(". ");                       // find ". " after the original question number
        if (dotPos == string::npos) return block;               // fallback if no prefix found
        return block.substr(dotPos + 2);                        // keep everything starting at "[ID] (Subject / Topic...)"
    }

    // Splits assembled plaintext paper into raw question blocks and extracts the trailing "Total marks:" line.
    static vector<string> splitQuestions(const string& paperText, string& totalMarksLine) {
        vector<string> questions;                               // collected question blocks
        totalMarksLine = "";                                    // reset footer line
        istringstream stream(paperText);                        // read line by line
        string line;                                            // current line
        string currentBlock = "";                               // accumulates lines of the current question

        while (getline(stream, line)) {                         // iterate through every line in the paper
            if (!line.empty() && line.back() == '\r') {         // strip any CR if present
                line.pop_back();
            }
            if (line.rfind("Total marks:", 0) == 0) {           // reached the total marks footer line
                if (!currentBlock.empty()) {                    // save any open question block before stopping
                    questions.push_back(stripNumberPrefix(currentBlock));
                    currentBlock.clear();
                }
                totalMarksLine = line;                          // remember footer line
                break;                                          // done reading questions
            }
            if (isQuestionStartLine(line)) {                    // starting a brand new question block
                if (!currentBlock.empty()) {                    // save previous question block if one was open
                    questions.push_back(stripNumberPrefix(currentBlock));
                }
                currentBlock = line;                            // start fresh with this header line
            } else if (!currentBlock.empty()) {                 // continuation line inside current question
                currentBlock += "\n" + line;                    // preserve question text / blank separator
            }
        }
        if (!currentBlock.empty()) {                            // flush final question block if no Total marks line was present
            questions.push_back(stripNumberPrefix(currentBlock));
        }

        for (string& q : questions) {                           // trim trailing newlines inside each block
            while (!q.empty() && q.back() == '\n') q.pop_back();
        }
        return questions;                                       // return parsed question bodies
    }

public:
    ExamCentre() {}

    // Enforces the TimeLock gate, unwraps all LCRS onion layers at 'centreId', and Huffman-decompresses
    // the paper into 'outPlaintext'. Returns false and sets 'outError' if locked or tampered.
    bool unlockAndDecompress(const string& centreId,
                             time_t unlockTime,
                             time_t currentTime,
                             const TimeLock& timeLock,
                             const LCRSTree& tree,
                             KeyManager& keyMgr,
                             const HuffmanCompressor& compressor,
                             const PaperPackage& pkg,
                             string& outPlaintext,
                             string& outError) const {
        // Gate 1: Verify the current clock has reached the scheduled exam start time
        if (!timeLock.canUnlock(unlockTime, currentTime)) {
            long waitSec = timeLock.secondsUntilUnlock(unlockTime, currentTime);
            outError = "TIME-LOCK ACTIVE: Paper cannot be decrypted at " + centreId
                     + " (" + to_string(waitSec) + "s remaining until scheduled start)";
            return false;                                       // block premature decryption
        }

        // Gate 2: Peel every AES-256-GCM onion layer along the hierarchy path (Centre -> ... -> Board)
        string peeledCompressed;                                // receives the Phase 1 compressed binary bytes
        if (!tree.unwrapAtNode(centreId, keyMgr, peeledCompressed)) {
            outError = "Onion decryption failed at " + centreId
                     + " (node was not delivered, key mismatch, or ciphertext tampered)";
            return false;                                       // reject invalid or tampered package
        }

        // Gate 3: Decompress the bit-packed Huffman payload back into plaintext questions
        outPlaintext = compressor.decompress(peeledCompressed, pkg.bitCount, pkg.codes);
        if (outPlaintext.empty()) {                             // check decompression produced valid text
            outError = "Huffman decompression produced empty output";
            return false;
        }

        return true;                                            // unlocked and verified plaintext paper is ready
    }

    // Generates a deterministically shuffled question paper for 'studentId' at 'centreId'
    // using Fisher-Yates shuffle seeded from (centreId, studentId).
    StudentView buildStudentTerminalPaper(const string& centreId,
                                          const string& studentId,
                                          const string& unlockedPlaintext) const {
        StudentView view;                                       // output view struct for this student
        view.centreId = centreId;                               // record centre ID
        view.studentId = studentId;                             // record student ID
        view.shuffleSeed = computeStudentSeed(centreId, studentId); // deterministic seed

        string totalMarksLine;                                  // footer like "Total marks: 221"
        vector<string> blocks = splitQuestions(unlockedPlaintext, totalMarksLine);

        // Run deterministic Fisher-Yates shuffle backwards from last index down to 1
        uint32_t prng = view.shuffleSeed;                       // local PRNG state (never affects global rand)
        for (int i = (int)blocks.size() - 1; i > 0; i--) {
            uint32_t r = nextRand(prng);                        // generate next deterministic 32-bit number
            int j = (int)(r % (uint32_t)(i + 1));               // pick random index in [0, i]
            swap(blocks[i], blocks[j]);                         // swap question i and question j
        }

        view.shuffledQuestions = blocks;                        // save ordered blocks for JSON/UI display
        ostringstream formatted;                                // build printable numbered paper
        for (size_t i = 0; i < blocks.size(); i++) {
            formatted << (i + 1) << ". " << blocks[i] << "\n\n"; // renumber sequentially 1, 2, 3...
        }
        if (!totalMarksLine.empty()) {                          // re-attach the original total marks footer
            formatted << totalMarksLine << "\n";
        }
        view.formattedPaper = formatted.str();                  // store complete paper text for student terminal
        return view;                                            // hand back complete student view
    }

    // Forwards the encrypted paper from 'centreId' to each student terminal in 'studentRolls'
    // by registering "STU-<roll>" (role "Student") and wrapping one more AES-256-GCM layer (L5).
    vector<StudentTerminalReceipt> forwardPaperToStudents(const string& centreId,
                                                          const vector<string>& studentRolls,
                                                          AVLRegistry& registry,
                                                          LCRSTree& tree,
                                                          KeyManager& keyMgr,
                                                          AuditLog& auditLog) const {
        vector<StudentTerminalReceipt> receipts;                // collect one receipt per student roll
        for (const string& roll : studentRolls) {               // process each requested student roll
            string stuNodeId = "STU-" + roll;                   // build student terminal node ID
            if (registry.find(stuNodeId) == nullptr) {          // register student terminal if not yet in AVLRegistry
                registry.registerNode(stuNodeId, "Student Terminal " + roll, "Student", centreId, "Registered");
            }
            if (tree.getNode(stuNodeId) == nullptr) {           // attach student terminal under centreId in LCRS tree
                tree.addChild(centreId, stuNodeId, "Student Terminal " + roll, "Student");
            }

            bool ok = tree.deliverSingleChild(centreId, stuNodeId, registry, keyMgr, auditLog); // wrap L5 layer
            int bytes = 0;                                      // default 0 if delivery failed
            string fp = "";                                     // default empty fingerprint
            if (ok) {
                const LCRSNode* stuNode = tree.getNode(stuNodeId); // fetch delivered student node
                if (stuNode != nullptr) {
                    bytes = (int)stuNode->encryptedPayload.size(); // record 6-layer onion size
                    static const char hex[] = "0123456789abcdef"; // hex lookup table
                    for (size_t i = 0; i < 4 && i < stuNode->key.size(); i++) {
                        unsigned char b = (unsigned char)stuNode->key[i];
                        fp += hex[(b >> 4) & 0xF];
                        fp += hex[b & 0xF];
                    }
                    registry.registerNode(stuNodeId, "Student Terminal " + roll, "Student", centreId, "Registered", fp);
                }
            }
            receipts.push_back({stuNodeId, roll, centreId, ok, bytes, fp}); // record receipt
        }
        return receipts;                                        // return all student terminal receipts
    }
};
