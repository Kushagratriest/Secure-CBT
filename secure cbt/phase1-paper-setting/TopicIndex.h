//It groups question IDs by topic.
//"DSA" -> ["Q1", "Q3", "Q7", "Q12"]
//"OOP" -> ["Q2", "Q5", "Q9"]
//"Maths" -> ["Q4", "Q6", "Q11"]


#pragma once
#include <iostream>
#include <unordered_map>
#include <vector>
#include <algorithm>     // sort(), used to list subjects/topics/marks in a fixed order
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

    // -----------------------------------------------------------------
    // Subject -> Topic -> Marks -> question IDs
    // Example:
    //   "CS" -> "DSA" -> 2 -> ["Q4", "Q5"]
    //                    4 -> ["Q1", "Q3"]
    //        -> "OOP" -> 4 -> ["Q6", "Q8", "Q9"]
    // This is what the "configure paper" screen needs: for each subject,
    // its topics, and inside each topic how many questions exist per mark value.
    // Same topic name under two subjects stays separate here.
    // -----------------------------------------------------------------
private:
    unordered_map<string, unordered_map<string, unordered_map<int, vector<string>>>> tree;   // 3 nested hash maps

public:
    // Files one question under its subject, topic and marks.
    void addToIndex(string subject, string topic, int marks, string questionId) {
        tree[subject][topic][marks].push_back(questionId);   // [] creates any missing level automatically
    }

    // All IDs for one (subject, topic, marks) group. Empty list if the group doesn't exist.
    vector<string> getQuestions(const string& subject, const string& topic, int marks) const {
        auto s = tree.find(subject);                      // look for the subject
        if (s == tree.end()) return {};                   // no such subject
        auto t = s->second.find(topic);                   // look for the topic inside it
        if (t == s->second.end()) return {};              // no such topic
        auto m = t->second.find(marks);                   // look for the mark value inside it
        if (m == t->second.end()) return {};              // no questions with these marks
        return m->second;                                 // copy of the ID list
    }

    // How many questions are in one (subject, topic, marks) group.
    int countQuestions(const string& subject, const string& topic, int marks) const {
        return getQuestions(subject, topic, marks).size();
    }

    // All subject names, sorted A-Z (hash maps have no fixed order, the screen needs one).
    vector<string> getSubjects() const {
        vector<string> out;
        for (auto& s : tree) out.push_back(s.first);      // collect every subject name
        sort(out.begin(), out.end());                     // alphabetical order
        return out;
    }

    // All topic names inside one subject, sorted A-Z.
    vector<string> getTopics(const string& subject) const {
        vector<string> out;
        auto s = tree.find(subject);
        if (s == tree.end()) return out;                  // unknown subject: empty list
        for (auto& t : s->second) out.push_back(t.first); // collect every topic name
        sort(out.begin(), out.end());
        return out;
    }

    // All mark values that exist inside one topic, sorted low to high (e.g. 2, 3, 5).
    vector<int> getMarkValues(const string& subject, const string& topic) const {
        vector<int> out;
        auto s = tree.find(subject);
        if (s == tree.end()) return out;
        auto t = s->second.find(topic);
        if (t == s->second.end()) return out;
        for (auto& m : t->second) out.push_back(m.first); // collect every mark value
        sort(out.begin(), out.end());
        return out;
    }
};
