#!/bin/sh
# 安装 git hooks 到 .git/hooks/
set -e

HOOKS_DIR="$(cd "$(dirname "$0")/../githooks" && pwd)"
GIT_HOOKS_DIR="$(cd "$(dirname "$0")/../.git/hooks" && pwd)"

for hook in "$HOOKS_DIR"/*; do
    hook_name=$(basename "$hook")
    cp "$hook" "$GIT_HOOKS_DIR/$hook_name"
    chmod +x "$GIT_HOOKS_DIR/$hook_name"
    echo "Installed: $hook_name"
done

echo "Done! Git hooks are now active."
