// structure how a question is stored in the system
//and a constructor to initialize the question object with the required parameters


#pragma once
#include <iostream>
#include <string>
using namespace std;

class Question {
private:
    string questionId;
    string text;
    string topic;
    string subject;     // subject the question belongs to, e.g. "CS" (comes from the CSV's subject column)
    int difficulty;
    int marks;


    Question() {
    questionId = "";
    text = "";
    topic = "";
    subject = "";       // start with no subject, like the other text fields
    difficulty = 0;
    marks = 0;
}
public:
    // subject is the LAST parameter and defaults to "" so the existing
    // 5-argument calls in main.cpp keep compiling without any change.
    Question(string id, string text, string topic, int difficulty, int marks, string subject = "") {
        this->questionId = id;
        this->text = text;
        this->topic = topic;
        this->difficulty = difficulty;
        this->marks = marks;
        this->subject = subject;    // store the subject passed in (or "" if none was given)
    }

    string getId() const    { return questionId; }
    string getText() const  { return text; }
    string getTopic() const { return topic; }
    string getSubject() const { return subject; }   // read-only access to the subject
    int getDifficulty() const { return difficulty; }
    int getMarks() const    { return marks; }
};
