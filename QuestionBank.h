//it's a hash map wrapper. You give it a Question, it stores it using the question's ID as the key. 
//That's it. So later when PaperGenerator needs to find a question by ID, 
//it's O(1) lookup instead of looping through everything.

#pragma once
#include <iostream>
#include <unordered_map>
#include "Question.h"
using namespace std;

class QuestionBank {
private:
// hash map to store questions by their ID
// key: question ID, value: Question object(object created in Question.h)
//bank is just a variable name for the hash map. 
    unordered_map<string, Question> bank;

public:
    QuestionBank() {
        cout << "QuestionBank created" << endl;
    }


    //takes a Question, uses its ID as the key, and puts it in the hash map.
    void addQuestion(const Question& q) {
        bank.insert_or_assign(q.getId(), q);
    }

    //tells how many question in the bank.
    int size() {
        return bank.size();
    }
};