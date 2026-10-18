#pragma once
#include <iostream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <sstream>                                              // ostringstream, used to print key fingerprints as hex
#include <iomanip>                                              // hex, setw, setfill for two-digit hex bytes
#include <stdexcept>                                            // runtime_error / invalid_argument when OpenSSL or the key is bad
#include <openssl/evp.h>                                        // AES-256-GCM cipher functions
#include <openssl/rand.h>                                       // RAND_bytes: cryptographically secure random numbers
#include <openssl/kdf.h>                                        // EVP_KDF: HKDF key derivation
#include <openssl/core_names.h>                                 // names of the HKDF settings (digest, key, info)
#include <openssl/params.h>                                     // OSSL_PARAM: how settings are passed to EVP_KDF
using namespace std;

const int AES_KEY_BYTES = 32;                                   // AES-256 needs a 32-byte (256-bit) key
const int GCM_NONCE_BYTES = 12;                                 // GCM's standard nonce ("number used once") size
const int GCM_TAG_BYTES = 16;                                   // GCM's authentication tag size (detects tampering)

class KeyManager {
private:
    string rootKey;                                             // the paper's root key (32 raw bytes, never printed in full)

    // Returns 'length' secure random bytes from OpenSSL (replaces the old rand() letters, which were guessable).
    string generateRandomKey(int length) {
        string key(length, '\0');                               // space for 'length' bytes
        if (RAND_bytes((unsigned char*)&key[0], length) != 1) { // fill it with secure random bytes
            throw runtime_error("OpenSSL: could not generate random bytes");
        }
        return key;                                             // 32 random bytes when called with AES_KEY_BYTES
    }

    // Turns raw bytes into readable hex text (raw key bytes are not printable).
    static string toHex(const string& bytes) {
        ostringstream out;                                      // stream that builds the hex text
        out << hex << setfill('0');                             // base 16, pad single digits with 0
        for (unsigned char c : bytes) out << setw(2) << (int)c; // each byte becomes 2 hex characters
        return out.str();                                       // e.g. "2df5a65d..."
    }

public:
    KeyManager() {//every time you run the program, rand() gives different numbers,
        srand(time(0));
        cout << "KeyManager created" << endl;
    }

    string generateRootKey() {//Creates a fresh 32-byte random root key, stores it in rootKey, prints only its first 8 hex characters, and returns it.
        rootKey = generateRandomKey(AES_KEY_BYTES);             // 32 random bytes = a full AES-256 key
        cout << "Root key generated (fingerprint: " << toHex(rootKey).substr(0, 8) << ")" << endl;   // show a short fingerprint, never the whole key
        return rootKey;
    }

    string generateNodeSecret() {
        string secret = generateRandomKey(AES_KEY_BYTES);       // 32 random bytes private to one node
        cout << "Node secret generated (fingerprint: " << toHex(secret).substr(0, 8) << ")" << endl;
        return secret;
    }


    //Takes the parent's key and the node's own secret and runs HKDF-SHA256 on them to produce a new unique 32-byte child key.
    //One-way: the child key cannot be turned back into the parent key.
    string deriveChildKey(string parentKey, string nodeSecret) {
        if (parentKey.empty() || nodeSecret.empty()) {          // HKDF needs both inputs
            throw invalid_argument("deriveChildKey: parentKey and nodeSecret must not be empty");
        }
        unsigned char out[AES_KEY_BYTES];                       // the derived key lands here
        EVP_KDF* kdf = EVP_KDF_fetch(nullptr, "HKDF", nullptr); // ask OpenSSL for the HKDF algorithm
        if (kdf == nullptr) throw runtime_error("OpenSSL: HKDF not available");
        EVP_KDF_CTX* kctx = EVP_KDF_CTX_new(kdf);               // working context for one derivation
        EVP_KDF_free(kdf);                                      // the context keeps its own reference, so free ours
        if (kctx == nullptr) throw runtime_error("OpenSSL: could not create HKDF context");

        OSSL_PARAM params[4];                                   // the HKDF settings
        params[0] = OSSL_PARAM_construct_utf8_string(OSSL_KDF_PARAM_DIGEST, (char*)"SHA256", 0);                          // hash function inside HKDF
        params[1] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_KEY, (void*)parentKey.data(), parentKey.size());     // input key = parent's key
        params[2] = OSSL_PARAM_construct_octet_string(OSSL_KDF_PARAM_INFO, (void*)nodeSecret.data(), nodeSecret.size());  // node-specific input
        params[3] = OSSL_PARAM_construct_end();                 // marks the end of the list

        int ok = EVP_KDF_derive(kctx, out, AES_KEY_BYTES, params);   // run HKDF, write 32 bytes
        EVP_KDF_CTX_free(kctx);                                 // always free the context
        if (ok != 1) throw runtime_error("OpenSSL: HKDF derivation failed");

        cout << "Child key derived using HKDF-SHA256" << endl;
        return string((char*)out, AES_KEY_BYTES);               // return the 32 key bytes
    }

    //Encrypts with AES-256-GCM. Output layout: nonce (12 bytes) + ciphertext (same length as data) + tag (16 bytes).
    string encrypt(string data, string key) {
        if (key.size() != AES_KEY_BYTES) {                      // AES-256 only accepts a 32-byte key
            throw invalid_argument("encrypt: key must be exactly 32 bytes");
        }
        unsigned char nonce[GCM_NONCE_BYTES];                   // a fresh nonce for every encryption
        if (RAND_bytes(nonce, GCM_NONCE_BYTES) != 1) throw runtime_error("OpenSSL: could not generate nonce");

        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();             // working context for one encryption
        if (ctx == nullptr) throw runtime_error("OpenSSL: could not create cipher context");

        string cipher(data.size(), '\0');                       // ciphertext is the same length as the plaintext
        unsigned char tag[GCM_TAG_BYTES];                       // authentication tag is written here
        int len = 0, finalLen = 0;                              // OpenSSL reports how many bytes it wrote
        unsigned char finalBuf[16];                             // GCM writes 0 bytes here, but OpenSSL needs a buffer

        bool ok = EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1                    // choose AES-256-GCM
               && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, GCM_NONCE_BYTES, nullptr) == 1               // tell it our nonce is 12 bytes
               && EVP_EncryptInit_ex(ctx, nullptr, nullptr, (const unsigned char*)key.data(), nonce) == 1;      // load key and nonce
        if (ok && !data.empty()) {                              // skip the update step for empty data
            ok = EVP_EncryptUpdate(ctx, (unsigned char*)&cipher[0], &len, (const unsigned char*)data.data(), (int)data.size()) == 1;   // encrypt the data
        }
        ok = ok && EVP_EncryptFinal_ex(ctx, finalBuf, &finalLen) == 1                                           // finish encryption
                && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, GCM_TAG_BYTES, tag) == 1;                     // read the authentication tag
        EVP_CIPHER_CTX_free(ctx);                               // always free the context
        if (!ok) throw runtime_error("OpenSSL: AES-256-GCM encryption failed");

        cout << "Data encrypted with AES-256-GCM" << endl;
        return string((char*)nonce, GCM_NONCE_BYTES) + cipher + string((char*)tag, GCM_TAG_BYTES);   // nonce + ciphertext + tag
    }

    //Decrypts AES-256-GCM data. Returns true and fills 'plain' only if the key is right AND nothing was changed.
    bool decryptChecked(const string& data, const string& key, string& plain) {
        if (key.size() != AES_KEY_BYTES) return false;          // wrong key length
        if (data.size() < (size_t)(GCM_NONCE_BYTES + GCM_TAG_BYTES)) return false;   // too short to hold nonce + tag

        const unsigned char* nonce = (const unsigned char*)data.data();                              // first 12 bytes
        size_t cipherLen = data.size() - GCM_NONCE_BYTES - GCM_TAG_BYTES;                            // bytes in the middle
        const unsigned char* cipher = nonce + GCM_NONCE_BYTES;                                       // ciphertext starts after the nonce
        unsigned char tag[GCM_TAG_BYTES];                       // copy of the tag (OpenSSL wants a writable pointer)
        for (int i = 0; i < GCM_TAG_BYTES; i++) tag[i] = (unsigned char)data[data.size() - GCM_TAG_BYTES + i];   // last 16 bytes

        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();             // working context for one decryption
        if (ctx == nullptr) return false;

        string out(cipherLen, '\0');                            // plaintext has the same length as the ciphertext
        int len = 0, finalLen = 0;
        unsigned char finalBuf[16];

        bool ok = EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) == 1
               && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, GCM_NONCE_BYTES, nullptr) == 1
               && EVP_DecryptInit_ex(ctx, nullptr, nullptr, (const unsigned char*)key.data(), nonce) == 1;
        if (ok && cipherLen > 0) {                              // skip the update step for empty data
            ok = EVP_DecryptUpdate(ctx, (unsigned char*)&out[0], &len, cipher, (int)cipherLen) == 1;
        }
        ok = ok && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, GCM_TAG_BYTES, tag) == 1   // give OpenSSL the expected tag
                && EVP_DecryptFinal_ex(ctx, finalBuf, &finalLen) == 1;                       // fails if the tag does not match
        EVP_CIPHER_CTX_free(ctx);
        if (!ok) return false;                                  // wrong key or tampered data

        plain = out;                                            // authentic: hand back the plaintext
        return true;
    }

    //Same job as before (decrypt with a key), but now returns "" and prints a warning if the key is wrong or the data was changed.
    string decrypt(string data, string key) {
        string plain;
        if (!decryptChecked(data, key, plain)) {                // check failed
            cout << "Decryption FAILED (wrong key or data was tampered with)" << endl;
            return "";
        }
        cout << "Data decrypted with AES-256-GCM" << endl;
        return plain;
    }

    string getRootKey() {
        return rootKey;
    }
};
