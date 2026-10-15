#pragma once
#include <iostream>
#include <string>
#include <cstdlib>
#include <ctime>
using namespace std;

class KeyManager {
private:
    string rootKey;
//It generates a random string of characters of given length.
//rand() % 26 gives a number between 0 and 25, adding 65 converts it to ASCII uppercase letters (A=65, B=66 ... Z=90).
//So generateRandomKey(16) gives something like "KXQMBTPRZWLNVHCA" — 16 random uppercase letters every time.
    string generateRandomKey(int length) {
        string key = "";
        for (int i = 0; i < length; i++) {
            key += char(rand() % 26 + 65);
        }
        return key;
    }

public:
    KeyManager() {//every time you run the program, rand() gives different numbers,
        srand(time(0));
        cout << "KeyManager created" << endl;
    }

    string generateRootKey() {//Calls generateRandomKey(16) to create a 16-character random string, stores it in rootKey, prints it, and returns it.
        rootKey = generateRandomKey(16);
        cout << "Root key generated: " << rootKey << endl;
        return rootKey;
    }

    string generateNodeSecret() {
        string secret = generateRandomKey(16);
        cout << "Node secret generated: " << secret << endl;
        return secret;
    }


    //Takes the parent's key and the node's own secret, XORs them character by character to produce a new unique key
    string deriveChildKey(string parentKey, string nodeSecret) {
        string childKey = "";
        for (int i = 0; i < parentKey.size(); i++) {
            childKey += char(parentKey[i] ^ nodeSecret[i % nodeSecret.size()]);
        }
        cout << "Child key derived using XOR" << endl;
        return childKey;
    }

    string encrypt(string data, string key) {
        string encrypted = "";
        for (int i = 0; i < data.size(); i++) {
            encrypted += char(data[i] ^ key[i % key.size()]);
        }
        cout << "Data encrypted with key" << endl;
        return encrypted;
    }

    string decrypt(string data, string key) {
        cout << "Data decrypted with key" << endl;
        return encrypt(data, key);
    }

    string getRootKey() {
        return rootKey;
    }
};