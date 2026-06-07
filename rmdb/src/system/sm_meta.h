/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#pragma once

#include <algorithm>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "errors.h"
#include "sm_defs.h"

/* 字段元数据 */
struct ColMeta {
    std::string tab_name;   // 字段所属表名称
    std::string name;       // 字段名称
    ColType type;           // 字段类型
    int len;                // 字段长度
    int offset;             // 字段位于记录中的偏移量
    bool index;             /** unused */
    bool not_null = false;  // NOT NULL constraint
    bool primary_key = false;  // PRIMARY KEY
    bool auto_increment = false;  // AUTO_INCREMENT
    bool has_default = false;
    std::string default_val;  // DEFAULT value (single token, no spaces)

    friend std::ostream &operator<<(std::ostream &os, const ColMeta &col) {
        os << col.tab_name << ' ' << col.name << ' ' << col.type << ' ' << col.len << ' ' << col.offset << ' '
           << col.index << ' ' << col.not_null << ' ' << col.primary_key << ' ' << col.auto_increment << ' ' << col.has_default;
        if (col.has_default) {
            os << ' ' << col.default_val;
        }
        return os;
    }

    friend std::istream &operator>>(std::istream &is, ColMeta &col) {
        is >> col.tab_name >> col.name >> col.type >> col.len >> col.offset >> col.index >> col.not_null >> col.primary_key >> col.auto_increment >> col.has_default;
        if (col.has_default) {
            is >> col.default_val;
        }
        return is;
    }
};

/* 索引元数据 */
struct IndexMeta {
    std::string tab_name;           // 索引所属表名称
    int col_tot_len;                // 索引字段长度总和
    int col_num;                    // 索引字段数量
    std::vector<ColMeta> cols;      // 索引包含的字段

    friend std::ostream &operator<<(std::ostream &os, const IndexMeta &index) {
        os << index.tab_name << " " << index.col_tot_len << " " << index.col_num;
        for(auto& col: index.cols) {
            os << "\n" << col;
        }
        return os;
    }

    friend std::istream &operator>>(std::istream &is, IndexMeta &index) {
        is >> index.tab_name >> index.col_tot_len >> index.col_num;
        for(int i = 0; i < index.col_num; ++i) {
            ColMeta col;
            is >> col;
            index.cols.push_back(col);
        }
        return is;
    }
};

/* 表元数据 */
struct TabMeta {
    std::string name;                   // 表名称
    std::vector<ColMeta> cols;          // 表包含的字段
    std::vector<IndexMeta> indexes;     // 表上建立的索引
    int next_auto_inc = 1;              // cached AUTO_INCREMENT value

    TabMeta(){}

    TabMeta(const TabMeta &other) {
        name = other.name;
        next_auto_inc = other.next_auto_inc;
        for(auto col : other.cols) cols.push_back(col);
    }

    /* 判断当前表中是否存在名为col_name的字段 */
    bool is_col(const std::string &col_name) const {
        auto pos = std::find_if(cols.begin(), cols.end(), [&](const ColMeta &col) { return col.name == col_name; });
        return pos != cols.end();
    }

    /* 判断当前表上是否建有指定索引，索引包含的字段为col_names */
    bool is_index(const std::vector<std::string>& col_names) const {
        for(auto& index: indexes) {
            if(static_cast<size_t>(index.col_num) == col_names.size()) {
                size_t i = 0;
                for(; i < static_cast<size_t>(index.col_num); ++i) {
                    if(index.cols[i].name.compare(col_names[i]) != 0)
                        break;
                }
                if(i == static_cast<size_t>(index.col_num)) return true;
            }
        }

        return false;
    }

    /* 根据字段名称集合获取索引元数据 */
    std::vector<IndexMeta>::iterator get_index_meta(const std::vector<std::string>& col_names) {
        for(auto index = indexes.begin(); index != indexes.end(); ++index) {
            if(static_cast<size_t>((*index).col_num) != col_names.size()) continue;
            auto& index_cols = (*index).cols;
            size_t i = 0;
            for(; i < col_names.size(); ++i) {
                if(index_cols[i].name.compare(col_names[i]) != 0) 
                    break;
            }
            if(i == col_names.size()) return index;
        }
        throw IndexNotFoundError(name, col_names);
    }

    /* 根据字段名称获取字段元数据 */
    std::vector<ColMeta>::iterator get_col(const std::string &col_name) {
        auto pos = std::find_if(cols.begin(), cols.end(), [&](const ColMeta &col) { return col.name == col_name; });
        if (pos == cols.end()) {
            throw ColumnNotFoundError(col_name);
        }
        return pos;
    }

    friend std::ostream &operator<<(std::ostream &os, const TabMeta &tab) {
        os << tab.name << '\n' << tab.cols.size() << '\n';
        for (auto &col : tab.cols) {
            os << col << '\n';
        }
        os << tab.indexes.size() << "\n";
        for (auto &index : tab.indexes) {
            os << index << "\n";
        }
        os << tab.next_auto_inc << '\n';
        return os;
    }

    friend std::istream &operator>>(std::istream &is, TabMeta &tab) {
        size_t n;
        is >> tab.name >> n;
        for (size_t i = 0; i < n; i++) {
            ColMeta col;
            is >> col;
            tab.cols.push_back(col);
        }
        is >> n;
        for(size_t i = 0; i < n; ++i) {
            IndexMeta index;
            is >> index;
            tab.indexes.push_back(index);
        }
        if (is >> tab.next_auto_inc) {} else tab.next_auto_inc = 1;
        return is;
    }
};

// 注意重载了操作符 << 和 >>，这需要更底层同样重载TabMeta、ColMeta的操作符 << 和 >>
/* 数据库元数据 */
class DbMeta {
    friend class SmManager;

   private:
    std::string name_;                      // 数据库名称
    std::map<std::string, TabMeta> tabs_;   // 数据库中包含的表
    std::map<std::string, std::string> views_;  // 视图: name -> definition SQL

   public:
    // DbMeta(std::string name) : name_(name) {}

    /* 判断数据库中是否存在指定名称的表 */
    bool is_table(const std::string &tab_name) const { return tabs_.find(tab_name) != tabs_.end(); }
    auto& tables() { return tabs_; }
    const auto& tables() const { return tabs_; }

    bool is_view(const std::string &name) const { return views_.find(name) != views_.end(); }

    std::string get_view(const std::string &name) const {
        auto it = views_.find(name);
        if (it == views_.end()) throw TableNotFoundError(name);
        return it->second;
    }

    void set_view(const std::string &name, const std::string &def) { views_[name] = def; }

    void drop_view(const std::string &name) { views_.erase(name); }

    void SetTabMeta(const std::string &tab_name, const TabMeta &meta) {
        tabs_[tab_name] = meta;
    }

    /* 获取指定名称表的元数据 */
    TabMeta &get_table(const std::string &tab_name) {
        auto pos = tabs_.find(tab_name);
        if (pos == tabs_.end()) {
            throw TableNotFoundError(tab_name);
        }

        return pos->second;
    }

    // 重载操作符 <<
    friend std::ostream &operator<<(std::ostream &os, const DbMeta &db_meta) {
        os << db_meta.name_ << '\n' << db_meta.tabs_.size() << '\n';
        for (auto &entry : db_meta.tabs_) {
            os << entry.second << '\n';
        }
        os << db_meta.views_.size() << '\n';
        for (auto &entry : db_meta.views_) {
            os << entry.first << '\n' << entry.second.size() << '\n' << entry.second;
        }
        return os;
    }

    friend std::istream &operator>>(std::istream &is, DbMeta &db_meta) {
        size_t n;
        is >> db_meta.name_ >> n;
        for (size_t i = 0; i < n; i++) {
            TabMeta tab;
            is >> tab;
            db_meta.tabs_[tab.name] = tab;
        }
        size_t vn;
        if (is >> vn) {
            is.ignore(1);  // skip newline after count
            for (size_t i = 0; i < vn; i++) {
                std::string vname;
                std::getline(is, vname);
                size_t vlen;
                is >> vlen;
                is.ignore(1);  // skip newline
                std::string vdef(vlen, ' ');
                is.read(&vdef[0], vlen);
                db_meta.views_[vname] = vdef;
            }
        }
        return is;
    }
};
