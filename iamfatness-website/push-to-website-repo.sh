#!/usr/bin/env bash
#
# Copy this staged site into the standalone IamfatnessWebsite repo and push it.
#
# This staging folder lives on a branch of the CoreVideo repo only as a delivery
# mechanism (the session that built it could not push to IamfatnessWebsite).
# Run this from inside the `iamfatness-website/` directory.
#
# Usage:
#   ./push-to-website-repo.sh [git-remote-url] [branch]
#
# Defaults:
#   remote = git@github.com:iamfatness/IamfatnessWebsite.git
#   branch = main
#
set -euo pipefail

REMOTE="${1:-git@github.com:iamfatness/IamfatnessWebsite.git}"
BRANCH="${2:-main}"

SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK_DIR="$(mktemp -d)"
trap 'rm -rf "$WORK_DIR"' EXIT

echo "Cloning $REMOTE ..."
git clone "$REMOTE" "$WORK_DIR"

echo "Copying staged site into the repo ..."
# Copy everything except this script and any local build artifacts.
rsync -a --delete \
  --exclude '.git/' \
  --exclude 'node_modules/' \
  --exclude '.wrangler/' \
  --exclude 'push-to-website-repo.sh' \
  "$SRC_DIR"/ "$WORK_DIR"/

cd "$WORK_DIR"
git checkout -B "$BRANCH"
git add -A
if git diff --cached --quiet; then
  echo "No changes to commit. The website repo is already up to date."
  exit 0
fi
git commit -m "Add iamfatness.us umbrella landing site"
git push -u origin "$BRANCH"

echo
echo "Done. Pushed the landing site to $REMOTE ($BRANCH)."
echo "Next: set CLOUDFLARE_API_TOKEN and CLOUDFLARE_ACCOUNT_ID repo secrets,"
echo "then run the 'Deploy Site' workflow (or 'npm install && npm run deploy')."
