/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "planner.h"

#include <algorithm>
#include <functional>
#include <memory>

#include "execution/executor_delete.h"
#include "execution/executor_index_scan.h"
#include "execution/executor_insert.h"
#include "execution/executor_nestedloop_join.h"
#include "execution/executor_projection.h"
#include "execution/executor_seq_scan.h"
#include "execution/executor_update.h"
#include "index/ix.h"
#include "record/rm_file_handle.h"
#include "record_printer.h"

// 目前的索引匹配规则为：完全匹配索引字段，且全部为单点查询，不会自动调整where条件的顺序
bool Planner::get_index_cols(std::string tab_name, std::vector<Condition> curr_conds, std::vector<std::string>& index_col_names) {
    index_col_names.clear();
    for(auto& cond: curr_conds) {
        // Skip conditions involving arithmetic expressions — cannot use index
        if (cond.is_arith_expr) continue;
        if(cond.lhs_col.tab_name.compare(tab_name) == 0) {
            // Match OP_EQ (value lookup) or OP_IS_NULL / OP_IS_NOT_NULL
            if ((cond.is_rhs_val && cond.op == OP_EQ) ||
                cond.op == OP_IS_NULL || cond.op == OP_IS_NOT_NULL) {
                index_col_names.push_back(cond.lhs_col.col_name);
            }
        }
    }
    if (index_col_names.empty()) return false;
    TabMeta& tab = sm_manager_->db_.get_table(tab_name);
    if(tab.is_index(index_col_names)) return true;
    index_col_names.clear();
    return false;
}

/**
 * @brief 表算子条件谓词生成
 *
 * @param conds 条件
 * @param tab_names 表名
 * @return std::vector<Condition>
 */
std::vector<Condition> pop_conds(std::vector<Condition> &conds, std::string tab_names) {
    // auto has_tab = [&](const std::string &tab_name) {
    //     return std::find(tab_names.begin(), tab_names.end(), tab_name) != tab_names.end();
    // };
    std::vector<Condition> solved_conds;
    auto it = conds.begin();
    while (it != conds.end()) {
        bool match = false;
        if (it->lhs_col.tab_name == tab_names) {
            // Push if it's a value comparison, unary, or multi-value condition
            if (it->is_rhs_val || it->op == OP_IS_NULL || it->op == OP_IS_NOT_NULL
                || it->op == OP_IN || it->op == OP_NOT_IN
                || it->op == OP_LIKE || it->op == OP_NOT_LIKE
                || it->op == OP_BETWEEN || it->op == OP_NOT_BETWEEN)
                match = true;
        }
        // Also match cross-table conditions (col-to-col with different tables)
        if (!match && it->lhs_col.tab_name == it->rhs_col.tab_name && !it->rhs_col.tab_name.empty())
            match = true;
        if (match) {
            solved_conds.emplace_back(std::move(*it));
            it = conds.erase(it);
        } else {
            it++;
        }
    }
    return solved_conds;
}

int push_conds(Condition *cond, std::shared_ptr<Plan> plan)
{
    if(auto x = std::dynamic_pointer_cast<ScanPlan>(plan))
    {
        if(x->tab_name_.compare(cond->lhs_col.tab_name) == 0) {
            return 1;
        } else if(x->tab_name_.compare(cond->rhs_col.tab_name) == 0){
            return 2;
        } else {
            return 0;
        }
    }
    else if(auto x = std::dynamic_pointer_cast<JoinPlan>(plan))
    {
        int left_res = push_conds(cond, x->left_);
        // 条件已经下推到左子节点
        if(left_res == 3){
            return 3;
        }
        int right_res = push_conds(cond, x->right_);
        // 条件已经下推到右子节点
        if(right_res == 3){
            return 3;
        }
        // 左子节点或右子节点有一个没有匹配到条件的列
        if(left_res == 0 || right_res == 0) {
            return left_res + right_res;
        }
        // 左子节点匹配到条件的右边
        if(left_res == 2) {
            // 需要将左右两边的条件变换位置
            std::map<CompOp, CompOp> swap_op = {
                {OP_EQ, OP_EQ}, {OP_NE, OP_NE}, {OP_LT, OP_GT}, {OP_GT, OP_LT}, {OP_LE, OP_GE}, {OP_GE, OP_LE},
            };
            std::swap(cond->lhs_col, cond->rhs_col);
            cond->op = swap_op.at(cond->op);
        }
        x->conds_.emplace_back(std::move(*cond));
        return 3;
    }
    return false;
}

std::shared_ptr<Plan> pop_scan(int *scantbl, std::string table, std::vector<std::string> &joined_tables, 
                std::vector<std::shared_ptr<Plan>> plans)
{
    for (size_t i = 0; i < plans.size(); i++) {
        auto x = std::dynamic_pointer_cast<ScanPlan>(plans[i]);
        if(x->tab_name_.compare(table) == 0)
        {
            scantbl[i] = 1;
            joined_tables.emplace_back(x->tab_name_);
            return plans[i];
        }
    }
    return nullptr;
}


std::shared_ptr<Query> Planner::logical_optimization(std::shared_ptr<Query> query, Context *context)
{
    // 逻辑优化规则：
    // 1. 将条件分类：单表过滤条件 vs 跨表连接条件
    // 2. 等式条件优先（有利于索引匹配）
    // 3. 过滤条件下推到连接条件之前（便于pop_conds高效提取）

    if (query->conds.empty()) {
        return query;
    }

    std::vector<Condition> filter_conds;
    std::vector<Condition> join_conds;

    for (auto &cond : query->conds) {
        // 单表条件：col op value 或者 同表两列比较
        if (cond.is_rhs_val || cond.lhs_col.tab_name == cond.rhs_col.tab_name) {
            filter_conds.emplace_back(std::move(cond));
        } else {
            join_conds.emplace_back(std::move(cond));
        }
    }

    // 等式条件排在前面，有利于索引匹配（get_index_cols只匹配OP_EQ）
    auto is_eq = [](const Condition &c) { return c.op == OP_EQ; };
    std::stable_sort(filter_conds.begin(), filter_conds.end(),
                     [&](const Condition &a, const Condition &b) {
                         return is_eq(a) && !is_eq(b);
                     });
    std::stable_sort(join_conds.begin(), join_conds.end(),
                     [&](const Condition &a, const Condition &b) {
                         return is_eq(a) && !is_eq(b);
                     });

    // 重建条件列表：先过滤条件，后连接条件
    query->conds = std::move(filter_conds);
    query->conds.insert(query->conds.end(),
                        std::make_move_iterator(join_conds.begin()),
                        std::make_move_iterator(join_conds.end()));

    return query;
}

std::shared_ptr<Plan> Planner::physical_optimization(std::shared_ptr<Query> query, Context *context)
{
    std::shared_ptr<Plan> plan = make_one_rel(query);

    auto x = std::dynamic_pointer_cast<ast::SelectStmt>(query->parse);

    // Aggregate / GROUP BY
    if (query->has_agg) {
        std::vector<ast::AggType> agg_types;
        std::vector<std::string> agg_cols;
        for (auto &agg : x->aggs) {
            agg_types.push_back(agg->agg_type);
            agg_cols.push_back(agg->col_name);
        }
        std::vector<std::string> group_by_cols;
        if (x->group_by) {
            for (auto &gb_col : x->group_by->cols) {
                group_by_cols.push_back(gb_col->col_name);
            }
        }
        std::vector<Condition> having_conds;
        if (x->having) {
            Analyze analyze(sm_manager_);
            analyze.get_clause(x->having, having_conds);
        }
        plan = std::make_shared<AggregationPlan>(T_Aggregation, std::move(plan),
            std::move(group_by_cols), std::move(agg_types), std::move(agg_cols),
            std::move(having_conds));
    }

    // DISTINCT
    if (query->has_distinct) {
        plan = std::make_shared<DistinctPlan>(T_Distinct, std::move(plan));
    }

    // ORDER BY
    plan = generate_sort_plan(query, std::move(plan));

    // LIMIT
    if (x->limit) {
        plan = std::make_shared<LimitPlan>(T_Limit, std::move(plan),
            x->limit->limit, x->limit->offset);
    }

    return plan;
}



int64_t Planner::estimate_row_count(const std::string &tab_name) {
    auto it = sm_manager_->fhs_.find(tab_name);
    if (it != sm_manager_->fhs_.end() && it->second) {
        auto fhdr = it->second->get_file_hdr();
        return static_cast<int64_t>(fhdr.num_pages) * fhdr.num_records_per_page;
    }
    return 0;
}

// Detect if a set of join conditions includes an equi-join (cross-table OP_EQ).
// Returns true and identifies the join column tab names.
static bool is_equi_join(const std::vector<Condition> &conds,
                         std::string &tab_a, std::string &tab_b) {
    for (auto &c : conds) {
        if (!c.is_rhs_val && c.op == OP_EQ &&
            c.lhs_col.tab_name != c.rhs_col.tab_name &&
            !c.lhs_col.tab_name.empty() && !c.rhs_col.tab_name.empty()) {
            tab_a = c.lhs_col.tab_name;
            tab_b = c.rhs_col.tab_name;
            return true;
        }
    }
    return false;
}

std::shared_ptr<Plan> Planner::make_one_rel(std::shared_ptr<Query> query)
{
    auto x = std::dynamic_pointer_cast<ast::SelectStmt>(query->parse);
    std::vector<std::string> tables = query->tables;
    // // Scan table , 生成表算子列表tab_nodes
    std::vector<std::shared_ptr<Plan>> table_scan_executors(tables.size());
    for (size_t i = 0; i < tables.size(); i++) {
        auto curr_conds = pop_conds(query->conds, tables[i]);
        std::vector<std::string> index_col_names;
        bool has_index = get_index_cols(tables[i], curr_conds, index_col_names);
        if (has_index) {
            table_scan_executors[i] =
                std::make_shared<ScanPlan>(T_IndexScan, sm_manager_, tables[i], curr_conds, index_col_names);
        } else {
            table_scan_executors[i] =
                std::make_shared<ScanPlan>(T_SeqScan, sm_manager_, tables[i], curr_conds, index_col_names);
        }
    }
    // 只有一个表，不需要join。
    if(tables.size() == 1)
    {
        return table_scan_executors[0];
    }
    // 获取where条件
    auto conds = std::move(query->conds);
    std::shared_ptr<Plan> table_join_executors;

    // 辅助函数：按表名查找 ScanPlan
    auto get_scan = [&](const std::string &tab_name) -> std::shared_ptr<Plan> {
        for (size_t i = 0; i < tables.size(); i++) {
            auto scan = std::dynamic_pointer_cast<ScanPlan>(table_scan_executors[i]);
            if (scan && scan->tab_name_ == tab_name) {
                return table_scan_executors[i];
            }
        }
        return nullptr;
    };

    // CBO: choose join algorithm and determine build/probe sides
    auto choose_join = [&](std::shared_ptr<Plan> &left, std::shared_ptr<Plan> &right,
                            std::vector<Condition> &join_conds) -> PlanTag {
        // Detect equi-join
        std::string tab_a, tab_b;
        bool is_equi = is_equi_join(join_conds, tab_a, tab_b);

        if (is_equi && enable_hashjoin_join) {
            // CBO: route small table to build (left) side
            auto left_scan = std::dynamic_pointer_cast<ScanPlan>(left);
            auto right_scan = std::dynamic_pointer_cast<ScanPlan>(right);
            if (left_scan && right_scan) {
                int64_t left_rows = estimate_row_count(left_scan->tab_name_);
                int64_t right_rows = estimate_row_count(right_scan->tab_name_);
                if (left_rows > right_rows) {
                    std::swap(left, right);
                }
            }
            return T_HashJoin;
        }

        if (!enable_nestedloop_join && enable_sortmerge_join) return T_SortMerge;
        return T_NestLoop;
    };

    // 根据 join_tree 顺序构建连接计划
    if (query->join_tree.size() > 1) {
        // 驱动表
        table_join_executors = get_scan(query->join_tree[0].tab_name);

        // 按 join_tree 顺序依次连接
        for (size_t i = 1; i < query->join_tree.size(); i++) {
            auto &item = query->join_tree[i];
            auto right = get_scan(item.tab_name);
            if (!right) continue;

            // 收集本步连接条件：ON条件 + WHERE中匹配的跨表条件
            std::vector<Condition> join_conds = item.conds;
            auto it = conds.begin();
            while (it != conds.end()) {
                if (!it->is_rhs_val) {
                    // 跨表条件：检查是否关联已连接的表和新表
                    join_conds.emplace_back(std::move(*it));
                    it = conds.erase(it);
                } else {
                    ++it;
                }
            }

            auto tag = choose_join(table_join_executors, right, join_conds);
            table_join_executors = std::make_shared<JoinPlan>(tag,
                std::move(table_join_executors), std::move(right), join_conds, item.join_type);
        }

        // 处理剩余条件：尝试下推到子计划
        for (auto &cond : conds) {
            if (cond.is_rhs_val) {
                push_conds(&cond, table_join_executors);
            }
        }
    } else {
        // 回退：无 join_tree 时使用原逻辑
        int scantbl[tables.size()];
        for (size_t i = 0; i < tables.size(); i++) scantbl[i] = -1;

        if (conds.size() >= 1) {
            std::vector<std::string> joined_tables;
            auto it = conds.begin();
            while (it != conds.end()) {
                std::shared_ptr<Plan> left, right;
                left = pop_scan(scantbl, it->lhs_col.tab_name, joined_tables, table_scan_executors);
                right = pop_scan(scantbl, it->rhs_col.tab_name, joined_tables, table_scan_executors);
                std::vector<Condition> join_conds{*it};
                auto tag = choose_join(left, right, join_conds);
                table_join_executors = std::make_shared<JoinPlan>(tag,
                    std::move(left), std::move(right), join_conds);
                it = conds.erase(it);
                break;
            }
            it = conds.begin();
            while (it != conds.end()) {
                std::shared_ptr<Plan> left_need = nullptr, right_need = nullptr;
                bool isneedreverse = false;
                if (std::find(joined_tables.begin(), joined_tables.end(), it->lhs_col.tab_name) == joined_tables.end()) {
                    left_need = pop_scan(scantbl, it->lhs_col.tab_name, joined_tables, table_scan_executors);
                }
                if (std::find(joined_tables.begin(), joined_tables.end(), it->rhs_col.tab_name) == joined_tables.end()) {
                    right_need = pop_scan(scantbl, it->rhs_col.tab_name, joined_tables, table_scan_executors);
                    isneedreverse = true;
                }
                if (left_need && right_need) {
                    std::vector<Condition> jc{*it};
                    auto tag = choose_join(left_need, right_need, jc);
                    auto temp = std::make_shared<JoinPlan>(tag, std::move(left_need),
                        std::move(right_need), jc);
                    table_join_executors = std::make_shared<JoinPlan>(T_NestLoop, std::move(temp),
                        std::move(table_join_executors), std::vector<Condition>());
                } else if (left_need || right_need) {
                    if (isneedreverse) {
                        std::map<CompOp, CompOp> swap_op = {
                            {OP_EQ, OP_EQ}, {OP_NE, OP_NE}, {OP_LT, OP_GT}, {OP_GT, OP_LT}, {OP_LE, OP_GE}, {OP_GE, OP_LE},
                        };
                        std::swap(it->lhs_col, it->rhs_col);
                        it->op = swap_op.at(it->op);
                        left_need = std::move(right_need);
                    }
                    std::vector<Condition> jc{*it};
                    table_join_executors = std::make_shared<JoinPlan>(T_NestLoop, std::move(left_need),
                        std::move(table_join_executors), jc);
                } else {
                    push_conds(std::move(&(*it)), table_join_executors);
                }
                it = conds.erase(it);
            }
        } else {
            table_join_executors = table_scan_executors[0];
            scantbl[0] = 1;
        }

        for (size_t i = 0; i < tables.size(); i++) {
            if (scantbl[i] == -1) {
                table_join_executors = std::make_shared<JoinPlan>(T_NestLoop, std::move(table_scan_executors[i]),
                    std::move(table_join_executors), std::vector<Condition>());
            }
        }
    }

    return table_join_executors;

}


std::shared_ptr<Plan> Planner::generate_sort_plan(std::shared_ptr<Query> query, std::shared_ptr<Plan> plan)
{
    auto x = std::dynamic_pointer_cast<ast::SelectStmt>(query->parse);
    if(!x->order) {
        return plan;
    }
    std::vector<std::string> tables = query->tables;
    std::vector<ColMeta> all_cols;
    for (auto &sel_tab_name : tables) {
        // 这里db_不能写成get_db(), 注意要传指针
        const auto &sel_tab_cols = sm_manager_->db_.get_table(sel_tab_name).cols;
        all_cols.insert(all_cols.end(), sel_tab_cols.begin(), sel_tab_cols.end());
    }
    TabCol sel_col;
    for (auto &col : all_cols) {
        if(col.name.compare(x->order->cols->col_name) == 0 )
        sel_col = {.tab_name = col.tab_name, .col_name = col.name};
    }
    return std::make_shared<SortPlan>(T_Sort, std::move(plan), sel_col, 
                                    x->order->orderby_dir == ast::OrderBy_DESC);
}


/**
 * @brief select plan 生成
 *
 * @param sel_cols select plan 选取的列
 * @param tab_names select plan 目标的表
 * @param conds select plan 选取条件
 */
std::shared_ptr<Plan> Planner::generate_select_plan(std::shared_ptr<Query> query, Context *context) {
    //逻辑优化
    query = logical_optimization(std::move(query), context);

    //物理优化
    auto sel_cols = query->cols;
    // Append aggregate output columns so projection includes them
    if (query->has_agg) {
        auto x = std::dynamic_pointer_cast<ast::SelectStmt>(query->parse);
        if (x) {
            for (auto &agg : x->aggs) {
                TabCol col;
                col.col_name = agg->col_name;
                sel_cols.push_back(col);
            }
        }
    }
    std::shared_ptr<Plan> plannerRoot = physical_optimization(query, context);
    plannerRoot = std::make_shared<ProjectionPlan>(T_Projection, std::move(plannerRoot),
                                                        std::move(sel_cols));

    // Enable index-only scan on any underlying ScanPlan whose index covers
    // every column needed by the final projection.
    {
        auto proj = std::dynamic_pointer_cast<ProjectionPlan>(plannerRoot);
        if (proj) {
            std::function<void(std::shared_ptr<Plan>)> enable_index_only;
            enable_index_only = [&](std::shared_ptr<Plan> node) {
                if (auto scan = std::dynamic_pointer_cast<ScanPlan>(node)) {
                    if (scan->can_do_index_only(sm_manager_, proj->sel_cols_)) {
                        scan->is_index_only_ = true;
                    }
                } else if (auto join = std::dynamic_pointer_cast<JoinPlan>(node)) {
                    enable_index_only(join->left_);
                    enable_index_only(join->right_);
                } else if (auto proj2 = std::dynamic_pointer_cast<ProjectionPlan>(node)) {
                    enable_index_only(proj2->subplan_);
                } else if (auto sort = std::dynamic_pointer_cast<SortPlan>(node)) {
                    enable_index_only(sort->subplan_);
                } else if (auto agg = std::dynamic_pointer_cast<AggregationPlan>(node)) {
                    enable_index_only(agg->subplan_);
                } else if (auto dist = std::dynamic_pointer_cast<DistinctPlan>(node)) {
                    enable_index_only(dist->subplan_);
                } else if (auto lim = std::dynamic_pointer_cast<LimitPlan>(node)) {
                    enable_index_only(lim->subplan_);
                }
            };
            enable_index_only(proj->subplan_);
        }
    }

    return plannerRoot;
}

// 生成DDL语句和DML语句的查询执行计划
std::shared_ptr<Plan> Planner::do_planner(std::shared_ptr<Query> query, Context *context)
{
    std::shared_ptr<Plan> plannerRoot;
    if (auto x = std::dynamic_pointer_cast<ast::CreateTable>(query->parse)) {
        // create table;
        std::vector<ColDef> col_defs;
        for (auto &field : x->fields) {
            if (auto sv_col_def = std::dynamic_pointer_cast<ast::ColDef>(field)) {
                ColDef col_def = {.name = sv_col_def->col_name,
                                  .type = interp_sv_type(sv_col_def->type_len->type),
                                  .len = sv_col_def->type_len->len,
                                  .not_null = sv_col_def->not_null_ || sv_col_def->primary_key_,
                                  .primary_key = sv_col_def->primary_key_,
                                  .auto_increment = sv_col_def->auto_increment_};
                if (sv_col_def->default_val_) {
                    col_def.has_default = true;
                    col_def.default_val = ast_value_to_string(sv_col_def->default_val_);
                }
                col_defs.push_back(col_def);
            } else {
                throw InternalError("Unexpected field type");
            }
        }
        plannerRoot = std::make_shared<DDLPlan>(T_CreateTable, x->tab_name, std::vector<std::string>(), col_defs);
    } else if (auto x = std::dynamic_pointer_cast<ast::DropTable>(query->parse)) {
        // drop table;
        plannerRoot = std::make_shared<DDLPlan>(T_DropTable, x->tab_name, std::vector<std::string>(), std::vector<ColDef>());
    } else if (auto x = std::dynamic_pointer_cast<ast::CreateIndex>(query->parse)) {
        // create index;
        plannerRoot = std::make_shared<DDLPlan>(T_CreateIndex, x->tab_name, x->col_names, std::vector<ColDef>());
    } else if (auto x = std::dynamic_pointer_cast<ast::DropIndex>(query->parse)) {
        // drop index
        plannerRoot = std::make_shared<DDLPlan>(T_DropIndex, x->tab_name, x->col_names, std::vector<ColDef>());
    } else if (auto x = std::dynamic_pointer_cast<ast::CreateView>(query->parse)) {
        // create view: store the SELECT SQL
        plannerRoot = std::make_shared<OtherPlan>(T_CreateView, x->view_name);
    } else if (auto x = std::dynamic_pointer_cast<ast::DropView>(query->parse)) {
        plannerRoot = std::make_shared<OtherPlan>(T_DropView, x->view_name);
    } else if (auto x = std::dynamic_pointer_cast<ast::InsertStmt>(query->parse)) {
        // insert;
        plannerRoot = std::make_shared<DMLPlan>(T_Insert, std::shared_ptr<Plan>(),  x->tab_name,
                                                    query->values_list, std::vector<Condition>(), std::vector<SetClause>(),
                                                    query->col_names);
    } else if (auto x = std::dynamic_pointer_cast<ast::DeleteStmt>(query->parse)) {
        // delete: prefer IndexScan when index matches WHERE conditions
        std::vector<std::string> index_col_names;
        bool has_index = get_index_cols(x->tab_name, query->conds, index_col_names);
        auto table_scan_executors = has_index
            ? std::make_shared<ScanPlan>(T_IndexScan, sm_manager_, x->tab_name, query->conds, index_col_names)
            : std::make_shared<ScanPlan>(T_SeqScan, sm_manager_, x->tab_name, query->conds, index_col_names);

        plannerRoot = std::make_shared<DMLPlan>(T_Delete, table_scan_executors, x->tab_name,
                                                std::vector<std::vector<Value>>(), query->conds, std::vector<SetClause>());
    } else if (auto x = std::dynamic_pointer_cast<ast::UpdateStmt>(query->parse)) {
        // update: prefer IndexScan when index matches WHERE conditions
        std::vector<std::string> index_col_names;
        bool has_index = get_index_cols(x->tab_name, query->conds, index_col_names);
        auto table_scan_executors = has_index
            ? std::make_shared<ScanPlan>(T_IndexScan, sm_manager_, x->tab_name, query->conds, index_col_names)
            : std::make_shared<ScanPlan>(T_SeqScan, sm_manager_, x->tab_name, query->conds, index_col_names);

        plannerRoot = std::make_shared<DMLPlan>(T_Update, table_scan_executors, x->tab_name,
                                                     std::vector<std::vector<Value>>(), query->conds,
                                                     query->set_clauses);
    } else if (auto x = std::dynamic_pointer_cast<ast::SelectStmt>(query->parse)) {

        std::shared_ptr<plannerInfo> root = std::make_shared<plannerInfo>(x);
        // Capture FOR UPDATE flag before query is moved
        bool for_update = x->is_for_update;
        // 生成select语句的查询执行计划
        std::shared_ptr<Plan> projection = generate_select_plan(std::move(query), context);
        auto dml_plan = std::make_shared<DMLPlan>(T_select, projection, std::string(), std::vector<std::vector<Value>>(),
                                                    std::vector<Condition>(), std::vector<SetClause>());
        dml_plan->is_for_update_ = for_update;
        plannerRoot = dml_plan;
    } else {
        throw InternalError("Unexpected AST root");
    }
    return plannerRoot;
}