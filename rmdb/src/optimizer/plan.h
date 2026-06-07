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
#include "parser/ast.h"

#include "parser/parser.h"

typedef enum PlanTag{
    T_Invalid = 1,
    T_Help,
    T_ShowTable,
    T_DescTable,
    T_CreateTable,
    T_DropTable,
    T_CreateIndex,
    T_DropIndex,
    T_CreateView,
    T_DropView,
    T_SetKnob,
    T_Insert,
    T_Update,
    T_Delete,
    T_select,
    T_Transaction_begin,
    T_Transaction_commit,
    T_Transaction_abort,
    T_Transaction_rollback,
    T_SeqScan,
    T_IndexScan,
    T_NestLoop,
    T_SortMerge,    // sort merge join
    T_HashJoin,     // hash join
    T_Sort,
    T_Projection,
    T_Aggregation,
    T_Distinct,
    T_Limit
} PlanTag;

// 查询执行计划
class Plan
{
public:
    PlanTag tag;
    virtual ~Plan() = default;
};

class ScanPlan : public Plan
{
    public:
        ScanPlan() = default;

        ScanPlan(PlanTag tag, SmManager *sm_manager, std::string tab_name, std::vector<Condition> conds, std::vector<std::string> index_col_names)
        {
            Plan::tag = tag;
            tab_name_ = std::move(tab_name);
            conds_ = std::move(conds);
            TabMeta &tab = sm_manager->db_.get_table(tab_name_);
            cols_ = tab.cols;
            len_ = cols_.back().offset + cols_.back().len;
            fed_conds_ = conds_;
            index_col_names_ = index_col_names;

        }
        ~ScanPlan(){}

        // Check whether every required output column is covered by the chosen
        // index.  When true the scan can skip table fetches entirely and
        // assemble tuples directly from index-key bytes (index-only scan).
        bool can_do_index_only(SmManager *sm_manager,
                               const std::vector<TabCol> &sel_cols) const {
            if (index_col_names_.empty()) return false;
            if (sel_cols.empty()) return false;
            const auto &tab = sm_manager->db_.get_table(tab_name_);
            // Find the matching index metadata
            const IndexMeta *matched = nullptr;
            for (const auto &idx : tab.indexes) {
                if (static_cast<size_t>(idx.col_num) == index_col_names_.size()) {
                    bool match = true;
                    for (size_t i = 0; i < index_col_names_.size(); i++) {
                        if (idx.cols[i].name != index_col_names_[i]) { match = false; break; }
                    }
                    if (match) { matched = &idx; break; }
                }
            }
            if (!matched) return false;
            for (const auto &sc : sel_cols) {
                bool found = false;
                for (const auto &idx_col : matched->cols) {
                    if (idx_col.name == sc.col_name) { found = true; break; }
                }
                if (!found) return false;
            }
            return true;
        }

        // 以下变量同ScanExecutor中的变量
        std::string tab_name_;
        std::vector<ColMeta> cols_;
        std::vector<Condition> conds_;
        size_t len_;
        std::vector<Condition> fed_conds_;
        std::vector<std::string> index_col_names_;
        bool is_index_only_ = false;

};

class JoinPlan : public Plan
{
    public:
        JoinPlan(PlanTag tag, std::shared_ptr<Plan> left, std::shared_ptr<Plan> right,
                 std::vector<Condition> conds, JoinType join_type = INNER_JOIN)
        {
            Plan::tag = tag;
            left_ = std::move(left);
            right_ = std::move(right);
            conds_ = std::move(conds);
            type = join_type;
        }
        ~JoinPlan(){}
        // 左节点
        std::shared_ptr<Plan> left_;
        // 右节点
        std::shared_ptr<Plan> right_;
        // 连接条件
        std::vector<Condition> conds_;
        JoinType type;
};

class ProjectionPlan : public Plan
{
    public:
        ProjectionPlan(PlanTag tag, std::shared_ptr<Plan> subplan, std::vector<TabCol> sel_cols)
        {
            Plan::tag = tag;
            subplan_ = std::move(subplan);
            sel_cols_ = std::move(sel_cols);
        }
        ~ProjectionPlan(){}
        std::shared_ptr<Plan> subplan_;
        std::vector<TabCol> sel_cols_;
        
};

class SortPlan : public Plan
{
    public:
        SortPlan(PlanTag tag, std::shared_ptr<Plan> subplan, TabCol sel_col, bool is_desc)
        {
            Plan::tag = tag;
            subplan_ = std::move(subplan);
            sel_col_ = sel_col;
            is_desc_ = is_desc;
        }
        ~SortPlan(){}
        std::shared_ptr<Plan> subplan_;
        TabCol sel_col_;
        bool is_desc_;
        
};

// dml语句，包括insert; delete; update; select语句　
class DMLPlan : public Plan
{
    public:
        DMLPlan(PlanTag tag, std::shared_ptr<Plan> subplan,std::string tab_name,
                std::vector<std::vector<Value>> values_list, std::vector<Condition> conds,
                std::vector<SetClause> set_clauses,
                std::vector<std::string> col_names = {})
        {
            Plan::tag = tag;
            subplan_ = std::move(subplan);
            tab_name_ = std::move(tab_name);
            values_list_ = std::move(values_list);
            conds_ = std::move(conds);
            set_clauses_ = std::move(set_clauses);
            col_names_ = std::move(col_names);
        }
        ~DMLPlan(){}
        std::shared_ptr<Plan> subplan_;
        std::string tab_name_;
        std::vector<std::vector<Value>> values_list_;
        std::vector<Condition> conds_;
        std::vector<SetClause> set_clauses_;
        std::vector<std::string> col_names_;
        bool is_for_update_ = false;
};

// ddl语句, 包括create/drop table; create/drop index;
class DDLPlan : public Plan
{
    public:
        DDLPlan(PlanTag tag, std::string tab_name, std::vector<std::string> col_names, std::vector<ColDef> cols)
        {
            Plan::tag = tag;
            tab_name_ = std::move(tab_name);
            cols_ = std::move(cols);
            tab_col_names_ = std::move(col_names);
        }
        ~DDLPlan(){}
        std::string tab_name_;
        std::vector<std::string> tab_col_names_;
        std::vector<ColDef> cols_;
};

// help; show tables; desc tables; begin; abort; commit; rollback语句对应的plan
class OtherPlan : public Plan
{
    public:
        OtherPlan(PlanTag tag, std::string tab_name, std::string def = "")
        {
            Plan::tag = tag;
            tab_name_ = std::move(tab_name);
            def_ = std::move(def);
        }
        ~OtherPlan(){}
        std::string tab_name_;
        std::string def_;
};

// Set Knob Plan
class SetKnobPlan : public Plan
{
    public:
        SetKnobPlan(ast::SetKnobType knob_type, bool bool_value) {
            Plan::tag = T_SetKnob;
            set_knob_type_ = knob_type;
            bool_value_ = bool_value;
        }
    ast::SetKnobType set_knob_type_;
    bool bool_value_;
};

class AggregationPlan : public Plan {
   public:
    AggregationPlan(PlanTag tag, std::shared_ptr<Plan> subplan,
                    std::vector<std::string> group_by_cols,
                    std::vector<ast::AggType> agg_types,
                    std::vector<std::string> agg_cols,
                    std::vector<Condition> having_conds)
        : subplan_(std::move(subplan)), group_by_cols_(std::move(group_by_cols)),
          agg_types_(std::move(agg_types)), agg_cols_(std::move(agg_cols)),
          having_conds_(std::move(having_conds)) {
        Plan::tag = tag;
    }
    ~AggregationPlan() {}
    std::shared_ptr<Plan> subplan_;
    std::vector<std::string> group_by_cols_;
    std::vector<ast::AggType> agg_types_;
    std::vector<std::string> agg_cols_;
    std::vector<Condition> having_conds_;
};

class DistinctPlan : public Plan {
   public:
    DistinctPlan(PlanTag tag, std::shared_ptr<Plan> subplan)
        : subplan_(std::move(subplan)) {
        Plan::tag = tag;
    }
    ~DistinctPlan() {}
    std::shared_ptr<Plan> subplan_;
};

class LimitPlan : public Plan {
   public:
    LimitPlan(PlanTag tag, std::shared_ptr<Plan> subplan, int limit, int offset)
        : subplan_(std::move(subplan)), limit_(limit), offset_(offset) {
        Plan::tag = tag;
    }
    ~LimitPlan() {}
    std::shared_ptr<Plan> subplan_;
    int limit_;
    int offset_;
};

class plannerInfo{
    public:
    std::shared_ptr<ast::SelectStmt> parse;
    std::vector<Condition> where_conds;
    std::vector<TabCol> sel_cols;
    std::shared_ptr<Plan> plan;
    std::vector<std::shared_ptr<Plan>> table_scan_executors;
    std::vector<SetClause> set_clauses;
    plannerInfo(std::shared_ptr<ast::SelectStmt> parse_):parse(std::move(parse_)){}

};
