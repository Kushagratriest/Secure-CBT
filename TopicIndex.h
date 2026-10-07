//It groups question IDs by topic.
//"DSA" -> ["Q1", "Q3", "Q7", "Q12"]
//"OOP" -> ["Q2", "Q5", "Q9"]
//"Maths" -> ["Q4", "Q6", "Q11"]


#pragma once
#include <iostream>
#include <unordered_map>
#include <vector>
#include "Question.h"
using namespace std;

class TopicIndex {
private:
//It's a hash map where:
//key is a string — the topic name like "DSA" or "OOP"
//value is a vector<string> — a list of question IDs under that top
    unordered_map<string, vector<string>> index;

public:
    TopicIndex() {
        cout << "TopicIndex created" << endl;
    }


    //takes a topic and a question ID, pushes that ID into the vector for that topic.
    // So index["DSA"].push_back("Q1") adds Q1 to the DSA list.
    void addToIndex(string topic, string questionId) {
        index[topic].push_back(questionId);
    }


    //you give it a topic, it returns the full list of question IDs under it
    vector<string> getQuestionsByTopic(string topic) {
        return index[topic];
    }
};