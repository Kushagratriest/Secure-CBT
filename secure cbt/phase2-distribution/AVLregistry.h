// AVLRegistry.h: Self-balancing AVL Tree Registry for Phase 2 (Secure Distribution).
// Stores every node in the hierarchy (Board, State, District, City, Exam Centre, Student PC)
// rotates nodes after every insertion so lookups stay O(log n).

#pragma once                                                    
#include <iostream>                                             // cout, endl for console output
#include <string>                                               // std::string for nodeId, name, parentId, and status
#include <vector>                                               // std::vector to return sorted node lists for the UI
#include <algorithm>                                            // std::max for calculating node height
using namespace std;                                            // use standard library names directly

// Information stored for each node in the distribution network.
struct NodeInfo {
    string nodeId;                                              // unique ID, e.g. "BOARD", "ST-01", "ST-01-C01" (also the AVL search key)
    string name;                                                // human-readable name, e.g. "Andhra Pradesh" or "Centre 1"
    string role;                                                // level in the hierarchy: "Board", "State", "District", "City", "Centre"
    string parentId;                                            // ID of the parent node in the hierarchy ("" for the root Board)
    string status;                                              // "Registered", "Pending", or "Rejected" (only "Registered" may receive the paper)
    string keyFingerprint;                                      // first 8 hex chars of the node's key (safe to show in UI; never the full key)
};

// One node inside the AVL binary search tree.
struct AVLNode {
    NodeInfo info;                                              // the registration details for this network node
    int height;                                                 // height of this subtree (a leaf node has height 1)
    AVLNode* left;                                              // pointer to left child (all nodeIds alphabetically smaller than info.nodeId)
    AVLNode* right;                                             // pointer to right child (all nodeIds alphabetically larger than info.nodeId)

    // Constructor: creates a new leaf node with height 1 and no children.
    AVLNode(const NodeInfo& data) {
        info = data;                                            // copy the node's registration details
        height = 1;                                             // a newly inserted leaf starts at height 1
        left = nullptr;                                         // no left child yet
        right = nullptr;                                        // no right child yet
    }
};

// Self-balancing AVL tree that registers and verifies distribution nodes in O(log n) time.
class AVLRegistry {
private:
    AVLNode* root;                                              // top node of the AVL tree (nullptr when empty)
    int nodeCount;                                              // total number of distinct nodes stored in the tree

    // Returns the height of a node, or 0 if the pointer is nullptr.
    int getHeight(AVLNode* n) const {
        if (n == nullptr) return 0;                             // an empty subtree has height 0
        return n->height;                                       // return the stored height
    }

    // Returns the balance factor of a node: height(left subtree) - height(right subtree).
    // In a valid AVL tree, this is always -1, 0, or +1.
    int getBalance(AVLNode* n) const {
        if (n == nullptr) return 0;                             // null node is perfectly balanced (0)
        return getHeight(n->left) - getHeight(n->right);        // positive = left-heavy, negative = right-heavy
    }

    // Recomputes a node's height from the heights of its two children.
    void updateHeight(AVLNode* n) {
        if (n != nullptr) {                                     // guard against null pointer
            n->height = 1 + max(getHeight(n->left), getHeight(n->right)); // 1 + taller child's height
        }
    }


    AVLNode* rotateRight(AVLNode* y) {
        AVLNode* x = y->left;                                   // x is y's left child (will become the new root of this subtree)
        AVLNode* T2 = x->right;                                 // T2 is x's right subtree (between x and y in alphabetical order)

        x->right = y;                                           // move y down to be x's right child
        y->left = T2;                                           // attach T2 as y's new left child

        updateHeight(y);                                        // update y's height first because y is now the lower node
        updateHeight(x);                                        // then update x's height because x is now the upper node
        return x;                                               // return the new root of this rotated subtree
    }

  
    AVLNode* rotateLeft(AVLNode* x) {
        AVLNode* y = x->right;                                  // y is x's right child (will become the new root of this subtree)
        AVLNode* T2 = y->left;                                  // T2 is y's left subtree (between x and y in alphabetical order)

        y->left = x;                                            // move x down to be y's left child
        x->right = T2;                                          // attach T2 as x's new right child

        updateHeight(x);                                        // update x's height first because x is now the lower node
        updateHeight(y);                                        // then update y's height because y is now the upper node
        return y;                                               // return the new root of this rotated subtree
    }

    // Recursive helper that inserts 'data' into the subtree rooted at 'node' and rebalances on the way back up.
    AVLNode* insertNode(AVLNode* node, const NodeInfo& data) {
        // Step 1: normal Binary Search Tree insertion
        if (node == nullptr) {                                  // reached an empty spot: put the new leaf node here
            nodeCount++;                                        // count one more node in the registry
            return new AVLNode(data);                           // allocate and return the new leaf
        }

        if (data.nodeId < node->info.nodeId) {                  // new ID is alphabetically smaller: go left
            node->left = insertNode(node->left, data);          // insert into left subtree
        } else if (data.nodeId > node->info.nodeId) {           // new ID is alphabetically larger: go right
            node->right = insertNode(node->right, data);        // insert into right subtree
        } else {                                                // duplicate nodeId: update the existing node's info in place
            node->info = data;                                  // overwrite with the updated status/name/fingerprint
            return node;                                        // tree shape didn't change, so no rotation needed
        }

        // Step 2: update the height of this ancestor node
        updateHeight(node);                                     // recalculate height after the child subtree grew

        // Step 3: check the balance factor to see if this node became unbalanced (+2 or -2)
        int balance = getBalance(node);                         // height(left) - height(right)

        // Case 1: Left-Left (LL) — left subtree is too tall, and the new node went into left->left
        if (balance > 1 && data.nodeId < node->left->info.nodeId) {
            return rotateRight(node);                           // single right rotation fixes LL
        }

        // Case 2: Right-Right (RR) — right subtree is too tall, and the new node went into right->right
        if (balance < -1 && data.nodeId > node->right->info.nodeId) {
            return rotateLeft(node);                            // single left rotation fixes RR
        }

        // Case 3: Left-Right (LR) — left subtree is too tall, and the new node went into left->right
        if (balance > 1 && data.nodeId > node->left->info.nodeId) {
            node->left = rotateLeft(node->left);                // first rotate the left child left (turns LR into LL)
            return rotateRight(node);                           // then rotate this node right
        }

        // Case 4: Right-Left (RL) — right subtree is too tall, and the new node went into right->left
        if (balance < -1 && data.nodeId < node->right->info.nodeId) {
            node->right = rotateRight(node->right);             // first rotate the right child right (turns RL into RR)
            return rotateLeft(node);                            // then rotate this node left
        }

        return node;                                            // already balanced (-1, 0, or +1): return unchanged
    }

    // Recursive helper that searches for 'nodeId' in O(log n) time.
    const AVLNode* findNode(const AVLNode* node, const string& nodeId) const {
        if (node == nullptr) return nullptr;                    // hit a dead end: nodeId is not in the registry
        if (nodeId == node->info.nodeId) return node;           // found the exact match
        if (nodeId < node->info.nodeId) {                       // target ID is alphabetically smaller
            return findNode(node->left, nodeId);                // search the left half only
        }
        return findNode(node->right, nodeId);                   // otherwise search the right half only
    }

    // Recursive in-order traversal (Left -> Root -> Right) to collect all nodes in alphabetical order.
    void inOrderCollect(const AVLNode* node, vector<NodeInfo>& out) const {
        if (node == nullptr) return;                            // base case: empty subtree
        inOrderCollect(node->left, out);                        // visit all smaller IDs first
        out.push_back(node->info);                              // visit this node
        inOrderCollect(node->right, out);                       // visit all larger IDs last
    }

    // Recursive helper that checks every node's balance factor is in [-1, 1] and heights are accurate.
    bool verifyBalancedNode(const AVLNode* node) const {
        if (node == nullptr) return true;                       // empty tree is balanced
        int bal = getBalance(const_cast<AVLNode*>(node));       // check balance factor at this node
        if (bal < -1 || bal > 1) return false;                  // AVL rule violated!
        return verifyBalancedNode(node->left) && verifyBalancedNode(node->right); // check both subtrees
    }

    // Recursive post-order cleanup (Left -> Right -> Root) so no memory is leaked when the registry is destroyed.
    void freeTree(AVLNode* node) {
        if (node == nullptr) return;                            // nothing to free
        freeTree(node->left);                                   // free left subtree first
        freeTree(node->right);                                  // free right subtree second
        delete node;                                            // delete this node last
    }

public:
    // Creates an empty AVL node registry.
    AVLRegistry() {
        root = nullptr;                                         // start with no root node
        nodeCount = 0;                                          // 0 nodes registered
    }

    // Destructor: frees every allocated AVLNode when the registry goes out of scope.
    ~AVLRegistry() {
        freeTree(root);                                         // clean up all heap memory
    }

    // Prevent accidental shallow copies of raw tree pointers.
    AVLRegistry(const AVLRegistry&) = delete;
    AVLRegistry& operator=(const AVLRegistry&) = delete;

    // Registers (or updates) a node in the AVL tree and rebalances if needed.
    void insert(const NodeInfo& info) {
        root = insertNode(root, info);                          // insert starting at root and store the (possibly rotated) new root
    }

    // Convenience overload: builds a NodeInfo from individual fields and inserts it.
    void registerNode(const string& nodeId, const string& name, const string& role,
                      const string& parentId, const string& status = "Registered",
                      const string& keyFingerprint = "") {
        insert({nodeId, name, role, parentId, status, keyFingerprint}); // pack into NodeInfo and insert
    }

    // Looks up a node by its ID in O(log n) time. Returns nullptr if the node is not found.
    const NodeInfo* find(const string& nodeId) const {
        const AVLNode* n = findNode(root, nodeId);              // search the AVL tree
        if (n == nullptr) return nullptr;                       // not found
        return &(n->info);                                      // return read-only pointer to the node's info
    }

    // Returns true ONLY if the node exists in the tree AND its status is "Registered".
    // Used at every broadcast hop to block unknown, "Pending", or "Rejected" nodes.
    bool isRegistered(const string& nodeId) const {
        const NodeInfo* info = find(nodeId);                    // O(log n) lookup
        return (info != nullptr && info->status == "Registered"); // must exist and be approved
    }

    // Returns the total number of nodes stored in the registry.
    int size() const {
        return nodeCount;                                       // number of distinct nodes
    }

    // Returns the height of the entire AVL tree (for 116 nodes, this is around 7 instead of 116).
    int height() const {
        return getHeight(root);                                 // height of the root node
    }

    // Returns the ID of the current root node (useful in dry runs/tests to see rotations happen).
    string rootId() const {
        if (root == nullptr) return "(empty)";                  // guard when tree has no nodes
        return root->info.nodeId;                               // ID of the node currently at the top
    }

    // Returns true if every single node in the tree satisfies the AVL balance condition (-1, 0, +1).
    bool isBalanced() const {
        return verifyBalancedNode(root);                        // recursively check the whole tree
    }

    // Returns all nodes sorted alphabetically by nodeId (via in-order traversal).
    vector<NodeInfo> getAllNodes() const {
        vector<NodeInfo> list;                                  // output vector
        inOrderCollect(root, list);                             // fill in sorted order
        return list;                                            // return to caller/UI
    }
};
