import os
from pathlib import Path

root_dir = Path(r"d:\C++pro\GRPD")
output_file = root_dir / "GRPD_AI_Context.md"

# 需要忽略的无关文件夹（不把它们打包给AI）
ignore_dirs = {'.git', 'build', 'out', 'Third', 'assets', '.vscode', '.idea', 'Release', 'Debug'}
# 需要提取代码的后缀类型
valid_extensions = {'.h', '.cpp', '.hpp', '.c', '.txt', '.cmake', '.yaml', '.yml', '.md'}

def generate_tree(dir_path, prefix=""):
    tree_str = ""
    try:
        items = sorted(os.listdir(dir_path))
    except PermissionError:
        return ""
    
    # 过滤掉忽略的目录
    items = [item for item in items if not (os.path.isdir(os.path.join(dir_path, item)) and item in ignore_dirs)]
    
    iters = len(items)
    for i, item in enumerate(items):
        item_path = os.path.join(dir_path, item)
        is_last = (i == iters - 1)
        tree_str += prefix + ("└── " if is_last else "├── ") + item + "\n"
        if os.path.isdir(item_path):
            tree_str += generate_tree(item_path, prefix + ("    " if is_last else "│   "))
    return tree_str

def is_binary_string(line):
    textchars = bytearray({7,8,9,10,12,13,27} | set(range(0x20, 0x100)) - {0x7f})
    return bool(line.translate(None, textchars))

print("开始生成 AI 上下文文档...")

with open(output_file, 'w', encoding='utf-8') as f:
    f.write("# GRPD Project Context for AI Analysis\n\n")
    
    f.write("## 1. Project Overview & Physical Meaning\n")
    f.write("> [👉 强烈建议：请在这里用一两句话描述 GRPD 近场动力学项目的核心诉求与背景]\n\n")
    
    f.write("## 2. Architecture & Goals\n")
    f.write("> [👉 强烈建议：请在这里填入你本次希望大模型帮你重点优化的方向（如内存、多线程并发、某个特定模块）]\n\n")
    
    f.write("## 3. Directory Tree\n")
    f.write("```text\n")
    f.write("GRPD/\n")
    f.write(generate_tree(root_dir))
    f.write("```\n\n")
    
    f.write("## 4. Source Code\n\n")
    
    file_count = 0
    for root, dirs, files in os.walk(root_dir):
        # 移除需要过滤掉的目录名
        dirs[:] = [d for d in dirs if d not in ignore_dirs]
        for file in files:
            path = Path(root) / file
            
            # 不打包自身
            if file == "GRPD_AI_Context.md" or file == "export_ai_context.py":
                continue
                
            if path.suffix in valid_extensions or file == 'CMakeLists.txt':
                try:
                    with open(path, 'r', encoding='utf-8') as src_file:
                        content = src_file.read()
                        
                        f.write(f"==================================================\n")
                        f.write(f"File: {path.relative_to(root_dir).as_posix()}\n")
                        f.write(f"==================================================\n")
                        
                        lang = "cpp"
                        if path.suffix in {'.yaml', '.yml'}: lang = "yaml"
                        elif path.suffix == '.md': lang = "markdown"
                        elif path.suffix == '.cmake' or file == 'CMakeLists.txt': lang = "cmake"
                        
                        f.write(f"```{lang}\n")
                        f.write(content)
                        if not content.endswith('\n'):
                            f.write('\n')
                        f.write("```\n\n")
                        file_count += 1
                except UnicodeDecodeError:
                    pass # 跳过非纯文本或者编码有误的文件
                except Exception as e:
                    f.write(f"// Error reading file {path.name}: {str(e)}\n\n")
                    
print(f"成功导出！\n文件路径: {output_file}\n共合并了 {file_count} 个核心源码文件。")
