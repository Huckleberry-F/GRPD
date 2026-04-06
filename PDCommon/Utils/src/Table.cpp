#include "Table.h"
#include <stdexcept>
#include <iterator>

namespace PDCommon::Utils {

Table::Table(const std::string &name) : name_(name) {}

void Table::setType(const std::string &type) {
    type_ = type;
}

const std::string &Table::getType() const {
    return type_;
}

const std::string &Table::getName() const {
    return name_;
}

void Table::addDataPoint(double x, double y) {
    dataPoints_[x] = y;
}

double Table::evaluate(double x) const {
    if (dataPoints_.empty()) {
        return 0.0;
    }
    if (dataPoints_.size() == 1) {
        return dataPoints_.begin()->second;
    }

    auto it = dataPoints_.lower_bound(x);

    // If x is smaller than or equal to the first point, return its value (Clamp)
    if (it == dataPoints_.begin()) {
        return it->second;
    }

    // If x is larger than the last point, return the last value (Clamp)
    if (it == dataPoints_.end()) {
        return std::prev(it)->second;
    }

    // Linear interpolation
    auto prev_it = std::prev(it);
    double x0 = prev_it->first;
    double y0 = prev_it->second;
    double x1 = it->first;
    double y1 = it->second;

    double factor = (x - x0) / (x1 - x0);
    return y0 + factor * (y1 - y0);
}

TableManager &TableManager::getInstance() {
    static TableManager instance;
    return instance;
}

void TableManager::addTable(const Table &table) {
    tables_[table.getName()] = table;
}

const Table *TableManager::getTable(const std::string &name) const {
    auto it = tables_.find(name);
    if (it != tables_.end()) {
        return &(it->second);
    }
    return nullptr;
}

void TableManager::clear() {
    tables_.clear();
}

} // namespace PDCommon::Utils
