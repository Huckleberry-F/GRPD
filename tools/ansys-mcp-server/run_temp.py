import sys
sys.path.append('D:/Project_C++/GRPD/tools/ansys-mcp-server')
from ansys_runner import AnsysRunner
import yaml
import pprint
import os

with open('D:/Project_C++/GRPD/tools/ansys-mcp-server/config.yaml', 'r', encoding='utf-8') as f:
    config = yaml.safe_load(f)

runner = AnsysRunner(
    executable=config['ansys_executable'],
    num_processors=4,
    memory_mb=2048,
    timeout_seconds=3600
)

mac_file = r'D:\Project_C++\GRPD\Examples\Axisymmetric_Ring\ansys_val.mac'
work_dir = r'D:\ANSYS_Project\GRPD_AXIS'

print('--- 开始运行 ANSYS 计算 ---')
res = runner.run_mac_file(mac_file, work_dir, 'ansys_val')
print(f'成功: {res.success}')
print(f'耗时: {res.elapsed_seconds:.2f} 秒')

print('\n--- 解析出的全部数据字典键 ---')
print(list(res.extracted_data.keys()))

print('\n--- 收敛信息 ---')
if 'convergence' in res.extracted_data:
    pprint.pprint(res.extracted_data['convergence'])

print('\n--- 数值结果预览 (ansys_val_results.txt) ---')
if 'ansys_val_results.txt' in res.extracted_data:
    data = res.extracted_data['ansys_val_results.txt']
    print(f"一共提取到 {data.get('num_rows', 0)} 行数据。")
    print(f"列头: {data.get('headers', '未找到')}")
    print("前5行数据:")
    for row in data.get('preview_rows', [])[:5]:
        print(row)
else:
    print("未找到 ansys_val_results.txt 文件。")
