import re
with open('d:/Project_C++/GRPD/Examples/Bullet_Contact/PD.yaml', 'r', encoding='utf-8') as f:
    text = f.read()

# Apply user parameter overrides
text = text.replace('Thickness: 0.1', 'Thickness: 0.05')
text = text.replace('dx: 0.1', 'dx: 0.05')

# LinearElasticMat for MatID 1
text = text.replace('Name: "Steel_Plate" # 均质装甲钢（RHA）\n    Type: "JCPlasticityMat"', 'Name: "Steel_Plate" # 均质装甲钢（RHA）\n    Type: "LinearElasticMat"')

# Comment out mechanical properties for MatID 1
text = text.replace('YieldStress: 235.0', '# YieldStress: 235.0')
text = text.replace('HardeningModulus: 5000.0', '# HardeningModulus: 5000.0')
text = text.replace('HardeningType: "Isotropic"', '# HardeningType: "Isotropic"')
text = text.replace('LargeDeformation: false\n    # Johnson-Cook 损伤参数 (钨相对较脆)\n    JC_D1: 0.05    # 较高的基础断裂应变\n    JC_D2: 0.1\n    JC_D3: -1.5', '# LargeDeformation: false\n    # # Johnson-Cook 损伤参数\n    # JC_D1: 0.05 # 较高的基础断裂应变\n    # JC_D2: 0.1\n    # JC_D3: -1.5')

# Change MatID 1 Fracture
text = text.replace('Fracture:\n      Type: "DamageValueFracture"\n      Critical_Damage: 0.99', 'Fracture:\n      Type: "BondStretchFracture"\n      Critical_Stretch: 0.02')

# Fix MatID 2 comment spaces
text = text.replace('JC_D1: 0.1    # 较低的基础断裂应变', 'JC_D1: 0.1 # 较低的基础断裂应变')

# Kinematic Contact params
text = text.replace('RestitutionCoeff: 1.0     # 0=完全塑性碰撞(无反弹), 1=完全弹性碰撞\n    FrictionCoeff: 0.3        # 库仑摩擦系数 (0代表光滑无摩擦)\n    DtEst: 8.7e-9', 'RestitutionCoeff: 0.0 # 0=完全塑性碰撞(无反弹), 1=完全弹性碰撞\n    DtEst: 2.0e-9')

# Solver params
text = text.replace('Time: 8.7e-9\n      TimeStep_dt: 8.7e-9', 'Time: 4.0e-9\n      TimeStep_dt: 2.0e-9')
text = text.replace('Time: 8.7e-5\n      TimeStep_dt: 8.7e-9', 'Time: 4.0e-4\n      TimeStep_dt: 2.0e-9')
text = text.replace('Horizon: 0.3015', 'Horizon: 0.15075')
text = text.replace('ZeroEnergyG0: 1.0', 'ZeroEnergyG0: 0.1')
text = text.replace('ZeroEnergyMethod: "Zhang"', 'ZeroEnergyMethod: "Silling"')

with open('d:/Project_C++/GRPD/Examples/Bullet_Contact/PD.yaml', 'w', encoding='utf-8') as f:
    f.write(text)
