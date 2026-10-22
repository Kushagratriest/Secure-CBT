// LCRSTree.h: Left-Child Right-Sibling (LCRS) Hierarchy Tree & BFS Broadcast for Phase 2.
// Represents the distribution network (e.g. Board -> State -> District -> City -> Centre -> PC)
// using only two pointers per node: 'firstChild' and 'nextSibling'.
// Uses Breadth-First Search (BFS) with a queue to broadcast the encrypted paper level by level:
//   1. Checks AVLRegistry::isRegistered(childId) before sending.
//   2. Derives the child's unique 32-byte key from its parent's key via HKDF-SHA256.
//   3. Wraps the parent's payload in one more AES-256-GCM onion layer using the child's key.
//   4. Appends every successful hop to the SHA-256 hash-chained AuditLog.

#pragma once                                                    // include this header only once per compilation
#include <iostream>                                             // cout, endl for console output
#include <string>                                               // std::string for node IDs, keys, and payloads
#include <vector>                                               // std::vector for hop reports and path key lists
#include <queue>                                                // std::queue for Breadth-First Search (BFS) broadcast
#include "AVLRegistry.h"                                        // AVLRegistry to verify nodes before transferring
#include "KeyManager.h"                                         // KeyManager for HKDF key derivation and AES-256-GCM layering
#include "AuditLog.h"                                           // AuditLog for SHA-256 hash-chained hop recording
using namespace std;                                            // use standard library names directly

// One node inside the Left-Child Right-Sibling (LCRS) distribution tree.
struct LCRSNode {
    string nodeId;                                              // unique ID matching AVLRegistry (e.g. "BOARD", "ST-01", "ST-01-C01")
    string name;                                                // readable name (e.g. "Andhra Pradesh", "AP Centre 1")
    string role;                                                // hierarchy level ("Board", "State", "District", "City", "Centre", "PC")
    string key;                                                 // this node's 32-byte AES-256 key (rootKey for Board, HKDF child key otherwise)
    string encryptedPayload;                                    // the onion-encrypted paper stored at this node after delivery
    bool delivered;                                             // true if this node received its encrypted copy during broadcast

    LCRSNode* parent;                                           // pointer up to the parent node (nullptr for the root Board)
    LCRSNode* firstChild;                                       // pointer down to this node's FIRST child (nullptr if leaf)
    LCRSNode* nextSibling;                                      // pointer sideways to the NEXT child of the same parent (nullptr if last)

    // Constructor: creates a tree node with empty key/payload and null pointers.
    LCRSNode(const string& id, const string& nodeName, const string& nodeRole) {
        nodeId = id;                                            // store unique node ID
        name = nodeName;                                        // store readable node name
        role = nodeRole;                                        // store hierarchy level name
        key = "";                                               // key is assigned when the root is set or when the parent sends to it
        encryptedPayload = "";                                  // payload is filled when the broadcast reaches this node
        delivered = false;                                      // not delivered yet
        parent = nullptr;                                       // linked when attached to a parent
        firstChild = nullptr;                                   // no children yet
        nextSibling = nullptr;                                  // no siblings yet
    }
};

// Summary of one transfer attempt during the BFS broadcast (used by CLI and UI tables).
struct HopReport {
    int hopNumber;                                              // 1-based attempt number (1, 2, 3...)
    string fromId;                                              // sender node ID
    string toId;                                                // receiver node ID
    string status;                                              // "Delivered" if registered, or "Blocked (Not Registered)"
    int payloadBytes;                                           // size of the onion-encrypted payload at the receiver (0 if blocked)
};

// Left-Child Right-Sibling tree that manages the distribution hierarchy and runs BFS broadcast.
class LCRSTree {
private:
    LCRSNode* root;                                             // top of the hierarchy (the Examination Board)
    int totalNodes;                                             // count of nodes added to the tree

    // Recursive helper that searches the LCRS tree for a node with 'targetId'.
    LCRSNode* findNode(LCRSNode* current, const string& targetId) const {
        if (current == nullptr) return nullptr;                 // base case: empty branch
        if (current->nodeId == targetId) return current;        // found the node we are looking for

        // first search down into this node's children
        LCRSNode* foundInChild = findNode(current->firstChild, targetId);
        if (foundInChild != nullptr) return foundInChild;       // found inside the child subtree

        // otherwise search sideways across this node's siblings
        return findNode(current->nextSibling, targetId);        // check the next sibling and its subtrees
    }

    // Recursive helper that frees every node in the LCRS tree so no memory leaks.
    void freeSubtree(LCRSNode* current) {
        if (current == nullptr) return;                         // base case: nothing to free
        freeSubtree(current->firstChild);                       // free all children first
        freeSubtree(current->nextSibling);                      // free all siblings second
        delete current;                                         // delete this node last
    }

public:
    // Creates an empty LCRS distribution tree.
    LCRSTree() {
        root = nullptr;                                         // no root node yet
        totalNodes = 0;                                         // 0 nodes in tree
    }

    // Destructor: frees all allocated LCRSNode objects when the tree is destroyed.
    ~LCRSTree() {
        freeSubtree(root);                                      // clean up all heap memory
    }

    // Prevent accidental shallow copies of raw tree pointers.
    LCRSTree(const LCRSTree&) = delete;
    LCRSTree& operator=(const LCRSTree&) = delete;

    // Sets the root node of the hierarchy (e.g. "BOARD", "Examination Board", "Board").
    bool setRoot(const string& id, const string& name, const string& role = "Board") {
        if (root != nullptr) return false;                      // root already exists; do not overwrite it
        root = new LCRSNode(id, name, role);                    // allocate the root node
        totalNodes = 1;                                         // tree now has 1 node
        return true;                                            // root created successfully
    }

    // Adds a new child node under 'parentId' using the Left-Child Right-Sibling pointer rule:
    // - If the parent has no children yet, the new node becomes parent->firstChild.
    // - Otherwise, we walk to the end of parent->firstChild's nextSibling chain and attach it there.
    bool addChild(const string& parentId, const string& childId,
                  const string& childName, const string& childRole) {
        if (root == nullptr) return false;                      // cannot add a child before a root exists
        if (findNode(root, childId) != nullptr) return false;   // duplicate childId: already in the tree

        LCRSNode* parentNode = findNode(root, parentId);        // locate the parent in the tree
        if (parentNode == nullptr) return false;                // parentId does not exist in the tree

        LCRSNode* child = new LCRSNode(childId, childName, childRole); // allocate the new child node
        child->parent = parentNode;                             // link child back up to its parent

        if (parentNode->firstChild == nullptr) {                // parent has no children yet
            parentNode->firstChild = child;                     // attach directly as the first (leftmost) child
        } else {                                                // parent already has at least one child
            LCRSNode* sib = parentNode->firstChild;             // start at the first child
            while (sib->nextSibling != nullptr) {               // walk right along the sibling chain until the last one
                sib = sib->nextSibling;                         // move to the next sibling
            }
            sib->nextSibling = child;                           // attach the new child at the end of the sibling list
        }

        totalNodes++;                                           // count one more node in the tree
        return true;                                            // child added successfully
    }

    // Looks up a node in the LCRS tree by ID (read-only). Returns nullptr if not found.
    const LCRSNode* getNode(const string& id) const {
        return findNode(root, id);                              // search starting from the root
    }

    // Returns the total number of nodes in the hierarchy tree.
    int size() const {
        return totalNodes;                                      // number of nodes in tree
    }

    // Runs Breadth-First Search (BFS) starting from the root to broadcast the encrypted paper level by level.
    // At every hop (parent -> child):
    //   1. Checks registry.isRegistered(child->nodeId). If false, blocks the hop and skips its subtree.
    //   2. Derives child->key = keyMgr.deriveChildKey(parent->key, child->nodeId).
    //   3. Adds an onion layer: child->encryptedPayload = keyMgr.encrypt(parent->encryptedPayload, child->key).
    //   4. Appends (parent->nodeId, child->nodeId) to auditLog.
    vector<HopReport> bfsBroadcast(const string& rootEncryptedPaper,
                                   const string& rootKey,
                                   const AVLRegistry& registry,
                                   KeyManager& keyMgr,
                                   AuditLog& auditLog) {
        vector<HopReport> reports;                              // list of hop outcomes to return to the caller
        if (root == nullptr) return reports;                    // empty tree: nothing to broadcast

        // Check that the root node itself (e.g. "BOARD") is registered before starting
        if (!registry.isRegistered(root->nodeId)) {             // root is missing or not "Registered"
            reports.push_back({1, "(init)", root->nodeId, "Blocked (Root Not Registered)", 0});
            return reports;                                     // abort broadcast
        }

        // Load the Phase 1 locked paper and root key into the Board (root) node
        root->key = rootKey;                                    // Board holds the root key (L0)
        root->encryptedPayload = rootEncryptedPaper;            // Board starts with the L0-encrypted paper from Phase 1
        root->delivered = true;                                 // Board now has the package ready to distribute

        queue<LCRSNode*> bfsQueue;                              // FIFO queue for level-by-level Breadth-First Search
        bfsQueue.push(root);                                    // start BFS at the Board (level 0)
        int hopCounter = 0;                                     // counts every transfer attempt in order

        while (!bfsQueue.empty()) {                             // keep going until every reachable level is processed
            LCRSNode* sender = bfsQueue.front();                // take the next parent node from the front of the queue
            bfsQueue.pop();                                     // remove it from the queue

            // Walk across all children of 'sender' using firstChild and nextSibling pointers
            for (LCRSNode* child = sender->firstChild; child != nullptr; child = child->nextSibling) {
                hopCounter++;                                   // increment the 1-based hop counter

                // Gatekeeper check: ask the AVL tree if this child is authorised ("Registered")
                if (!registry.isRegistered(child->nodeId)) {    // child is Pending, Rejected, or missing from registry
                    child->delivered = false;                   // mark as not delivered
                    reports.push_back({hopCounter, sender->nodeId, child->nodeId,
                                       "Blocked (Not Registered)", 0}); // record the blocked attempt
                    continue;                                   // do NOT derive a key, do NOT send, and do NOT push to queue
                }

                // Child is verified: derive its unique 32-byte key from its parent's key + its own nodeId
                child->key = keyMgr.deriveChildKey(sender->key, child->nodeId);

                // Onion layering: wrap the sender's encrypted payload in one more AES-256-GCM layer with child->key
                child->encryptedPayload = keyMgr.encrypt(sender->encryptedPayload, child->key);
                child->delivered = true;                        // mark delivery complete for this node

                // Record the verified transfer hop in the SHA-256 hash-chained audit log
                auditLog.append(sender->nodeId, child->nodeId);

                // Save a summary row for the console/UI table
                reports.push_back({hopCounter, sender->nodeId, child->nodeId,
                                   "Delivered", (int)child->encryptedPayload.size()});

                // Push this child onto the BFS queue so its own children (next level down) are visited next
                bfsQueue.push(child);
            }
        }

        return reports;                                         // return the complete level-order list of hop results
    }

    // Unwraps the onion-encrypted payload at 'targetNodeId' from the outside in (leaf -> ... -> parent -> root)
    // and puts the recovered compressed bytes into 'outCompressed'.
    // Returns true if every single AES-256-GCM layer along the path is authentic and intact.
    bool unwrapAtNode(const string& targetNodeId, KeyManager& keyMgr, string& outCompressed) const {
        const LCRSNode* node = findNode(root, targetNodeId);    // find the target node (e.g. a Centre or Student PC)
        if (node == nullptr || !node->delivered) return false;  // node doesn't exist or never received the paper

        string currentData = node->encryptedPayload;            // start with the outermost onion ciphertext at this node
        const LCRSNode* cursor = node;                          // walk up the parent pointers from leaf to root

        while (cursor != nullptr) {                             // peel one layer per ancestor all the way up to the Board
            string peeled;                                      // buffer to hold the payload after removing this node's layer
            if (!keyMgr.decryptChecked(currentData, cursor->key, peeled)) { // verify GCM tag and decrypt this layer
                return false;                                   // wrong key or tampered bytes at this layer!
            }
            currentData = peeled;                               // move inward to the next layer underneath
            cursor = cursor->parent;                            // step up to the parent node
        }

        outCompressed = currentData;                            // after peeling the Board's L0 layer, we have the compressed paper
        return true;                                            // all layers unlocked cleanly
    }
};
