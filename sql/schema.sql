CREATE EXTENSION IF NOT EXISTS pgcrypto;
CREATE EXTENSION IF NOT EXISTS pg_trgm;


CREATE TABLE IF NOT EXISTS journals (
    id                    SERIAL PRIMARY KEY,
    openalex_id           TEXT UNIQUE,
    name                  TEXT NOT NULL,
    issn_l                TEXT,
    source_type           TEXT,
    publisher             TEXT,
    impact_factor         NUMERIC(10, 3),
    impact_factor_source  TEXT,
    impact_factor_year    INT
);

CREATE TABLE IF NOT EXISTS papers (
    id                SERIAL PRIMARY KEY,
    openalex_id       TEXT UNIQUE,
    doi               TEXT UNIQUE,
    title             TEXT NOT NULL,
    abstract          TEXT,
    publication_year  INT,
    publication_date  DATE,
    publication_type  TEXT,
    url               TEXT,
    cited_by_count    INT NOT NULL DEFAULT 0,
    category          TEXT,
    topic             TEXT,
    journal_id        INT REFERENCES journals(id),
    data_source       TEXT NOT NULL,
    retrieved_at      DATE,
    created_at        TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at        TIMESTAMPTZ NOT NULL DEFAULT now(),
    search_vector     TSVECTOR GENERATED ALWAYS AS (
        setweight(to_tsvector('english', coalesce(title, '')), 'A') ||
        setweight(to_tsvector('english', coalesce(abstract, '')), 'B')) STORED
);

CREATE TABLE IF NOT EXISTS authors (
    id           SERIAL PRIMARY KEY,
    openalex_id  TEXT UNIQUE,
    name         TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS paper_authors (
    paper_id   INT NOT NULL REFERENCES papers(id) ON DELETE CASCADE,
    author_id  INT NOT NULL REFERENCES authors(id) ON DELETE CASCADE,
    position   INT NOT NULL,
    PRIMARY KEY (paper_id, author_id)
);

CREATE TABLE IF NOT EXISTS keywords (
    id    SERIAL PRIMARY KEY,
    name  TEXT NOT NULL
);
CREATE UNIQUE INDEX IF NOT EXISTS keywords_name_lower ON keywords (lower(name));

CREATE TABLE IF NOT EXISTS paper_keywords (
    paper_id    INT NOT NULL REFERENCES papers(id) ON DELETE CASCADE,
    keyword_id  INT NOT NULL REFERENCES keywords(id) ON DELETE CASCADE,
    PRIMARY KEY (paper_id, keyword_id)
);


CREATE TABLE IF NOT EXISTS users (
    id             SERIAL PRIMARY KEY,
    full_name      TEXT NOT NULL,
    email          TEXT NOT NULL,
    password_hash  TEXT NOT NULL,
    created_at     TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at     TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE UNIQUE INDEX IF NOT EXISTS users_email_lower ON users (lower(email));

CREATE TABLE IF NOT EXISTS sessions (
    token_hash  BYTEA PRIMARY KEY,
    user_id     INT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
    expires_at  TIMESTAMPTZ NOT NULL
);

CREATE TABLE IF NOT EXISTS bookmarks (
    user_id     INT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    paper_id    INT NOT NULL REFERENCES papers(id) ON DELETE CASCADE,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (user_id, paper_id)
);

CREATE TABLE IF NOT EXISTS search_history (
    id            BIGSERIAL PRIMARY KEY,
    user_id       INT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    query         TEXT NOT NULL,
    filters       JSONB NOT NULL DEFAULT '{}',
    result_count  INT NOT NULL DEFAULT 0,
    created_at    TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS view_history (
    user_id    INT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    paper_id   INT NOT NULL REFERENCES papers(id) ON DELETE CASCADE,
    viewed_at  TIMESTAMPTZ NOT NULL DEFAULT now(),
    PRIMARY KEY (user_id, paper_id)
);

CREATE TABLE IF NOT EXISTS user_interests (
    user_id   INT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    interest  TEXT NOT NULL,
    PRIMARY KEY (user_id, interest)
);


CREATE INDEX IF NOT EXISTS papers_year          ON papers (publication_year);
CREATE INDEX IF NOT EXISTS papers_category      ON papers (category);
CREATE INDEX IF NOT EXISTS papers_type          ON papers (publication_type);
CREATE INDEX IF NOT EXISTS papers_journal       ON papers (journal_id);
CREATE INDEX IF NOT EXISTS papers_cited         ON papers (cited_by_count DESC);
CREATE INDEX IF NOT EXISTS papers_search_vector ON papers USING GIN (search_vector);
CREATE INDEX IF NOT EXISTS journals_impact      ON journals (impact_factor);
CREATE INDEX IF NOT EXISTS journals_name_trgm   ON journals USING GIN (name gin_trgm_ops);
CREATE INDEX IF NOT EXISTS authors_name_trgm    ON authors USING GIN (name gin_trgm_ops);
CREATE INDEX IF NOT EXISTS paper_authors_author ON paper_authors (author_id);
CREATE INDEX IF NOT EXISTS paper_keywords_kw    ON paper_keywords (keyword_id);
CREATE INDEX IF NOT EXISTS sessions_user        ON sessions (user_id);
CREATE INDEX IF NOT EXISTS bookmarks_user_time  ON bookmarks (user_id, created_at DESC);
CREATE INDEX IF NOT EXISTS search_history_user  ON search_history (user_id, created_at DESC);
CREATE INDEX IF NOT EXISTS view_history_user    ON view_history (user_id, viewed_at DESC);
