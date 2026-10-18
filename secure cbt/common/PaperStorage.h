// Saves and loads the encrypted question paper as a single file
// (the "package" that Phase 1 hands to Phase 2 for distribution).
//
// Package file layout (binary, all numbers stored as 4 bytes, little-endian):
//
//   "SCBT"            4 bytes   magic word: proves this is our file type
//   version           1 byte    format version (currently 1)
//   bitCount          4 bytes   real bits in the compressed paper (last byte is padded)
//   tableSize         4 bytes   how many entries the Huffman code table has
//   table entries     each:  1 byte character, 1 byte code length, then the code as '0'/'1' bytes
//   payloadSize       4 bytes   how many encrypted bytes follow
//   payload           payloadSize bytes   the encrypted, compressed paper
//
// The KEY is NOT stored in the package. It is written to a separate file,
// because a secret must never travel together with the data it protects.
//
// Note: the code table is stored unencrypted. It only reveals how often each
// character appears, not the paper itself; the paper text stays encrypted.

#pragma once
#include <string>
#include <fstream>           // ifstream / ofstream for file reading and writing
#include <unordered_map>
#include <cstdint>           // uint32_t: a number that is exactly 4 bytes on every machine
using namespace std;

// Everything Phase 3 will need (apart from the key) to get the paper back.
struct PaperPackage {
    int bitCount = 0;                       // real bits in the compressed paper
    unordered_map<char, string> codes;      // Huffman code table, e.g. 'Q' -> "0110"
    string encrypted;                       // encrypted, compressed paper bytes
};

const string PACKAGE_MAGIC = "SCBT";        // first 4 bytes of every package file
const unsigned char PACKAGE_VERSION = 2;    // bump this if the layout ever changes (2 = AES-256-GCM payload)

// --- small helpers for writing/reading 4-byte numbers ---------------------
// Written byte by byte (lowest byte first) so the file looks the same on
// every computer, no matter how that computer stores numbers in memory.

inline void writeU32(ofstream& out, uint32_t value) {
    for (int i = 0; i < 4; i++) {                          // 4 bytes, one at a time
        out.put((char)((value >> (8 * i)) & 0xFF));        // take byte i of the number and write it
    }
}

inline bool readU32(ifstream& in, uint32_t& value) {
    value = 0;                                             // start from zero
    for (int i = 0; i < 4; i++) {                          // read 4 bytes
        int c = in.get();                                  // next byte, or EOF if the file ended
        if (c == EOF) return false;                        // file too short: report failure
        value |= ((uint32_t)(unsigned char)c) << (8 * i);  // put the byte back in position i
    }
    return true;
}

// --- save -------------------------------------------------------------------
// Writes the package to 'path'. Returns true on success.
// On failure returns false and puts a plain-language reason in 'error'.
inline bool savePaperPackage(const string& path, const PaperPackage& pkg, string& error) {
    ofstream out(path, ios::binary);                       // binary: write bytes exactly, no newline conversion
    if (!out.is_open()) {                                  // folder missing, no permission, etc.
        error = "Could not create " + path;
        return false;
    }

    out.write(PACKAGE_MAGIC.data(), 4);                    // "SCBT"
    out.put((char)PACKAGE_VERSION);                        // version byte
    writeU32(out, (uint32_t)pkg.bitCount);                 // how many real bits
    writeU32(out, (uint32_t)pkg.codes.size());             // how many table entries

    for (const auto& entry : pkg.codes) {                  // every character and its code
        if (entry.second.empty() || entry.second.size() > 255) {   // a code must fit in a 1-byte length
            error = "Invalid Huffman code length in table";
            return false;
        }
        out.put(entry.first);                              // the character
        out.put((char)entry.second.size());                // its code length
        out.write(entry.second.data(), entry.second.size());   // the code itself, e.g. "0110"
    }

    writeU32(out, (uint32_t)pkg.encrypted.size());         // how many encrypted bytes
    out.write(pkg.encrypted.data(), pkg.encrypted.size()); // the encrypted bytes

    if (!out.good()) {                                     // disk full or write error
        error = "Error while writing " + path;
        return false;
    }
    return true;
}

// --- load -------------------------------------------------------------------
// Reads a package from 'path' into 'pkg'. Returns true on success.
// Checks every part, so a damaged or wrong file is reported, not crashed on.
inline bool loadPaperPackage(const string& path, PaperPackage& pkg, string& error) {
    ifstream in(path, ios::binary);                        // open for reading bytes
    if (!in.is_open()) {
        error = "Could not open " + path;
        return false;
    }

    char magic[4];                                         // room for "SCBT"
    if (!in.read(magic, 4) || string(magic, 4) != PACKAGE_MAGIC) {   // missing or wrong magic word
        error = path + " is not a Secure CBT paper file";
        return false;
    }
    int version = in.get();                                // version byte
    if (version != PACKAGE_VERSION) {                      // made by a different version of the program
        error = "Unsupported package version";
        return false;
    }

    uint32_t bitCount = 0, tableSize = 0;
    if (!readU32(in, bitCount) || !readU32(in, tableSize) || tableSize > 256) {   // a char has only 256 values
        error = "Package header is damaged";
        return false;
    }

    pkg.codes.clear();                                     // start with an empty table
    for (uint32_t i = 0; i < tableSize; i++) {             // read each table entry
        int ch = in.get();                                 // the character
        int len = in.get();                                // its code length
        if (ch == EOF || len == EOF || len == 0) {
            error = "Code table is damaged";
            return false;
        }
        string code(len, '0');                             // space for the code
        if (!in.read(&code[0], len)) {                     // read the '0'/'1' bytes
            error = "Code table is damaged";
            return false;
        }
        pkg.codes[(char)ch] = code;                        // store it
    }

    uint32_t payloadSize = 0;
    if (!readU32(in, payloadSize) || payloadSize != (bitCount + 7) / 8 + 28) {   // AES-GCM adds 12-byte nonce + 16-byte tag = 28 bytes
        error = "Payload size does not match the bit count";
        return false;
    }
    pkg.encrypted.assign(payloadSize, '\0');               // space for the encrypted bytes
    if (payloadSize > 0 && !in.read(&pkg.encrypted[0], payloadSize)) {   // file ended too early
        error = "Encrypted paper is truncated";
        return false;
    }

    pkg.bitCount = (int)bitCount;                          // everything checked: fill in the bit count
    return true;
}

// --- key file ---------------------------------------------------------------
// DEMO ONLY: writes the root key to its own file so Phase 2/3 can use it.
// In the real system the root key stays on the central server and is
// never written next to the paper.
inline bool saveKeyFile(const string& path, const string& key, string& error) {
    ofstream out(path, ios::binary);                       // open (or create) the key file
    if (!out.is_open()) {
        error = "Could not create " + path;
        return false;
    }
    out.write(key.data(), key.size());                     // write the key bytes exactly
    if (!out.good()) {
        error = "Error while writing " + path;
        return false;
    }
    return true;
}
