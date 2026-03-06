# sensor_anomaly_detection

## Collaboration Workflow

### 1) Clone the repository (one time)

```bash
git clone https://github.com/hemzp/sensor_anomaly_detection.git
cd sensor_anomaly_detection
```

- `git clone ...` downloads the full project and history from GitHub to your computer.
- `cd sensor_anomaly_detection` moves into the project folder so Git commands run in this repo.

### 2) Start work from latest `main`

```bash
git checkout main
git pull origin main
```

- `git checkout main` switches to the main branch.
- `git pull origin main` gets the newest changes from GitHub to avoid starting from old code.

### 3) Create your own feature branch

```bash
git checkout -b feature/<short-name>
```

- Creates and switches to a new branch for your task.
- Keeps `main` clean and makes review easier.

### 4) Save your work in commits

```bash
git add .
git commit -m "Describe what you changed"
```

- `git add .` stages changed files for the next commit.
- `git commit -m ...` saves a snapshot with a message.

### 5) Push your branch to GitHub

```bash
git push -u origin feature/<short-name>
```

- Uploads your branch to GitHub.
- `-u` sets upstream so next time you can use just `git push`.

### 6) Open a Pull Request (PR)

- Open a PR from `feature/<short-name>` into `main`.
- Ask for at least 1 teammate review before merge.
- Merge after approval and passing checks.

### 7) Keep your branch updated before PR merge

```bash
git checkout main
git pull origin main
git checkout feature/<short-name>
git rebase main
```

- Updates your feature branch on top of latest `main`.
- Reduces merge conflicts and keeps history clean.

### Conflict handling

- If Git reports conflicts, open conflicted files and choose/fix final content.
- Then run:

```bash
git add .
git rebase --continue
```

- Repeat until rebase finishes, then push:

```bash
git push --force-with-lease
```

Use `--force-with-lease` only on your own feature branch after rebase, never on `main`.