#!/usr/bin/env bash
set -euo pipefail

echo "==> Installing g++, PostgreSQL, libpq and psycopg"
sudo apt-get update -qq
sudo apt-get install -y -qq g++ libpq-dev postgresql postgresql-contrib postgresql-client python3-pip
pip install --quiet --break-system-packages "psycopg[binary]==3.2.3" 2>/dev/null || pip install --quiet "psycopg[binary]==3.2.3"

echo "==> Starting PostgreSQL"
sudo service postgresql start
cd /tmp
sudo runuser -u postgres -- psql -qc "ALTER USER postgres PASSWORD 'postgres';"
if ! sudo runuser -u postgres -- psql -tAc "SELECT 1 FROM pg_database WHERE datname='researchnet'" | grep -q 1; then
  sudo runuser -u postgres -- psql -qc "CREATE DATABASE researchnet;"
fi
cd - > /dev/null

URL="postgresql://postgres:postgres@localhost:5432/researchnet"
echo "==> Creating tables and importing papers"
python3 tools/load_papers.py "$URL"

if [ ! -f config.txt ]; then
  echo "host=localhost port=5432 dbname=researchnet user=postgres password=postgres" > config.txt
fi

echo "==> Compiling and running the unit tests"
./build.sh test
./build.sh

echo ""
echo "Setup finished. Start the website with:  ./researchnet   (then open port 8080)"
