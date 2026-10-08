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
    int difficulty;
    int marks;


    Question() {
    questionId = "";
    text = "";
    topic = "";
    difficulty = 0;
    marks = 0;
}
public:
    Question(string id, string text, string topic, int difficulty, int marks) {
        this->questionId = id;
        this->text = text;
        this->topic = topic;
        this->difficulty = difficulty;
        this->marks = marks;
    }

    string getId() const    { return questionId; }
    string getText() const  { return text; }
    string getTopic() const { return topic; }
    int getDifficulty() const { return difficulty; }
    int getMarks() const    { return marks; }
};