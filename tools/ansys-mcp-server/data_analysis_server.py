import os
from mcp.server.fastmcp import FastMCP
from compare_and_export import compare_grpd_and_ansys

# 初始化分离后的数据分析 MCP 服务器
mcp = FastMCP("DataAnalysisServer")

@mcp.tool()
def generate_comparison_report(
    vtk_file: str, 
    ansys_txt_file: str, 
    output_dir: str,
    start_x: float = 0.5, start_y: float = 0.0, start_z: float = 0.0,
    end_x: float = 0.5, end_y: float = 5.0, end_z: float = 0.0
) -> dict:
    """
    读取 GRPD 求解器输出的 VTK 文件和 ANSYS 的文本结果文件，对齐数据并计算相对误差。
    生成 Excel 对比报告以及 PNG 可视化图表。允许动态指定采样直线的空间坐标。
    :param vtk_file: GRPD 输出的二进制 VTK 文件的绝对路径。
    :param ansys_txt_file: ANSYS 导出的 TXT 结果文件的绝对路径。
    :param output_dir: 生成报告的保存目录。
    :param start_x, start_y, start_z: 采样直线起始点坐标 (默认 0.5, 0.0, 0.0)
    :param end_x, end_y, end_z: 采样直线终止点坐标 (默认 0.5, 5.0, 0.0)
    """
    line_start = (start_x, start_y, start_z)
    line_end = (end_x, end_y, end_z)
    return compare_grpd_and_ansys(vtk_file, ansys_txt_file, output_dir, line_start, line_end)

if __name__ == "__main__":
    mcp.run()
