#!/usr/bin/env bash
# GitHub Codespaces setup: installs g++, PostgreSQL and libpq, creates the
# database and table with the sample papers, writes config.txt, and compiles.
# Safe to run again: it does not delete papers if the table already exists.
set -euo pipefail

echo "==> Installing g++, PostgreSQL and libpq"
sudo apt-get update -qq
sudo apt-get install -y -qq g++ libpq-dev postgresql postgresql-client unzip

echo "==> Starting PostgreSQL"
sudo service postgresql start
cd /tmp
sudo runuser -u postgres -- psql -qc "ALTER USER postgres PASSWORD 'postgres';"
if ! sudo runuser -u postgres -- psql -tAc "SELECT 1 FROM pg_database WHERE datname='researchnet'" | grep -q 1; then
  sudo runuser -u postgres -- psql -qc "CREATE DATABASE researchnet;"
fi
cd - > /dev/null

export PGPASSWORD=postgres
HAS_TABLE=$(psql -h localhost -U postgres -d researchnet -tAc \
  "SELECT 1 FROM information_schema.columns WHERE table_name='papers' AND column_name='impact_factor'")
if [ -z "$HAS_TABLE" ]; then
  echo "==> Creating the papers table and inserting sample papers"
  psql -q -h localhost -U postgres -d researchnet -f sql/database.sql
fi

if [ ! -f config.txt ]; then
  echo "host=localhost port=5432 dbname=researchnet user=postgres password=postgres" > config.txt
fi

echo "==> Compiling"
g++ -std=c++17 -I include src/*.cpp -o researchnet -I/usr/include/postgresql -lpq

echo ""
echo "Setup finished."
echo "  Menu:     ./researchnet"
echo "  Website:  ./researchnet --web   then open port 8080"
