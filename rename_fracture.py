import os

replacements = [
    ('DamageRegistry', 'FractureRegistry'),
    ('DamageModel', 'FractureModel'),
    ('Damage_Obj', 'Fracture_Obj'),
    ('namespace PDCommon::Damage', 'namespace PDCommon::Fracture'),
    ('PDCommon::Damage', 'PDCommon::Fracture'),
    ('REGISTER_DAMAGE_MODEL', 'REGISTER_FRACTURE_MODEL'),
    ('PDCOMMON_DAMAGE', 'PDCOMMON_FRACTURE'),
    ('Damage', 'Fracture'),
    ('damage', 'fracture'),
    ('computeDamage', 'evaluateFracture'),
    ('calculateDamageRatio', 'calculateBondIntegrity'),
    ('CriticalStretch', 'BondStretchFracture'),
]

files_to_process = []
for root, dirs, files in os.walk('PDCommon/Fracture'):
    for f in files:
        files_to_process.append(os.path.join(root, f))
files_to_process.append('PDCommon/CMakeLists.txt')

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
        print(f"Error reading {fpath}: {e}")
