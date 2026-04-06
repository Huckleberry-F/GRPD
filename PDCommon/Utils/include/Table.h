#ifndef PDCOMMON_UTILS_TABLE_H
#define PDCOMMON_UTILS_TABLE_H

// ============================================================================
// Table.h - 数据时空曲线插值表
//
// 责任：
// 1. 存储一维离散数据对 (x, y)，x 可代表 Time 或 X/Y/Z 空间坐标
// 2. 提供基于 x 坐标的线性插值查询能力 evaluate(x)
// ============================================================================

#include <string>
#include <vector>
#include <map>

namespace PDCommon::Utils {

class Table {
public:
  Table() = default;
  explicit Table(const std::string &name);
  ~Table() = default;

  /// @brief 设置数据表类型 ("Time", "Coord_X", "Coord_Y", "Coord_Z")
  void setType(const std::string &type);

  /// @brief 获取数据表类型
  const std::string &getType() const;

  /// @brief 获取数据表名称
  const std::string &getName() const;

  /// @brief 添加数据点
  void addDataPoint(double x, double y);

  /// @brief 基于给定 x 进行线性插值（如果在范围外则外推或保持边界值）
  double evaluate(double x) const;

private:
  std::string name_;
  std::string type_{"Time"};

  // 使用基于 x 排序的自平衡树存储数据点
  std::map<double, double> dataPoints_;
};

class TableManager {
public:
  static TableManager &getInstance();

  TableManager(const TableManager &) = delete;
  TableManager &operator=(const TableManager &) = delete;

  void addTable(const Table &table);
  const Table *getTable(const std::string &name) const;
  void clear();

private:
  TableManager() = default;
  ~TableManager() = default;

  std::map<std::string, Table> tables_;
};

} // namespace PDCommon::Utils

#endif // PDCOMMON_UTILS_TABLE_H
