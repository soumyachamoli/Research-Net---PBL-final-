#include "History.h"
#include <iostream>
using namespace std;

History::History()
{
    head = nullptr;
    count = 0;
}

History::~History()
{
    clearHistory();
}

// Adds a paper at the front (newest first). If it was already in the list,
// the old node is removed first, so each paper appears only once.
void History::addHistory(int paperId)
{
    deleteHistoryItem(paperId);

    HistoryNode* node = new HistoryNode;
    node->paperId = paperId;
    node->next = head;       // the new node points to the old first node
    head = node;             // and becomes the new first node
    count++;

    if (count > MAX_ITEMS)
    {
        removeLast();
    }
}

void History::displayHistory() const
{
    if (head == nullptr)
    {
        cout << "History is empty." << endl;
        return;
    }
    HistoryNode* current = head;
    while (current != nullptr)
    {
        cout << current->paperId;
        if (current->next != nullptr)
        {
            cout << " -> ";
        }
        current = current->next;
    }
    cout << endl;
}

// Removes one paper from the history. Returns false if it was not there.
//
//  Before:  10 -> 7 -> 4      delete 7
//  After:   10 -> 4           (10's next pointer now skips 7, then 7 is deleted)
bool History::deleteHistoryItem(int paperId)
{
    HistoryNode* current = head;
    HistoryNode* previous = nullptr;

    while (current != nullptr)
    {
        if (current->paperId == paperId)
        {
            if (previous == nullptr)
            {
                head = current->next;            // deleting the first node
            }
            else
            {
                previous->next = current->next;  // link around the deleted node
            }
            delete current;                      // free the memory
            count--;
            return true;
        }
        previous = current;
        current = current->next;
    }
    return false;
}

void History::clearHistory()
{
    while (head != nullptr)
    {
        HistoryNode* temp = head;
        head = head->next;
        delete temp;
    }
    count = 0;
}

// Removes the oldest entry (the last node).
void History::removeLast()
{
    if (head == nullptr)
    {
        return;
    }
    if (head->next == nullptr)
    {
        delete head;
        head = nullptr;
        count = 0;
        return;
    }
    HistoryNode* current = head;
    while (current->next->next != nullptr)
    {
        current = current->next;                 // stop at the second-to-last node
    }
    delete current->next;
    current->next = nullptr;
    count--;
}

HistoryNode* History::getHead() const
{
    return head;
}

int History::size() const
{
    return count;
}
