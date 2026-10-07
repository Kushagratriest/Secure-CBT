#include <iostream>
#include <string>
#include <vector>
#include "Question.h"
#include "QuestionBank.h"
#include "TopicIndex.h"
#include "PaperGenerator.h"
#include "HuffmanCompressor.h"
#include "KeyManager.h"
using namespace std;

int main() {
    cout << "===== PHASE 1: PAPER SETTING =====" << endl;

    // step 1 - create questions
    QuestionBank qBank;
    TopicIndex tIndex;

    vector<Question> questions = {
        Question("Q1",  "What is a linked list?",           "DSA",   2, 4),
        Question("Q2",  "What is a binary tree?",           "DSA",   3, 5),
        Question("Q3",  "What is a hash map?",              "DSA",   2, 4),
        Question("Q4",  "What is a stack?",                 "DSA",   1, 3),
        Question("Q5",  "What is a queue?",                 "DSA",   1, 3),
        Question("Q6",  "What is inheritance?",             "OOP",   2, 4),
        Question("Q7",  "What is polymorphism?",            "OOP",   3, 5),
        Question("Q8",  "What is encapsulation?",           "OOP",   2, 4),
        Question("Q9",  "What is abstraction?",             "OOP",   2, 4),
        Question("Q10", "What is a constructor?",           "OOP",   1, 3),
        Question("Q11", "What is a pointer?",               "CPP",   3, 5),
        Question("Q12", "What is a reference?",             "CPP",   2, 4),
        Question("Q13", "What is a template?",              "CPP",   3, 5),
        Question("Q14", "What is operator overloading?",    "CPP",   3, 5),
        Question("Q15", "What is a destructor?",            "CPP",   2, 4),
        Question("Q16", "What is a deadlock?",              "OS",    3, 5),
        Question("Q17", "What is a process?",               "OS",    1, 3),
        Question("Q18", "What is virtual memory?",          "OS",    3, 5),
        Question("Q19", "What is paging?",                  "OS",    2, 4),
        Question("Q20", "What is semaphore?",               "OS",    3, 5)
    };

    // step 2 - add to bank and index
    for (Question q : questions) {
        qBank.addQuestion(q);
        tIndex.addToIndex(q.getTopic(), q.getId());
    }

    cout << "\nTotal questions in bank: " << qBank.size() << endl;

    // step 3 - generate paper
    PaperGenerator pGen;
    vector<string> topics = {"DSA", "OOP", "CPP", "OS"};
    vector<string> selectedIds = pGen.generate(tIndex, topics, 2);

    cout << "\nSelected questions for paper:" << endl;
    for (string id : selectedIds) {
        cout << "  " << id << endl;
    }

    // step 4 - compress
    string paperData = "";
    for (string id : selectedIds) {
        paperData += id + " ";
    }

    HuffmanCompressor compressor;
    string compressed = compressor.compress(paperData);

    // step 5 - encrypt
    KeyManager keyMgr;
    string rootKey = keyMgr.generateRootKey();
    string encrypted = keyMgr.encrypt(compressed, rootKey);

    cout << "\nPaper encrypted successfully" << endl;
    cout << "===== PHASE 1 COMPLETE =====" << endl;

    return 0;
}