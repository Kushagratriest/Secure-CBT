//this file does the halfman compression 

#pragma once
#include <iostream>
#include <string>
#include <queue>
#include <unordered_map>
using namespace std;

struct HuffmanNode {//every node in the huffman tree has a character, its frequency, and pointers to left and right children.
    char ch;
    int freq;
    HuffmanNode* left;
    HuffmanNode* right;

    HuffmanNode(char c, int f) {//constructor
        ch = c;
        freq = f;
        left = nullptr;
        right = nullptr;
    }
};

struct Compare {//By default a priority_queue in C++ makes a max-heap — biggest on top. We want smallest frequency on top. So Compare flips th
    bool operator()(HuffmanNode* a, HuffmanNode* b) {
        return a->freq > b->freq;
    }
};

class HuffmanCompressor {
private:
    unordered_map<char, string> codes;
    int bitCount = 0;   // how many real bits the last compress() produced (the last byte may have padding)

    void buildCodes(HuffmanNode* root, string path) {
        if (root == nullptr) return;

        if (root->left == nullptr && root->right == nullptr) {
            codes[root->ch] = path;
            return;
        }

        buildCodes(root->left, path + "0");
        buildCodes(root->right, path + "1");
    }

public:
    HuffmanCompressor() {
        cout << "HuffmanCompressor created" << endl;
    }

    // Number of meaningful bits in the last compressed output.
    // Needed later to decompress, because the last byte is padded with 0s.
    int getBitCount() const { return bitCount; }

    // Returns the code table from the last compress() call, e.g. 'Q' -> "01".
    // Needed later to decompress.
    unordered_map<char, string> getCodes() const { return codes; }

    string compress(string data) {
        codes.clear();          // forget codes from any earlier call so they don't mix with this one
        bitCount = 0;           // reset the bit counter for this call

        // edge case: nothing to compress (minHeap.top() below would crash on an empty heap)
        if (data.empty()) {
            cout << "Nothing to compress (empty input)" << endl;
            return "";          // empty in, empty out
        }

        // step 1 - frequency map
        unordered_map<char, int> freq;
        for (char c : data) {
            freq[c]++;
        }

        // step 2 - build min heap
        priority_queue<HuffmanNode*, vector<HuffmanNode*>, Compare> minHeap;
        for (auto pair : freq) {
            minHeap.push(new HuffmanNode(pair.first, pair.second));
        }

        // step 3 - build huffman tree
        while (minHeap.size() > 1) {
            HuffmanNode* left = minHeap.top(); minHeap.pop();
            HuffmanNode* right = minHeap.top(); minHeap.pop();

            HuffmanNode* parent = new HuffmanNode('\0', left->freq + right->freq);
            parent->left = left;
            parent->right = right;

            minHeap.push(parent);
        }

        HuffmanNode* root = minHeap.top();

        // step 4 - assign codes by traversing tree
        if (root->left == nullptr && root->right == nullptr) {   // only one distinct character (e.g. "aaaa"): root is a leaf
            codes[root->ch] = "0";                              // give it a 1-bit code; otherwise its code would be "" and the output empty
        } else {
            buildCodes(root, "");                               // normal case: walk the tree, left = 0, right = 1
        }

        // step 5 - encode the string, packing 8 bits into each byte
        string packed = "";             // the compressed bytes go here
        unsigned char currentByte = 0;  // the byte we are filling right now
        int bitsInByte = 0;             // how many bits are already in currentByte (0-7)

        for (char c : data) {                           // for every character in the input
            for (char bit : codes[c]) {                 // for every '0'/'1' in its code
                currentByte = currentByte << 1;         // shift existing bits left to make room
                if (bit == '1') currentByte |= 1;       // put a 1 in the lowest position (a '0' leaves it 0)
                bitsInByte++;                           // one more bit stored
                bitCount++;                             // one more bit overall
                if (bitsInByte == 8) {                  // byte is full
                    packed += (char)currentByte;        // save it
                    currentByte = 0;                    // start a fresh byte
                    bitsInByte = 0;
                }
            }
        }
        if (bitsInByte > 0) {                           // leftover bits that didn't fill a whole byte
            currentByte = currentByte << (8 - bitsInByte);  // push them to the left; the right side is 0 padding
            packed += (char)currentByte;                // save the last, partly filled byte
        }

        cout << "Original size : " << data.size() * 8 << " bits" << endl;
        cout << "Compressed size: " << bitCount << " bits (" << packed.size() << " bytes)" << endl;   // real bits + bytes actually stored
        cout << "Compression done using Huffman coding" << endl;

        return packed;          // real binary bytes, about bitCount/8 long
    }
};