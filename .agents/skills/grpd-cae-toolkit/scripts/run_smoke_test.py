import os
import sys
import subprocess
import glob
import time
import argparse
import importlib.util

def get_latest_vtk(result_dir):
    folders = glob.glob(os.path.join(result_dir, 'Result_*'))
    if not folders:
        raise RuntimeError(f'No result folders found in {result_dir}!')
    latest_folder = max(folders, key=os.path.getctime)
    vtks = glob.glob(os.path.join(latest_folder, '*.vtk'))
    if not vtks:
        raise RuntimeError(f'No VTK files found in the latest result folder: {latest_folder}!')
    return max(vtks, key=os.path.getctime)

def find_grpd_root(start_file):
    path = os.path.abspath(start_file)
    current = os.path.dirname(path)
    while True:
        if os.path.exists(os.path.join(current, 'AGENTS.md')) and os.path.exists(os.path.join(current, 'CMakeLists.txt')):
            return current
        parent = os.path.dirname(current)
        if parent == current:
            raise RuntimeError('Cannot locate GRPD repository root from script path.')
        current = parent

def load_module(module_name, module_path):
    module_dir = os.path.dirname(module_path)
    sys.path.insert(0, module_dir)
    try:
        spec = importlib.util.spec_from_file_location(module_name, module_path)
        if spec is None or spec.loader is None:
            raise ImportError(f'Cannot create import spec for {module_path}')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module
    finally:
        if sys.path and sys.path[0] == module_dir:
            sys.path.pop(0)

def main():
    parser = argparse.ArgumentParser(description="GRPD 自动化冒烟测试脚本")
    parser.add_argument('--build-dir', type=str, required=True, help="CMake build 目录路径")
    parser.add_argument('--test-dir', type=str, required=True, help="GRPD 算例运行目录")
    parser.add_argument('--mac-file', type=str, required=True, help="ANSYS 验证宏文件路径")
    parser.add_argument('--ansys-workdir', type=str, required=True, help="ANSYS 工作目录")
    parser.add_argument('--start-x', type=float, default=1.0, help="提取数据的起始 X 坐标")
    parser.add_argument('--end-x', type=float, default=1.0, help="提取数据的结束 X 坐标")
    
    args = parser.parse_args()
    
    # 动态加载 MCP Server 模块，保持与项目级 .codex/config.toml 的拆分一致。
    grpd_root = find_grpd_root(__file__)
    ansys_mcp = os.path.join(grpd_root, 'tools', 'ansys-mcp-server', 'server.py')
    path_mcp = os.path.join(grpd_root, 'tools', 'path-comparison-mcp-server', 'server.py')

    try:
        ansys_server = load_module('grpd_ansys_mcp_server', ansys_mcp)
        path_comparison_server = load_module('grpd_path_comparison_mcp_server', path_mcp)
    except ImportError as e:
        print(f"Error loading MCP modules: {e}")
        sys.exit(1)

    print('\n--- Step 1: Compiling GRPD ---')
    subprocess.run(['cmake', '--build', '.', '--config', 'Release'], cwd=args.build_dir, check=True)

    print(f'\n--- Step 2: Running GRPD Simulation in {args.test_dir} ---')
    exe_path = os.path.join(grpd_root, 'bin', 'release', 'GRPD.exe')
    if not os.path.exists(exe_path):
        # 兼容不同的输出目录
        exe_path = os.path.join(grpd_root, 'bin', 'PD.exe')
    
    start_time = time.time()
    subprocess.run([exe_path], cwd=args.test_dir, check=True)
    print(f'GRPD Run Time: {time.time() - start_time:.2f}s')

    print('\n--- Step 3: Finding latest VTK ---')
    latest_vtk = get_latest_vtk(args.test_dir)
    print(f'Latest VTK: {latest_vtk}')

    print('\n--- Step 4: Running ANSYS ---')
    res_ansys = ansys_server.run_ansys_mac(
        mac_file=args.mac_file,
        work_dir=args.ansys_workdir,
        job_name='ansys_smoke_test',
        parameters={'START_X': args.start_x, 'END_X': args.end_x}
    )
    print('ANSYS Result Success:', res_ansys.get('success', False))

    print('\n--- Step 5: Fusing Data ---')
    ansys_txt = os.path.join(args.ansys_workdir, 'ansys_val_results.txt')
    output_dir = os.path.join(args.ansys_workdir, 'Smoke_Test_Output')
    
    res_fusion = path_comparison_server.generate_comparison_report(
        vtk_file=latest_vtk,
        ansys_txt_file=ansys_txt,
        output_dir=output_dir,
        start_x=args.start_x, start_y=0.0, start_z=0.0,
        end_x=args.end_x, end_y=5.0, end_z=0.0
    )
    
    print('\n====================================')
    print('🎉 SMOKE TEST COMPLETED 🎉')
    print(f"Max Error (UY)   : {res_fusion.get('max_error_uy_percent', 0):.4f}%")
    print(f"Max Error (SEQV) : {res_fusion.get('max_error_seqv_percent', 0):.4f}%")
    print(f"Comparison Plot  : {res_fusion.get('plot_path')}")
    print('====================================')

if __name__ == "__main__":
    main()
