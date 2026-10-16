// AuditLog.h: Hash-chained transfer log for Phase 2 (Secure Distribution).
// Every time the encrypted paper moves from one node to another (for example,
// Board -> State, or State -> Centre), a new LogEntry is added at the end.
// Each entry stores the SHA-256 hash of the entry before it (prevHash) and its
// own SHA-256 hash, so changing any old entry breaks the chain from that point on.

#pragma once                                                    // only include this header once per compile
#include <iostream>                                             // cout, endl for console output
#include <string>                                               // std::string for node names, timestamps, and hashes
#include <vector>                                               // std::vector to store the list of log entries in order
#include <ctime>                                                // time(), localtime(), strftime() for the timestamp
#include <sstream>                                              // ostringstream for formatting the 64-character hex hash
#include <iomanip>                                              // hex, setw, setfill for two-digit hex byte formatting
#include <stdexcept>                                            // runtime_error if OpenSSL fails to hash
#include <openssl/evp.h>                                        // OpenSSL EVP_MD_CTX and EVP_sha256() functions
using namespace std;                                            // use standard library names directly, matching the rest of the repo

// One record in the transfer audit log, representing a single hop between two nodes.
struct LogEntry {
    int index;                                                  // position in the log (0 for the first entry, 1 for the second, etc.)
    string fromNode;                                            // sender node ID or name (e.g. "BOARD" or "ST-01")
    string toNode;                                              // receiver node ID or name (e.g. "ST-01" or "ST-01-C01")
    string timestamp;                                           // when the hop happened, formatted as "YYYY-MM-DD HH:MM:SS"
    string prevHash;                                            // 64-char hex SHA-256 hash of the previous entry (64 zeros for entry 0)
    string hash;                                                // 64-char hex SHA-256 hash of (index + fromNode + toNode + timestamp + prevHash)
};

// Manages the list of transfer entries, computes their SHA-256 hashes, and checks for tampering.
class AuditLog {
private:
    vector<LogEntry> entries;                                   // all log entries in the order they were appended

    // Returns the current local time as a readable string ("YYYY-MM-DD HH:MM:SS").
    static string currentTimestamp() {
        time_t now = time(nullptr);                             // get current time in seconds since Jan 1, 1970
        tm* local = localtime(&now);                            // break it down into year, month, day, hour, minute, second
        if (local == nullptr) return "1970-01-01 00:00:00";     // fallback string in case the system clock call fails
        char buf[32];                                           // buffer big enough to hold "YYYY-MM-DD HH:MM:SS\0"
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", local); // format the date and time into buf
        return string(buf);                                     // return as a C++ string
    }

    // Computes the SHA-256 digest of 'input' using the OpenSSL EVP API and returns it as 64 lowercase hex characters.
    static string sha256(const string& input) {
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();                     // allocate a new OpenSSL hashing context
        if (ctx == nullptr) {                                   // out of memory
            throw runtime_error("OpenSSL: could not create EVP_MD_CTX");
        }

        unsigned char digest[EVP_MAX_MD_SIZE];                  // raw output buffer (SHA-256 uses 32 bytes)
        unsigned int digestLen = 0;                             // OpenSSL writes the actual byte count (32) here

        // initialize SHA-256, feed all input bytes, and finalize the 32-byte digest
        bool ok = (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) == 1)
               && (EVP_DigestUpdate(ctx, input.data(), input.size()) == 1)
               && (EVP_DigestFinal_ex(ctx, digest, &digestLen) == 1);

        EVP_MD_CTX_free(ctx);                                   // always free the OpenSSL context so memory never leaks
        if (!ok) {                                              // if any OpenSSL step returned an error
            throw runtime_error("OpenSSL: SHA-256 digest calculation failed");
        }

        ostringstream hexStream;                                // stream to convert raw bytes into readable hex text
        hexStream << hex << setfill('0');                       // print in base 16, padding single-digit bytes with a leading '0'
        for (unsigned int i = 0; i < digestLen; i++) {          // loop through all 32 bytes of the SHA-256 result
            hexStream << setw(2) << (int)digest[i];             // each byte becomes 2 hex characters (32 * 2 = 64 characters)
        }
        return hexStream.str();                                 // return the 64-character hex string
    }

    // Computes the expected SHA-256 hash for an entry from its five fields concatenated together.
    static string computeEntryHash(int index, const string& fromNode, const string& toNode,
                                   const string& timestamp, const string& prevHash) {
        // join index + fromNode + toNode + timestamp + prevHash and hash the combined string
        return sha256(to_string(index) + fromNode + toNode + timestamp + prevHash);
    }

public:
    // Creates an empty audit log with no entries yet.
    AuditLog() {}

    // Appends a new transfer hop (from -> to) to the end of the chain and computes its SHA-256 hash.
    const LogEntry& append(const string& from, const string& to) {
        LogEntry entry;                                         // create a blank log entry to fill in
        entry.index = (int)entries.size();                      // 0 for the first entry, 1 for the second, and so on
        entry.fromNode = from;                                  // record who sent the paper
        entry.toNode = to;                                      // record who received the paper
        entry.timestamp = currentTimestamp();                   // stamp the current date and time
        if (entries.empty()) {                                  // if this is the very first entry in the log (genesis entry)
            entry.prevHash = string(64, '0');                   // there is no previous entry, so prevHash is 64 zeros
        } else {                                                // for every entry after the first
            entry.prevHash = entries.back().hash;               // link to the hash of the entry right before this one
        }
        // hash (index + from + to + timestamp + prevHash) to seal this entry
        entry.hash = computeEntryHash(entry.index, entry.fromNode, entry.toNode,
                                      entry.timestamp, entry.prevHash);
        entries.push_back(entry);                               // store the finished entry at the back of the vector
        return entries.back();                                  // return a read-only reference to the newly added entry
    }

    // Recomputes every entry's hash and checks every link in the chain.
    // Returns the index of the FIRST broken entry, or -1 if the entire chain is intact.
    int verify() const {
        string expectedPrev = string(64, '0');                  // entry 0 must point back to 64 zeros
        for (size_t i = 0; i < entries.size(); i++) {           // walk through the chain from start to finish
            const LogEntry& e = entries[i];                     // current entry being checked
            if (e.index != (int)i) {                            // someone changed or reordered the entry's index number
                return (int)i;                                  // report this position as broken
            }
            if (e.prevHash != expectedPrev) {                   // prevHash does not match the previous entry's stored hash
                return (int)i;                                  // report this position as broken
            }
            // recompute SHA256(index + from + to + timestamp + prevHash) from the entry's current fields
            string recomputed = computeEntryHash(e.index, e.fromNode, e.toNode,
                                                 e.timestamp, e.prevHash);
            if (e.hash != recomputed) {                         // stored hash doesn't match: a field or the hash was tampered with
                return (int)i;                                  // report this position as the first broken entry
            }
            expectedPrev = e.hash;                              // the next entry's prevHash must equal this entry's hash
        }
        return -1;                                              // every entry matched: the chain is completely intact
    }

    // Read-only getter so the UI or server can display all log entries without modifying them.
    const vector<LogEntry>& getEntries() const {
        return entries;                                         // return constant reference to the internal vector
    }

    // Mutable getter used only by tests to simulate an attacker tampering with an existing entry in memory.
    vector<LogEntry>& getMutableEntries() {
        return entries;                                         // return non-const reference so a test can edit entries[i]
    }
};
