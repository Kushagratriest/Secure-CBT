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

    string compress(string data) {
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
        buildCodes(root, "");

        // step 5 - encode the string
        string encoded = "";
        for (char c : data) {
            encoded += codes[c];
        }

        cout << "Original size : " << data.size() * 8 << " bits" << endl;
        cout << "Compressed size: " << encoded.size() << " bits" << endl;
        cout << "Compression done using Huffman coding" << endl;

        return encoded;
    }
};