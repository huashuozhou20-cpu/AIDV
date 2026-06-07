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

#include <cassert>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "parser/parser.h"
#include "system/sm.h"
#include "common/common.h"

// 连接树中的一项，描述一个表如何加入查询
struct JoinItem {
    std::string tab_name;                // 表名
    JoinType join_type;                  // 连接类型（INNER / LEFT / RIGHT / FULL / CROSS）
    std::vector<Condition> conds;        // ON 连接条件
};

class Query{
    public:
    std::shared_ptr<ast::TreeNode> parse;
    // 连接树：按join顺序排列，第一项为驱动表
    std::vector<JoinItem> join_tree;
    // where条件（已从连接条件中分离）
    std::vector<Condition> conds;
    // 投影列
    std::vector<TabCol> cols;
    // 表名（所有参与查询的表）
    std::vector<std::string> tables;
    // update 的set 值
    std::vector<SetClause> set_clauses;
    //insert 的values值
    std::vector<Value> values;
    // multi-row INSERT values
    std::vector<std::vector<Value>> values_list;
    // INSERT column list
    std::vector<std::string> col_names;
    // select distinct
    bool has_distinct = false;
    // select 中有聚合函数
    bool has_agg = false;
    // SELECT ... FOR UPDATE
    bool is_for_update = false;

    Query(){}

};

class Analyze
{
private:
    SmManager *sm_manager_;
    std::map<std::string, std::string> aliases_;  // alias → real table name
public:
    Analyze(SmManager *sm_manager) : sm_manager_(sm_manager){}
    ~Analyze(){}

    std::shared_ptr<Query> do_analyze(std::shared_ptr<ast::TreeNode> root);

    void get_clause(const std::shared_ptr<ast::CondExpr> &cond, std::vector<Condition> &conds);

private:
    TabCol check_column(const std::vector<ColMeta> &all_cols, TabCol target);
    void get_all_cols(const std::vector<std::string> &tab_names, std::vector<ColMeta> &all_cols);
    void check_clause(const std::vector<std::string> &tab_names, std::vector<Condition> &conds);
    Value convert_sv_value(const std::shared_ptr<ast::Value> &sv_val);
    CompOp convert_sv_comp_op(ast::SvCompOp op);
};

