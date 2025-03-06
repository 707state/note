import re
import os

org_file = "./NetWork.org"  # 替换为你的 Org 文件名
image_folder = "/home/jask/codes/Stuff/复习资料/old/"         # 图片所在录

# 获取新图片的列表，并按编号排序（确保以 img001.png 格式）
new_images = sorted([f for f in os.listdir(image_folder) if re.match(r"screen-shot-\d{3}\.png", f)])

# 读取 Org 文件
with open(org_file, "r", encoding="utf-8") as f:
    content = f.read()

# 替换图片路径的正则表达式
pattern = r"~/Pictures/Screenshots/Screenshot_\d{4}-\d{2}-\d{2}-\d{2}-\d{2}-\d{2}\_\d{4}x\d{4}.png"

# 替换图片路径
for i, new_image in enumerate(new_images):
    content = re.sub(pattern, f"{image_folder}{new_image}", content, count=1)

# 保存修改后的 Org 文件
with open("updated_document.org", "w", encoding="utf-8") as f:
    f.write(content)

print("图片路径已替换完成，生成 updated_document.org")
