import os

replacements = [
    ('Damage_Obj', 'Fracture_Obj'),
    ('add_subdirectory(Damage)', 'add_subdirectory(Fracture)')
]

# We need to process specific folders
target_dirs = ['PDCommon', 'Src/Engine']
files_to_process = []
for d in target_dirs:
    for root, dirs, files in os.walk(d):
        for f in files:
            if f.endswith('txt'):
                files_to_process.append(os.path.join(root, f))

for fpath in files_to_process:
    try:
        with open(fpath, 'r', encoding='utf-8') as f:
            content = f.read()
        new_content = content
        for old, new in replacements:
            new_content = new_content.replace(old, new)
        if new_content != content:
            with open(fpath, 'w', encoding='utf-8') as f:
                f.write(new_content)
            print(f"Updated {fpath}")
    except Exception as e:
        pass
