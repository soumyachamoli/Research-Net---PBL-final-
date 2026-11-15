#include "Database.h"
#include "TextUtils.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
using namespace std;

Database::Database()
{
    conn = nullptr;
}

Database::~Database()
{
    disconnect();
}

// The config file contains ONE line, in libpq's own format, for example:
//   host=localhost port=5432 dbname=researchnet user=postgres password=my_password
// The file is listed in .gitignore, so the password never goes to GitHub.
bool Database::connect(const char* configFile)
{
    ifstream file(configFile);
    if (!file)
    {
        cout << "Cannot open " << configFile << ". Copy config.example.txt to config.txt first." << endl;
        return false;
    }
    char connectionInfo[500];
    file.getline(connectionInfo, sizeof(connectionInfo));

    conn = PQconnectdb(connectionInfo);
    if (PQstatus(conn) != CONNECTION_OK)
    {
        cout << "Connection to PostgreSQL failed: " << PQerrorMessage(conn) << endl;
        disconnect();
        return false;
    }
    return true;
}

// Copies one row of the query result into a Paper struct.
// libpq gives every value as text, so numbers are converted with atoi / atof.
// Column order: id, title, authors, keywords, abstract_text, publication_year,
//               impact_factor, quartile, journal, url
void Database::fillPaper(PGresult* result, int row, Paper& paper)
{
    paper.id = atoi(PQgetvalue(result, row, 0));
    copyText(paper.title, PQgetvalue(result, row, 1), TITLE_SIZE);
    copyText(paper.authors, PQgetvalue(result, row, 2), AUTHORS_SIZE);
    copyText(paper.keywords, PQgetvalue(result, row, 3), KEYWORDS_SIZE);
    copyText(paper.abstractText, PQgetvalue(result, row, 4), ABSTRACT_SIZE);
    paper.year = atoi(PQgetvalue(result, row, 5));
    paper.impactFactor = (float)atof(PQgetvalue(result, row, 6));
    copyText(paper.quartile, PQgetvalue(result, row, 7), QUARTILE_SIZE);
    copyText(paper.journal, PQgetvalue(result, row, 8), JOURNAL_SIZE);
    copyText(paper.url, PQgetvalue(result, row, 9), URL_SIZE);
}

int Database::getPapers(Paper*& papers)
{
    PGresult* result = PQexec(conn,
        "SELECT id, title, authors, keywords, abstract_text, COALESCE(publication_year, 0), "
        "COALESCE(impact_factor, 0), COALESCE(quartile, ''), COALESCE(journal, ''), COALESCE(url, '') "
        "FROM papers ORDER BY id");

    if (PQresultStatus(result) != PGRES_TUPLES_OK)
    {
        cout << "Query failed: " << PQerrorMessage(conn) << endl;
        PQclear(result);
        return -1;
    }

    int rows = PQntuples(result);
    papers = new Paper[rows];             // one Paper struct per row
    for (int i = 0; i < rows; i++)
    {
        fillPaper(result, i, papers[i]);
    }
    PQclear(result);                      // free the memory libpq used for the result
    return rows;
}

bool Database::searchPaperById(int id, Paper& paper)
{
    char idText[20];
    snprintf(idText, sizeof(idText), "%d", id);
    const char* params[1] = {idText};

    // $1 is a parameter: the id is sent separately from the SQL (safe from SQL injection).
    PGresult* result = PQexecParams(conn,
        "SELECT id, title, authors, keywords, abstract_text, COALESCE(publication_year, 0), "
        "COALESCE(impact_factor, 0), COALESCE(quartile, ''), COALESCE(journal, ''), COALESCE(url, '') "
        "FROM papers WHERE id = $1",
        1, nullptr, params, nullptr, nullptr, 0);

    bool found = false;
    if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1)
    {
        fillPaper(result, 0, paper);
        found = true;
    }
    PQclear(result);
    return found;
}

void Database::disconnect()
{
    if (conn != nullptr)
    {
        PQfinish(conn);
        conn = nullptr;
    }
}
