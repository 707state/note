#!/bin/bash

# 检查路径是否传入
if [ -z "$1" ]; then
  echo "请提供一个目录路径作为参数"
  exit 1
fi

# 获取传入的目录路径
dir="$1"

# 确保传入的路径是一个有效目录
if [ ! -d "$dir" ]; then
  echo "$dir 不是一个有效的目录"
  exit 1
fi

# 递归查找所有的 .md 文件
find "$dir" -type f -name "*.md" | while read file; do
  echo "正在处理文件: $file"

  # 使用 pandoc 生成目录并更新文件
  pandoc -f gfm --toc -s "$file" -t markdown  -o "$file.tmp"

  # 使用 sed 删除多余的 {#toc-...} 标签
  sed '/{#toc-/s/{[^}]*}//g' "$file.tmp" > "$file"

  # 删除临时文件
  rm "$file.tmp"

  echo "处理完成: $file"
done

echo "所有文件处理完毕"

