import os

replacements = [
    ('DamageModel.h', 'FractureModel.h'),
    ('DamageRegistry.h', 'FractureRegistry.h'),
    ('DamageModel', 'FractureModel'),
    ('PDCommon::Damage', 'PDCommon::Fracture'),
    ('damageModel_', 'fractureModel_'),
    ('getDamageModel', 'getFractureModel'),
    ('setDamageModel', 'setFractureModel'),
    ('computeDamage', 'computeFracture'),
    ('InitDamageModels', 'InitFractureModels'),
    ('initDamageModels', 'initFractureModels'),
    ('DamageRegistry', 'FractureRegistry'),
    ('damageEvaluated', 'fractureEvaluated'),
    ('evaluateDamage', 'evaluateFracture'),
    ('PDCommon/Damage', 'PDCommon/Fracture')
]

# We need to process specific folders
target_dirs = ['PDCommon/Material', 'PDCommon/Kernel', 'Src/Engine']
files_to_process = []
for d in target_dirs:
    for root, dirs, files in os.walk(d):
        for f in files:
            if f.endswith('.h') or f.endswith('.cpp') or f.endswith('txt'):
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
