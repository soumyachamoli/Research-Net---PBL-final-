#ifndef DATABASE_H
#define DATABASE_H

#include <libpq-fe.h>
#include "Paper.h"

// Connects to PostgreSQL with libpq and reads papers from the "papers" table.
class Database
{
private:
    PGconn* conn;

    void fillPaper(PGresult* result, int row, Paper& paper);

public:
    Database();
    ~Database();

    bool connect(const char* configFile);           // reads connection settings from a file
    int getPapers(Paper*& papers);                  // all papers; returns how many (-1 on error)
    bool searchPaperById(int id, Paper& paper);     // one paper by its id
    void disconnect();
};

#endif
