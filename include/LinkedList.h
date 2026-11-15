#ifndef LINKEDLIST_H
#define LINKEDLIST_H

// One node of the list: a paper ID and a pointer to the next node.
struct ListNode
{
    int value;
    ListNode* next;
};

// Singly linked list of paper IDs (no duplicates).
// Used by the Inverted Index (word -> paper IDs) and for search results.
class LinkedList
{
private:
    ListNode* head;
    int count;

public:
    LinkedList();
    ~LinkedList();

    // A list must not be copied: two lists would then delete the same nodes.
    LinkedList(const LinkedList&) = delete;
    LinkedList& operator=(const LinkedList&) = delete;

    void insert(int value);                        // add at the end (skips duplicates)
    bool search(int value) const;                  // is this value in the list?
    void display() const;                          // print all values
    void clear();                                  // delete all nodes
    int size() const;
    ListNode* getHead() const;

    void addAll(const LinkedList& other);          // OR:  add every value of other
    void keepOnlyCommon(const LinkedList& other);  // AND: remove values not in other
};

#endif
