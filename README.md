# Portfolio Generator

A minimal static site generator written in C. Reads two CSV files and a set of
HTML templates, and produces a single `output/index.html` ready for GitHub Pages.

## Project layout

```
.
├── src/
│   ├── gen.c          ← main generator
│   ├── template.c     ← template engine (your code)
│   └── template.h
├── templates/
│   ├── index.html         ← page skeleton; {{PROJECTS_CARDS}} and {{ARTICLES_CARDS}}
│   ├── project_card.html  ← one card; {{LINK}}, {{TITLE}}, {{DESCRIPTION}}
│   └── article_card.html  ← same placeholders
├── data/
│   ├── projects.csv   ← link,title,description (one project per row)
│   └── articles.csv   ← link,title,description (one article per row)
├── output/
│   └── index.html     ← generated; committed by CI or locally
├── Makefile
└── .github/workflows/deploy.yml
```

## Local usage

```sh
# Build the binary and generate output/index.html in one step:
make run

# Or separately:
make build   # produces ./gen
./gen        # reads data/ and templates/, writes output/index.html

# Clean up the binary:
make clean
```

Requires a C11-capable compiler (`cc`/`gcc`/`clang`). No external dependencies.

## Editing your content

### Personalise the page

Open `templates/index.html` and update:

- `<title>` — your name
- `.hero-name` — your name
- `.hero-bio` — the short bio paragraph
- `.hero-links` — your GitHub, email, and LinkedIn URLs

Re-run `make run` to regenerate.

### Add or edit projects

Edit `data/projects.csv`. The format is:

```
link,title,description
https://github.com/you/project,My Project,"A short description of the project."
```

Rules:
- First row is the header and is skipped.
- Fields containing commas or quotes must be enclosed in double quotes.
- A literal `"` inside a quoted field is written as `""`.

### Add or edit articles

Edit `data/articles.csv`. Same format as projects.

## Deploying to GitHub Pages

### One-time setup

1. Push the repository to GitHub.
2. Go to **Settings → Pages**.
3. Under *Source*, select **GitHub Actions**.

That's it. The workflow at `.github/workflows/deploy.yml` will automatically
rebuild and deploy the site whenever you push a change to `data/`, `templates/`,
or `src/` on the `main` branch.

You can also trigger a manual build from the **Actions** tab → **Build & Deploy
Portfolio** → **Run workflow**.

### What the workflow does

1. Checks out the repository.
2. Compiles `gen` with `make build`.
3. Runs `./gen` to produce `output/index.html`.
4. Uploads `output/` as a Pages artifact.
5. Deploys the artifact to `https://<username>.github.io/<repo>/`.

> **Tip:** the `output/` directory is not committed to the repository — GitHub
> Actions rebuilds it fresh on every push. If you want to preview locally just
> open `output/index.html` directly in a browser after `make run`.

## CSV field limits

| Constant        | Value  | What it governs                    |
|-----------------|-------:|------------------------------------|
| `MAX_FIELD_LEN` | 1 024  | Max bytes per CSV field            |
| `MAX_ROWS`      |   256  | Max rows per CSV file              |
| `CARD_BUF_SIZE` | 4 096  | Max bytes per rendered card        |
| `BLOCK_BUF_SIZE`| ~1 MB  | Max total bytes for all cards      |
| `PAGE_BUF_SIZE` | ~2 MB  | Max bytes for the final HTML page  |

Increase these constants in `src/gen.c` if you hit the limits.
