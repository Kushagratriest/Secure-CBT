// IndiaNetwork.h: Populates the All-India Examination Distribution Hierarchy.
// Registers the National Examination Board (root), all 28 Indian States + 8 Union Territories
// (ST-01 through ST-36), and District -> City -> Exam Centre chains for key regions.

#pragma once                                                    // include this header only once per compilation
#include <string>                                               // std::string for node IDs and names
#include "AVLRegistry.h"                                        // AVLRegistry::registerNode()
#include "LCRSTree.h"                                           // LCRSTree::setRoot(), addChild()
using namespace std;                                            // use standard library names directly

// Helper: registers a node in the AVLRegistry and attaches it to the LCRSTree.
inline void addIndiaNode(AVLRegistry& reg, LCRSTree& tree,
                         const string& parentId, const string& nodeId,
                         const string& name, const string& role, const string& status) {
    reg.registerNode(nodeId, name, role, parentId, status);     // record node in AVL tree
    if (parentId.empty()) {                                     // empty parentId means root node
        tree.setRoot(nodeId, name, role);                       // set as LCRS root
    } else {
        tree.addChild(parentId, nodeId, name, role);            // attach as child under parentId
    }
}

// Populates the full All-India distribution network (Board -> 36 States/UTs -> Districts -> Cities -> Centres).
inline void populateAllIndiaNetwork(AVLRegistry& reg, LCRSTree& tree) {
    // Level 0: National Examination Board (Root)
    addIndiaNode(reg, tree, "", "BOARD", "National Examination Board", "Board", "Registered");

    // Level 1: All 28 Indian States (ST-01 .. ST-28)
    addIndiaNode(reg, tree, "BOARD", "ST-01", "Andhra Pradesh",    "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-02", "Arunachal Pradesh", "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-03", "Assam",             "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-04", "Bihar",             "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-05", "Chhattisgarh",      "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-06", "Goa",               "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-07", "Gujarat",           "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-08", "Haryana",           "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-09", "Himachal Pradesh",  "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-10", "Jharkhand",         "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-11", "Karnataka",         "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-12", "Kerala",            "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-13", "Madhya Pradesh",    "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-14", "Maharashtra",       "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-15", "Manipur",           "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-16", "Meghalaya",         "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-17", "Mizoram",           "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-18", "Nagaland",          "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-19", "Odisha",            "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-20", "Punjab",            "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-21", "Rajasthan",         "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-22", "Sikkim",            "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-23", "Tamil Nadu",        "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-24", "Telangana",         "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-25", "Tripura",           "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-26", "Uttar Pradesh",     "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-27", "Uttarakhand",       "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-28", "West Bengal",       "State", "Registered");

    // Level 1: All 8 Union Territories (ST-29 .. ST-36)
    addIndiaNode(reg, tree, "BOARD", "ST-29", "Andaman and Nicobar Islands",              "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-30", "Chandigarh",                               "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-31", "Dadra and Nagar Haveli and Daman and Diu", "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-32", "Delhi NCR",                                "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-33", "Jammu and Kashmir",                        "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-34", "Ladakh",                                   "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-35", "Lakshadweep",                              "State", "Registered");
    addIndiaNode(reg, tree, "BOARD", "ST-36", "Puducherry",                               "State", "Registered");

    // Level 2-4: Uttarakhand (ST-27) -> Dehradun District -> Dehradun City -> Exam Centres
    addIndiaNode(reg, tree, "ST-27",        "ST-27-D01",    "Dehradun District",             "District", "Registered");
    addIndiaNode(reg, tree, "ST-27-D01",    "ST-27-D01-CT", "Dehradun City",                 "City",     "Registered");
    addIndiaNode(reg, tree, "ST-27-D01-CT", "CTR-UK-01",    "Graphic Era University Centre", "Centre",   "Registered");
    addIndiaNode(reg, tree, "ST-27-D01-CT", "CTR-UK-02",    "Doon Valley Centre 2",          "Centre",   "Registered");

    // Level 2-4: Andhra Pradesh (ST-01) -> Visakhapatnam District -> Visakhapatnam City -> Exam Centres
    addIndiaNode(reg, tree, "ST-01",        "ST-01-D01",    "Visakhapatnam District",        "District", "Registered");
    addIndiaNode(reg, tree, "ST-01-D01",    "ST-01-D01-CT", "Visakhapatnam City",            "City",     "Registered");
    addIndiaNode(reg, tree, "ST-01-D01-CT", "CTR-AP-01",    "GITAM Exam Centre 1",           "Centre",   "Registered");
    addIndiaNode(reg, tree, "ST-01-D01-CT", "CTR-AP-02",    "Unverified AP Centre 2",        "Centre",   "Pending");

    // Level 2-4: Assam (ST-03) -> Kamrup District -> Guwahati City -> Exam Centre
    addIndiaNode(reg, tree, "ST-03",        "ST-03-D01",    "Kamrup District",               "District", "Registered");
    addIndiaNode(reg, tree, "ST-03-D01",    "ST-03-D01-CT", "Guwahati City",                 "City",     "Registered");
    addIndiaNode(reg, tree, "ST-03-D01-CT", "CTR-AS-01",    "AEC Exam Centre 1",             "Centre",   "Registered");

    // Level 2-4: Haryana (ST-08) -> Gurugram District -> Gurugram City -> Exam Centres
    addIndiaNode(reg, tree, "ST-08",        "ST-08-D01",    "Gurugram District",             "District", "Registered");
    addIndiaNode(reg, tree, "ST-08-D01",    "ST-08-D01-CT", "Gurugram City",                 "City",     "Registered");
    addIndiaNode(reg, tree, "ST-08-D01-CT", "CTR-HR-01",    "NHCE Exam Centre 1",            "Centre",   "Registered");
    addIndiaNode(reg, tree, "ST-08-D01-CT", "CTR-HR-99",    "Revoked Rogue Centre",          "Centre",   "Rejected");

    // Level 2-4: Delhi NCR (ST-32) -> New Delhi District -> New Delhi City -> Exam Centre
    addIndiaNode(reg, tree, "ST-32",        "ST-32-D01",    "New Delhi District",            "District", "Registered");
    addIndiaNode(reg, tree, "ST-32-D01",    "ST-32-D01-CT", "New Delhi City",                "City",     "Registered");
    addIndiaNode(reg, tree, "ST-32-D01-CT", "CTR-DL-01",    "DTU Exam Centre 1",             "Centre",   "Registered");

    // Level 2-4: Maharashtra (ST-14) -> Mumbai District -> Mumbai City -> Exam Centre
    addIndiaNode(reg, tree, "ST-14",        "ST-14-D01",    "Mumbai District",               "District", "Registered");
    addIndiaNode(reg, tree, "ST-14-D01",    "ST-14-D01-CT", "Mumbai City",                   "City",     "Registered");
    addIndiaNode(reg, tree, "ST-14-D01-CT", "CTR-MH-01",    "VJTI Exam Centre 1",            "Centre",   "Registered");
}
