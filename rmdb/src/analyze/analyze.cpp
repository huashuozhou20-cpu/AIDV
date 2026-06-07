/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "analyze.h"
#include <functional>
#include <map>
extern std::map<std::string, std::shared_ptr<ast::TreeNode>> view_defs;

/**
 * @description: 分析器，进行语义分析和查询重写，需要检查不符合语义规定的部分
 * @param {shared_ptr<ast::TreeNode>} parse parser生成的结果集
 * @return {shared_ptr<Query>} Query 
 */
std::shared_ptr<Query> Analyze::do_analyze(std::shared_ptr<ast::TreeNode> parse)
{
    std::shared_ptr<Query> query = std::make_shared<Query>();
    if (auto x = std::dynamic_pointer_cast<ast::SelectStmt>(parse))
    {
        // 处理表名和别名
        aliases_.clear();
        // fromList now contains [tab1, alias1?, tab2, alias2?, ...]
        // Extract aliases: entry after a real table name that is NOT a real table is an alias
        for (size_t i = 0; i + 1 < x->tabs.size(); i++) {
            std::string &name = x->tabs[i];
            std::string &next = x->tabs[i+1];
            bool name_is_table = sm_manager_->db_.is_table(name);
            bool next_is_table = sm_manager_->db_.is_table(next);
            if (name_is_table && !next_is_table) {
                aliases_[next] = name;  // alias → real
                i++;  // skip alias
            }
        }
        // Build real table list (exclude aliases)
        for (auto &t : x->tabs) {
            if (aliases_.count(t)) continue;  // skip aliases
            query->tables.push_back(t);
        }
        if (query->tables.empty()) query->tables = x->tabs;  // fallback
        // 检查表是否存在，扩展视图
        std::vector<std::string> expanded_tables;
        std::map<std::string, std::shared_ptr<ast::SelectStmt>> view_col_map;  // table → view stmt
        for (auto &tab_name : query->tables) {
            if (sm_manager_->db_.is_table(tab_name)) {
                expanded_tables.push_back(tab_name);
            } else {
                auto vit = view_defs.find(tab_name);
                if (vit != view_defs.end()) {
                    auto view_stmt = std::dynamic_pointer_cast<ast::SelectStmt>(vit->second);
                    if (view_stmt) {
                        for (auto &t : view_stmt->tabs) {
                            if (!sm_manager_->db_.is_table(t)) continue;
                            expanded_tables.push_back(t);
                            // Remember view column restriction for * expansion
                            if (!view_stmt->cols.empty())
                                view_col_map[t] = view_stmt;
                        }
                        for (auto &j : view_stmt->joins)
                            if (sm_manager_->db_.is_table(j->tab_name))
                                expanded_tables.push_back(j->tab_name);
                        // Merge view conditions (WHERE)
                        if (view_stmt->cond) {
                            if (x->cond) {
                                auto and_expr = std::make_shared<ast::LogicExpr>(ast::LOGIC_AND,
                                    std::vector<std::shared_ptr<ast::CondExpr>>{x->cond, view_stmt->cond});
                                x->cond = and_expr;
                            } else {
                                x->cond = view_stmt->cond;
                            }
                        }
                        continue;
                    }
                }
                sm_manager_->db_.get_table(tab_name);
            }
        }
        query->tables = expanded_tables;

        // 处理 SELECT 投影列
        for (auto &sv_sel_col : x->cols) {
            TabCol sel_col = {.tab_name = sv_sel_col->tab_name, .col_name = sv_sel_col->col_name};
            query->cols.push_back(sel_col);
        }

        // 标记聚合和 DISTINCT
        query->has_agg = x->is_agg;
        query->has_distinct = x->has_distinct;
        query->is_for_update = x->is_for_update;

        std::vector<ColMeta> all_cols;
        get_all_cols(query->tables, all_cols);
        // Add expression columns (CONCAT etc.) to projection
        std::vector<bool> is_concat_col;
        for (auto &e : x->exprs) {
            if (std::dynamic_pointer_cast<ast::ConcatExpr>(e)) {
                TabCol tc;
                tc.col_name = "concat";
                tc.tab_name = "__expr__";  // mark as expression
                query->cols.push_back(tc);
                is_concat_col.push_back(true);
            }
        }
        if (query->cols.empty()) {
            if (!query->has_agg && x->exprs.empty()) {
                // select * : expand to all columns
                // For views with column restrictions, only include view columns
                auto view_stmt = view_col_map.empty() ? nullptr : view_col_map.begin()->second;
                for (auto &col : all_cols) {
                    if (view_stmt && !view_stmt->cols.empty()) {
                        bool in_view = false;
                        for (auto &vc : view_stmt->cols) {
                            if (vc->col_name == col.name && (vc->tab_name.empty() || vc->tab_name == col.tab_name))
                            { in_view = true; break; }
                        }
                        if (!in_view) continue;
                    }
                    TabCol sel_col = {.tab_name = col.tab_name, .col_name = col.name};
                    query->cols.push_back(sel_col);
                }
            }
            // For aggregate/expression-only queries, leave cols empty; the projection
            // will be built from agg/expr output columns by the planner.
        } else {
            // infer table name from column name
            for (auto &sel_col : query->cols) {
                if (sel_col.tab_name != "__expr__")  // skip expression/virtual columns
                    sel_col = check_column(all_cols, sel_col);
            }
        }

        // 处理 WHERE 条件
        get_clause(x->cond, query->conds);
        check_clause(query->tables, query->conds);

        // 构建连接树（JoinTree）
        query->join_tree.clear();
        if (!query->tables.empty()) {
            // 第一个表为驱动表
            query->join_tree.push_back({query->tables[0], INNER_JOIN, {}});

            // 后续表根据 JOIN 语法确定连接类型
            for (size_t i = 1; i < query->tables.size(); i++) {
                JoinItem item;
                item.tab_name = query->tables[i];
                item.join_type = INNER_JOIN;  // 默认：逗号分隔的隐式交叉连接

                // 查找对应的显式 JOIN 表达式
                for (auto &join : x->joins) {
                    if (join->tab_name == query->tables[i]) {
                        item.join_type = join->type;
                        if (join->cond) {
                            get_clause(join->cond, item.conds);
                            check_clause(query->tables, item.conds);
                            // ON条件同时加入conds，供logical_optimization分类使用
                            query->conds.insert(query->conds.end(),
                                              item.conds.begin(), item.conds.end());
                        }
                        break;
                    }
                }
                query->join_tree.push_back(item);
            }
        }

        // 处理 GROUP BY
        if (x->group_by) {
            for (auto &gb_col : x->group_by->cols) {
                TabCol gb = {.tab_name = gb_col->tab_name, .col_name = gb_col->col_name};
                gb = check_column(all_cols, gb);
                // 确保 GROUP BY 列出现在 SELECT 列表中
                bool in_select = false;
                for (auto &sel_col : query->cols) {
                    if (sel_col.tab_name == gb.tab_name && sel_col.col_name == gb.col_name) {
                        in_select = true;
                        break;
                    }
                }
                if (!in_select) {
                    throw InternalError("GROUP BY column '" + gb.col_name + "' must appear in SELECT list");
                }
            }
        }

        // HAVING conditions are processed in the planner (generate_select_plan)
        // 校验 LIMIT 值
        if (x->limit) {
            if (x->limit->limit <= 0) {
                throw InternalError("LIMIT must be greater than 0");
            }
        }
    } else if (auto x = std::dynamic_pointer_cast<ast::UpdateStmt>(parse)) {
        // 检查表是否存在
        sm_manager_->db_.get_table(x->tab_name);
        query->tables.push_back(x->tab_name);

        // 处理 SET 子句
        for (auto &sv_set : x->set_clauses) {
            std::vector<ColMeta> all_cols;
            get_all_cols({x->tab_name}, all_cols);
            TabCol target = {.tab_name = "", .col_name = sv_set->col_name};
            target = check_column(all_cols, target);
            SetClause set_clause;
            set_clause.lhs = target;
            set_clause.rhs = convert_sv_value(sv_set->val);
            query->set_clauses.push_back(set_clause);
        }

        // 处理 WHERE 条件
        get_clause(x->cond, query->conds);
        check_clause({x->tab_name}, query->conds);
    } else if (auto x = std::dynamic_pointer_cast<ast::DeleteStmt>(parse)) {
        // 检查表是否存在
        sm_manager_->db_.get_table(x->tab_name);
        //处理where条件
        get_clause(x->cond, query->conds);
        check_clause({x->tab_name}, query->conds);        
    } else if (auto x = std::dynamic_pointer_cast<ast::InsertStmt>(parse)) {
        // 处理insert 的values值 (multi-row, optional column list)
        for (auto &row : x->vals_list) {
            std::vector<Value> vals;
            for (auto &sv_val : row) {
                vals.push_back(convert_sv_value(sv_val));
            }
            query->values_list.push_back(std::move(vals));
        }
        query->col_names = x->col_names;
    } else {
        // do nothing
    }
    query->parse = std::move(parse);
    return query;
}


TabCol Analyze::check_column(const std::vector<ColMeta> &all_cols, TabCol target) {
    // Resolve table alias
    if (!target.tab_name.empty()) {
        auto it = aliases_.find(target.tab_name);
        if (it != aliases_.end()) {
            target.tab_name = it->second;
        }
    }
    if (target.tab_name.empty()) {
        // Table name not specified, infer table name from column name
        std::string tab_name;
        for (auto &col : all_cols) {
            if (col.name == target.col_name) {
                if (!tab_name.empty()) {
                    throw AmbiguousColumnError(target.col_name);
                }
                tab_name = col.tab_name;
            }
        }
        if (tab_name.empty()) {
            throw ColumnNotFoundError(target.col_name);
        }
        target.tab_name = tab_name;
    } else {
        // 校验指定表中的列是否存在
        TabMeta &tab = sm_manager_->db_.get_table(target.tab_name);
        tab.get_col(target.col_name);  // throws ColumnNotFoundError if not found
    }
    return target;
}

void Analyze::get_all_cols(const std::vector<std::string> &tab_names, std::vector<ColMeta> &all_cols) {
    for (auto &sel_tab_name : tab_names) {
        // 这里db_不能写成get_db(), 注意要传指针
        const auto &sel_tab_cols = sm_manager_->db_.get_table(sel_tab_name).cols;
        all_cols.insert(all_cols.end(), sel_tab_cols.begin(), sel_tab_cols.end());
    }
}

void Analyze::get_clause(const std::shared_ptr<ast::CondExpr> &cond, std::vector<Condition> &conds) {
    conds.clear();
    if (!cond) return;

    std::function<void(const std::shared_ptr<ast::CondExpr>&, std::vector<Condition>&)> traverse;
    traverse = [&](const std::shared_ptr<ast::CondExpr> &node, std::vector<Condition> &out) {
        if (!node) return;

        if (auto logic = std::dynamic_pointer_cast<ast::LogicExpr>(node)) {
            if (logic->op == ast::LOGIC_AND) {
                for (auto &arg : logic->args) {
                    traverse(arg, out);
                }
            } else if (logic->op == ast::LOGIC_OR) {
                Condition c;
                c.op = OP_OR;
                for (auto &arg : logic->args) {
                    std::vector<Condition> child_list;
                    traverse(arg, child_list);
                    if (child_list.size() == 1) {
                        c.children.push_back(std::move(child_list[0]));
                    } else if (!child_list.empty()) {
                        // Wrap multiple ANDed conditions as a group
                        Condition group;
                        group.op = OP_EQ;  // placeholder, not used
                        group.children = std::move(child_list);
                        c.children.push_back(std::move(group));
                    }
                }
                out.push_back(c);
            } else if (logic->op == ast::LOGIC_NOT) {
                Condition c;
                c.op = OP_NOT;
                std::vector<Condition> child_list;
                traverse(logic->args[0], child_list);
                if (child_list.size() == 1) {
                    c.children.push_back(std::move(child_list[0]));
                } else {
                    // Wrap multiple conditions
                    Condition group;
                    group.op = OP_EQ;
                    group.children = std::move(child_list);
                    c.children.push_back(std::move(group));
                }
                out.push_back(c);
            }
        } else if (auto binary = std::dynamic_pointer_cast<ast::BinaryExpr>(node)) {
            Condition c;
            if (auto lhs_col = std::dynamic_pointer_cast<ast::Col>(binary->lhs)) {
                c.lhs_col = {.tab_name = lhs_col->tab_name, .col_name = lhs_col->col_name};
            } else if (auto lhs_agg = std::dynamic_pointer_cast<ast::AggExpr>(binary->lhs)) {
                // HAVING agg > N: use the aggregate's column name
                c.lhs_col = {.tab_name = "", .col_name = lhs_agg->col_name};
            } else if (auto lhs_arith = std::dynamic_pointer_cast<ast::ArithExpr>(binary->lhs)) {
                // Arithmetic expression (e.g., age + 1 > 10):
                // extract the underlying column from the expression tree
                std::function<std::shared_ptr<ast::Col>(const std::shared_ptr<ast::Expr>&)>
                find_col = [&](const std::shared_ptr<ast::Expr>& e) -> std::shared_ptr<ast::Col> {
                    if (!e) return nullptr;
                    if (auto col = std::dynamic_pointer_cast<ast::Col>(e)) return col;
                    if (auto arith = std::dynamic_pointer_cast<ast::ArithExpr>(e)) {
                        auto c = find_col(arith->lhs);
                        if (c) return c;
                        return find_col(arith->rhs);
                    }
                    return nullptr;
                };
                auto lhs_col = find_col(binary->lhs);
                if (lhs_col) {
                    c.lhs_col = {.tab_name = lhs_col->tab_name, .col_name = lhs_col->col_name};
                } else {
                    c.lhs_col = {.tab_name = "", .col_name = ""};
                }
                c.is_arith_expr = true;  // mark for CBO — no index scan
            } else {
                // unknown lhs expression — skip
                c.lhs_col = {.tab_name = "", .col_name = ""};
            }
            c.op = convert_sv_comp_op(binary->op);
            bool has_rhs = false;
            if (auto rhs_val = std::dynamic_pointer_cast<ast::Value>(binary->rhs)) {
                c.is_rhs_val = true;
                c.rhs_val = convert_sv_value(rhs_val);
                has_rhs = true;
            } else if (auto rhs_col = std::dynamic_pointer_cast<ast::Col>(binary->rhs)) {
                c.is_rhs_val = false;
                c.rhs_col = {.tab_name = rhs_col->tab_name, .col_name = rhs_col->col_name};
                has_rhs = true;
            }
            if (has_rhs) out.push_back(c);
        } else if (auto unary = std::dynamic_pointer_cast<ast::UnaryCondExpr>(node)) {
            Condition c;
            c.lhs_col = {.tab_name = unary->col->tab_name, .col_name = unary->col->col_name};
            c.op = (unary->op == ast::SV_OP_IS_NULL) ? OP_IS_NULL : OP_IS_NOT_NULL;
            c.is_rhs_val = false;
            out.push_back(c);
        } else if (auto like = std::dynamic_pointer_cast<ast::LikeExpr>(node)) {
            Condition c;
            c.lhs_col = {.tab_name = like->col->tab_name, .col_name = like->col->col_name};
            c.op = like->not_like ? OP_NOT_LIKE : OP_LIKE;
            c.is_rhs_val = true;
            c.rhs_val.set_str(like->pattern);
            out.push_back(c);
        } else if (auto between = std::dynamic_pointer_cast<ast::BetweenExpr>(node)) {
            Condition c;
            c.lhs_col = {.tab_name = between->col->tab_name, .col_name = between->col->col_name};
            c.op = between->not_between ? OP_NOT_BETWEEN : OP_BETWEEN;
            c.is_rhs_val = true;
            c.rhs_val = convert_sv_value(between->low);
            c.rhs_val2 = convert_sv_value(between->high);
            out.push_back(c);
        } else if (auto in_expr = std::dynamic_pointer_cast<ast::InExpr>(node)) {
            Condition c;
            c.lhs_col = {.tab_name = in_expr->col->tab_name, .col_name = in_expr->col->col_name};
            c.op = in_expr->not_in ? OP_NOT_IN : OP_IN;
            c.is_rhs_val = false;
            for (auto &v : in_expr->values) {
                c.in_values.push_back(convert_sv_value(v));
            }
            out.push_back(c);
        }
    };

    traverse(cond, conds);
}

void Analyze::check_clause(const std::vector<std::string> &tab_names, std::vector<Condition> &conds) {
    std::vector<ColMeta> all_cols;
    get_all_cols(tab_names, all_cols);

    std::function<void(Condition&)> check_one;
    check_one = [&](Condition &cond) {
        if (cond.op == OP_OR || cond.op == OP_NOT) {
            for (auto &child : cond.children) {
                check_one(child);
            }
            return;
        }
        // Infer table name from column name, skip for NULL checks where lhs may be empty
        if (!cond.lhs_col.tab_name.empty() || !cond.lhs_col.col_name.empty()) {
            cond.lhs_col = check_column(all_cols, cond.lhs_col);
        }
        if (!cond.is_rhs_val && cond.op != OP_IS_NULL && cond.op != OP_IS_NOT_NULL
            && cond.op != OP_IN && cond.op != OP_NOT_IN) {
            cond.rhs_col = check_column(all_cols, cond.rhs_col);
        }
        // Skip type checking for expressions where column couldn't be resolved
        // (e.g., complex arithmetic expressions — filtering left to executor)
        if (cond.lhs_col.tab_name.empty() && cond.lhs_col.col_name.empty()) {
            return;
        }
        TabMeta &lhs_tab = sm_manager_->db_.get_table(cond.lhs_col.tab_name);
        auto lhs_col = lhs_tab.get_col(cond.lhs_col.col_name);
        ColType lhs_type = lhs_col->type;

        switch (cond.op) {
            case OP_IS_NULL:
            case OP_IS_NOT_NULL:
                break;  // no type check needed
            case OP_LIKE:
            case OP_NOT_LIKE:
                if (lhs_type != TYPE_STRING) {
                    throw IncompatibleTypeError(coltype2str(lhs_type), "STRING");
                }
                break;
            case OP_BETWEEN:
            case OP_NOT_BETWEEN: {
                if (lhs_type == TYPE_FLOAT && cond.rhs_val.type == TYPE_INT)
                    cond.rhs_val.set_float((float)cond.rhs_val.int_val);
                if (lhs_type == TYPE_FLOAT && cond.rhs_val2.type == TYPE_INT)
                    cond.rhs_val2.set_float((float)cond.rhs_val2.int_val);
                cond.rhs_val.init_raw(lhs_col->len);
                cond.rhs_val2.init_raw(lhs_col->len);
                if (lhs_type != cond.rhs_val.type) {
                    throw IncompatibleTypeError(coltype2str(lhs_type), coltype2str(cond.rhs_val.type));
                }
                break;
            }
            case OP_IN:
            case OP_NOT_IN: {
                for (auto &v : cond.in_values) {
                    if (lhs_type == TYPE_FLOAT && v.type == TYPE_INT)
                        v.set_float((float)v.int_val);
                    v.init_raw(lhs_col->len);
                    if (lhs_type != v.type) {
                        throw IncompatibleTypeError(coltype2str(lhs_type), coltype2str(v.type));
                    }
                }
                break;
            }
            default: {  // regular comparison: EQ, NE, LT, GT, LE, GE
                if (cond.is_rhs_val) {
                    // Implicit INT↔FLOAT type promotion
                    if (lhs_type == TYPE_FLOAT && cond.rhs_val.type == TYPE_INT) {
                        cond.rhs_val.set_float((float)cond.rhs_val.int_val);
                    } else if (lhs_type == TYPE_INT && cond.rhs_val.type == TYPE_FLOAT) {
                        cond.rhs_val.set_int((int)cond.rhs_val.float_val);
                    }
                    cond.rhs_val.init_raw(lhs_col->len);
                    if (lhs_type != cond.rhs_val.type) {
                        throw IncompatibleTypeError(coltype2str(lhs_type), coltype2str(cond.rhs_val.type));
                    }
                } else {
                    TabMeta &rhs_tab = sm_manager_->db_.get_table(cond.rhs_col.tab_name);
                    auto rhs_col = rhs_tab.get_col(cond.rhs_col.col_name);
                    if (lhs_type != rhs_col->type) {
                        throw IncompatibleTypeError(coltype2str(lhs_type), coltype2str(rhs_col->type));
                    }
                }
                break;
            }
        }
    };

    for (auto &cond : conds) {
        check_one(cond);
    }
}


Value Analyze::convert_sv_value(const std::shared_ptr<ast::Value> &sv_val) {
    Value val;
    if (auto int_lit = std::dynamic_pointer_cast<ast::IntLit>(sv_val)) {
        val.set_int(int_lit->val);
    } else if (auto float_lit = std::dynamic_pointer_cast<ast::FloatLit>(sv_val)) {
        val.set_float(float_lit->val);
    } else if (auto str_lit = std::dynamic_pointer_cast<ast::StringLit>(sv_val)) {
        val.set_str(str_lit->val);
    } else if (auto bool_lit = std::dynamic_pointer_cast<ast::BoolLit>(sv_val)) {
        // store bool as int: 1 for true, 0 for false
        val.set_int(bool_lit->val ? 1 : 0);
    } else if (auto null_lit = std::dynamic_pointer_cast<ast::NullLit>(sv_val)) {
        val.set_null();
    } else {
        throw InternalError("Unexpected sv value type");
    }
    return val;
}

CompOp Analyze::convert_sv_comp_op(ast::SvCompOp op) {
    std::map<ast::SvCompOp, CompOp> m = {
        {ast::SV_OP_EQ, OP_EQ}, {ast::SV_OP_NE, OP_NE}, {ast::SV_OP_LT, OP_LT},
        {ast::SV_OP_GT, OP_GT}, {ast::SV_OP_LE, OP_LE}, {ast::SV_OP_GE, OP_GE},
    };
    return m.at(op);
}
