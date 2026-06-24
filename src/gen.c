/*
 * gen.c — Static site generator for a developer portfolio.
 *
 * Reads:
 *   data/projects.csv   (link,title,description)
 *   data/articles.csv   (link,title,description)
 *   templates/index.html
 *   templates/project_card.html
 *   templates/article_card.html
 *
 * Writes:
 *   output/index.html
 */

#include "template.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Config ──────────────────────────────────────────────────────────── */

#define MAX_ROWS      256
#define MAX_FIELD_LEN 1024
#define CARD_BUF_SIZE 4096
#define BLOCK_BUF_SIZE (MAX_ROWS * CARD_BUF_SIZE)
#define PAGE_BUF_SIZE  (BLOCK_BUF_SIZE * 2 + 65536)

#define TPL_INDEX        "templates/index.html"
#define TPL_PROJECT_CARD "templates/project_card.html"
#define TPL_ARTICLE_CARD "templates/article_card.html"
#define CSV_PROJECTS     "data/projects.csv"
#define CSV_ARTICLES     "data/articles.csv"
#define OUT_FILE         "output/index.html"

/* ── CSV row ─────────────────────────────────────────────────────────── */

typedef struct {
    char link[MAX_FIELD_LEN];
    char title[MAX_FIELD_LEN];
    char description[MAX_FIELD_LEN];
} Row;

/* ── Minimal CSV parser ──────────────────────────────────────────────── */

/*
 * Parses a single CSV field starting at *pos.
 * Handles quoted fields (RFC 4180-lite): "" inside quotes → single ".
 * On return *pos points past the trailing comma (or to '\0'/'\n').
 * Returns 0 on success, -1 if the output buffer would overflow.
 */
static int parse_field(const char **pos, char *out, size_t outlen) {
    const char *p = *pos;
    size_t w = 0;

#define WRITE(c) do { \
    if (w + 1 >= outlen) return -1; \
    out[w++] = (c); \
} while (0)

    if (*p == '"') {
        p++; /* skip opening quote */
        while (*p && *p != '\n') {
            if (*p == '"') {
                if (*(p + 1) == '"') { WRITE('"'); p += 2; }
                else { p++; break; } /* closing quote */
            } else {
                WRITE(*p++);
            }
        }
    } else {
        while (*p && *p != ',' && *p != '\n' && *p != '\r') {
            WRITE(*p++);
        }
    }

    out[w] = '\0';
    if (*p == ',') p++;
    *pos = p;
    return 0;
#undef WRITE
}

/*
 * Loads a three-column CSV (link,title,description) from path.
 * Skips the header row and blank lines.
 * Returns the number of rows parsed, or -1 on error.
 */
static int load_csv(const char *path, Row *rows, int maxrows) {
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return -1; }

    char line[MAX_FIELD_LEN * 3 + 8];
    int  n = 0;
    int  first = 1;

    while (fgets(line, sizeof(line), f)) {
        /* strip trailing CR/LF */
        size_t l = strlen(line);
        while (l > 0 && (line[l-1] == '\n' || line[l-1] == '\r'))
            line[--l] = '\0';

        if (l == 0)  continue; /* blank line */
        if (first)  { first = 0; continue; } /* header */
        if (n >= maxrows) { fprintf(stderr, "csv: too many rows in %s\n", path); break; }

        const char *p = line;
        if (parse_field(&p, rows[n].link,        sizeof(rows[n].link))        < 0 ||
            parse_field(&p, rows[n].title,       sizeof(rows[n].title))       < 0 ||
            parse_field(&p, rows[n].description, sizeof(rows[n].description)) < 0) {
            fprintf(stderr, "csv: field overflow in %s line %d\n", path, n + 2);
            fclose(f);
            return -1;
        }
        n++;
    }

    fclose(f);
    printf("csv: loaded %d rows from '%s'\n", n, path);
    return n;
}

/* ── Card rendering ──────────────────────────────────────────────────── */

/*
 * Renders all rows into consecutive card HTML and appends them into block.
 * block must have at least BLOCK_BUF_SIZE bytes available.
 * Returns the total bytes written, or -1 on error.
 */
static int render_cards(const Template *card_tpl,
                         const Row *rows, int nrows,
                         char *block, size_t block_size) {
    char card[CARD_BUF_SIZE];
    size_t total = 0;

    for (int i = 0; i < nrows; i++) {
        TplVar vars[] = {
            { "LINK",        rows[i].link        },
            { "TITLE",       rows[i].title       },
            { "DESCRIPTION", rows[i].description },
        };
        int written = tpl_render(card_tpl, card, sizeof(card), vars, 3);
        if (written < 0) {
            fprintf(stderr, "render: card buffer overflow at row %d\n", i);
            return -1;
        }
        if (total + (size_t)written + 1 > block_size) {
            fprintf(stderr, "render: block buffer overflow\n");
            return -1;
        }
        memcpy(block + total, card, (size_t)written);
        total += (size_t)written;
    }
    block[total] = '\0';
    return (int)total;
}

/* ── Entry point ─────────────────────────────────────────────────────── */

int main(void) {
    /* 1. Load CSV data */
    Row projects[MAX_ROWS];
    Row articles[MAX_ROWS];

    int nprojects = load_csv(CSV_PROJECTS, projects, MAX_ROWS);
    int narticles = load_csv(CSV_ARTICLES, articles, MAX_ROWS);
    if (nprojects < 0 || narticles < 0) return 1;

    /* 2. Load templates */
    if (tpl_load_files(TPL_INDEX, TPL_PROJECT_CARD, TPL_ARTICLE_CARD, NULL) != 0)
        return 1;

    const Template *tpl_index   = tpl_get(TPL_INDEX);
    const Template *tpl_project = tpl_get(TPL_PROJECT_CARD);
    const Template *tpl_article = tpl_get(TPL_ARTICLE_CARD);

    if (!tpl_index || !tpl_project || !tpl_article) {
        fprintf(stderr, "gen: failed to retrieve one or more templates\n");
        tpl_unload_all();
        return 1;
    }

    /* 3. Render card blocks */
    char *projects_block = malloc(BLOCK_BUF_SIZE);
    char *articles_block = malloc(BLOCK_BUF_SIZE);
    if (!projects_block || !articles_block) {
        perror("malloc");
        free(projects_block);
        free(articles_block);
        tpl_unload_all();
        return 1;
    }

    if (render_cards(tpl_project, projects, nprojects,
                     projects_block, BLOCK_BUF_SIZE) < 0 ||
        render_cards(tpl_article, articles, narticles,
                     articles_block, BLOCK_BUF_SIZE) < 0) {
        free(projects_block);
        free(articles_block);
        tpl_unload_all();
        return 1;
    }

    /* 4. Render the index page */
    char *page = malloc(PAGE_BUF_SIZE);
    if (!page) {
        perror("malloc");
        free(projects_block);
        free(articles_block);
        tpl_unload_all();
        return 1;
    }

    TplVar page_vars[] = {
        { "PROJECTS_CARDS", projects_block },
        { "ARTICLES_CARDS", articles_block },
    };

    int written = tpl_render(tpl_index, page, PAGE_BUF_SIZE, page_vars, 2);
    if (written < 0) {
        fprintf(stderr, "gen: page buffer overflow — increase PAGE_BUF_SIZE\n");
        free(page);
        free(projects_block);
        free(articles_block);
        tpl_unload_all();
        return 1;
    }

    /* 5. Write output */
    FILE *out = fopen(OUT_FILE, "w");
    if (!out) {
        perror(OUT_FILE);
        free(page);
        free(projects_block);
        free(articles_block);
        tpl_unload_all();
        return 1;
    }
    fwrite(page, 1, (size_t)written, out);
    fclose(out);

    printf("gen: wrote %d bytes to '%s'\n", written, OUT_FILE);

    /* 6. Cleanup */
    free(page);
    free(projects_block);
    free(articles_block);
    tpl_unload_all();
    return 0;
}
