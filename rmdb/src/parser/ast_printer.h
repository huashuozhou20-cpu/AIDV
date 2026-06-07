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

#include "ast.h"
#include <cassert>
#include <iostream>
#include <map>

namespace ast {

class TreePrinter {
public:
    static void print(const std::shared_ptr<TreeNode> &node) {
        print_node(node, 0);
    }

private:
    static std::string offset2string(int offset) {
        return std::string(offset, ' ');
    }

    template<typename T>
    static void print_val(const T &val, int offset) {
        std::cout << offset2string(offset) << val << '\n';
    }

    template<typename T>
    static void print_val_list(const std::vector<T> &vals, int offset) {
        std::cout << offset2string(offset) << "LIST\n";
        offset += 2;
        for (auto &val : vals) {
            print_val(val, offset);
        }
    }

    static std::string type2str(SvType type) {
        static std::map<SvType, std::string> m{
                {SV_TYPE_INT,    "INT"},
                {SV_TYPE_FLOAT,  "FLOAT"},
                {SV_TYPE_STRING, "STRING"},
        };
        return m.at(type);
    }

    static std::string op2str(SvCompOp op) {
        static std::map<SvCompOp, std::string> m{
                {SV_OP_EQ,        "=="},
                {SV_OP_NE,        "!="},
                {SV_OP_LT,        "<"},
                {SV_OP_GT,        ">"},
                {SV_OP_LE,        "<="},
                {SV_OP_GE,        ">="},
                {SV_OP_LIKE,      "LIKE"},
                {SV_OP_NOT_LIKE,  "NOT LIKE"},
                {SV_OP_BETWEEN,   "BETWEEN"},
                {SV_OP_NOT_BETWEEN, "NOT BETWEEN"},
                {SV_OP_IN,        "IN"},
                {SV_OP_NOT_IN,    "NOT IN"},
                {SV_OP_IS_NULL,   "IS NULL"},
                {SV_OP_IS_NOT_NULL, "IS NOT NULL"},
        };
        return m.at(op);
    }

    static std::string agg2str(AggType type) {
        static std::map<AggType, std::string> m{
                {AGG_COUNT, "COUNT"},
                {AGG_SUM,   "SUM"},
                {AGG_AVG,   "AVG"},
                {AGG_MAX,   "MAX"},
                {AGG_MIN,   "MIN"},
        };
        return m.at(type);
    }

    static std::string arith2str(ArithOp op) {
        static std::map<ArithOp, std::string> m{
                {ARITH_ADD, "+"},
                {ARITH_SUB, "-"},
                {ARITH_MUL, "*"},
                {ARITH_DIV, "/"},
                {ARITH_MOD, "%"},
                {ARITH_NEG, "-"},
        };
        return m.at(op);
    }

    static std::string logic2str(LogicOp op) {
        static std::map<LogicOp, std::string> m{
                {LOGIC_AND, "AND"},
                {LOGIC_OR,  "OR"},
                {LOGIC_NOT, "NOT"},
        };
        return m.at(op);
    }

    static std::string jointype2str(JoinType type) {
        static std::map<JoinType, std::string> m{
                {INNER_JOIN, "INNER"},
                {LEFT_JOIN,  "LEFT"},
                {RIGHT_JOIN, "RIGHT"},
                {FULL_JOIN,  "FULL"},
        };
        return m.at(type);
    }

    template<typename T>
    static void print_node_list(const std::vector<std::shared_ptr<T>> &nodes, int offset) {
        std::cout << offset2string(offset) << "LIST\n";
        offset += 2;
        for (auto &node : nodes) {
            print_node(node, offset);
        }
    }

    static void print_node(const std::shared_ptr<TreeNode> &node, int offset) {
        if (!node) return;
        std::cout << offset2string(offset);
        offset += 2;

        // Simple commands
        if (auto x = std::dynamic_pointer_cast<Help>(node)) {
            std::cout << "HELP\n";
        } else if (auto x = std::dynamic_pointer_cast<ShowTables>(node)) {
            std::cout << "SHOW_TABLES\n";
        } else if (auto x = std::dynamic_pointer_cast<TxnBegin>(node)) {
            std::cout << "BEGIN\n";
        } else if (auto x = std::dynamic_pointer_cast<TxnCommit>(node)) {
            std::cout << "COMMIT\n";
        } else if (auto x = std::dynamic_pointer_cast<TxnAbort>(node)) {
            std::cout << "ABORT\n";
        } else if (auto x = std::dynamic_pointer_cast<TxnRollback>(node)) {
            std::cout << "ROLLBACK\n";
        }

        // DDL
        else if (auto x = std::dynamic_pointer_cast<CreateTable>(node)) {
            std::cout << "CREATE_TABLE\n";
            print_val(x->tab_name, offset);
            print_node_list(x->fields, offset);
        } else if (auto x = std::dynamic_pointer_cast<DropTable>(node)) {
            std::cout << "DROP_TABLE\n";
            print_val(x->tab_name, offset);
        } else if (auto x = std::dynamic_pointer_cast<DescTable>(node)) {
            std::cout << "DESC_TABLE\n";
            print_val(x->tab_name, offset);
        } else if (auto x = std::dynamic_pointer_cast<CreateIndex>(node)) {
            std::cout << "CREATE_INDEX\n";
            print_val(x->tab_name, offset);
            for (auto &col_name : x->col_names)
                print_val(col_name, offset);
        } else if (auto x = std::dynamic_pointer_cast<DropIndex>(node)) {
            std::cout << "DROP_INDEX\n";
            print_val(x->tab_name, offset);
            for (auto &col_name : x->col_names)
                print_val(col_name, offset);
        }

        // Type system
        else if (auto x = std::dynamic_pointer_cast<ColDef>(node)) {
            std::cout << "COL_DEF\n";
            print_val(x->col_name, offset);
            print_node(x->type_len, offset);
        } else if (auto x = std::dynamic_pointer_cast<TypeLen>(node)) {
            std::cout << "TYPE_LEN\n";
            print_val(type2str(x->type), offset);
            print_val(x->len, offset);
        }

        // Column reference
        else if (auto x = std::dynamic_pointer_cast<Col>(node)) {
            std::cout << "COL\n";
            print_val(x->tab_name, offset);
            print_val(x->col_name, offset);
        }

        // Value literals
        else if (auto x = std::dynamic_pointer_cast<IntLit>(node)) {
            std::cout << "INT_LIT\n";
            print_val(x->val, offset);
        } else if (auto x = std::dynamic_pointer_cast<FloatLit>(node)) {
            std::cout << "FLOAT_LIT\n";
            print_val(x->val, offset);
        } else if (auto x = std::dynamic_pointer_cast<StringLit>(node)) {
            std::cout << "STRING_LIT\n";
            print_val(x->val, offset);
        } else if (auto x = std::dynamic_pointer_cast<BoolLit>(node)) {
            std::cout << "BOOL_LIT\n";
            print_val(x->val ? "TRUE" : "FALSE", offset);
        } else if (auto x = std::dynamic_pointer_cast<NullLit>(node)) {
            std::cout << "NULL_LIT\n";
        }

        // Arithmetic expression
        else if (auto x = std::dynamic_pointer_cast<ArithExpr>(node)) {
            std::cout << "ARITH_EXPR\n";
            print_val(arith2str(x->op), offset);
            if (x->lhs) print_node(x->lhs, offset);
            print_node(x->rhs, offset);
        }

        // Aggregate expression
        else if (auto x = std::dynamic_pointer_cast<AggExpr>(node)) {
            std::cout << "AGG_EXPR\n";
            print_val(agg2str(x->agg_type), offset);
            if (x->is_star) {
                print_val("*", offset);
            } else {
                print_val(x->col_name, offset);
            }
        }

        // Subquery expression
        else if (auto x = std::dynamic_pointer_cast<SubqueryExpr>(node)) {
            std::cout << "SUBQUERY_EXPR\n";
            print_node(x->subquery, offset);
        }

        // Condition expressions
        else if (auto x = std::dynamic_pointer_cast<BinaryExpr>(node)) {
            std::cout << "BINARY_EXPR\n";
            print_node(x->lhs, offset);
            print_val(op2str(x->op), offset);
            print_node(x->rhs, offset);
        } else if (auto x = std::dynamic_pointer_cast<LogicExpr>(node)) {
            std::cout << "LOGIC_EXPR\n";
            print_val(logic2str(x->op), offset);
            print_node_list(x->args, offset);
        } else if (auto x = std::dynamic_pointer_cast<UnaryCondExpr>(node)) {
            std::cout << "UNARY_COND\n";
            print_val(op2str(x->op), offset);
            print_node(x->col, offset);
        } else if (auto x = std::dynamic_pointer_cast<LikeExpr>(node)) {
            std::cout << "LIKE_EXPR\n";
            print_val(x->not_like ? "NOT LIKE" : "LIKE", offset);
            print_node(x->col, offset);
            print_val(x->pattern, offset);
        } else if (auto x = std::dynamic_pointer_cast<BetweenExpr>(node)) {
            std::cout << "BETWEEN_EXPR\n";
            print_val(x->not_between ? "NOT BETWEEN" : "BETWEEN", offset);
            print_node(x->col, offset);
            print_node(x->low, offset);
            print_node(x->high, offset);
        } else if (auto x = std::dynamic_pointer_cast<InExpr>(node)) {
            std::cout << "IN_EXPR\n";
            print_val(x->not_in ? "NOT IN" : "IN", offset);
            print_node(x->col, offset);
            print_node_list(x->values, offset);
        }

        // DML statements
        else if (auto x = std::dynamic_pointer_cast<InsertStmt>(node)) {
            std::cout << "INSERT\n";
            print_val(x->tab_name, offset);
            if (!x->col_names.empty()) {
                for (auto &c : x->col_names) print_val(c, offset);
            }
            for (auto &row : x->vals_list) {
                print_node_list(row, offset);
            }
        } else if (auto x = std::dynamic_pointer_cast<DeleteStmt>(node)) {
            std::cout << "DELETE\n";
            print_val(x->tab_name, offset);
            if (x->cond) print_node(x->cond, offset);
        } else if (auto x = std::dynamic_pointer_cast<UpdateStmt>(node)) {
            std::cout << "UPDATE\n";
            print_val(x->tab_name, offset);
            print_node_list(x->set_clauses, offset);
            if (x->cond) print_node(x->cond, offset);
        } else if (auto x = std::dynamic_pointer_cast<SelectStmt>(node)) {
            std::cout << "SELECT\n";
            if (x->has_distinct) print_val("DISTINCT", offset);
            print_node_list(x->cols, offset);
            if (!x->aggs.empty()) print_node_list(x->aggs, offset);
            if (!x->exprs.empty()) print_node_list(x->exprs, offset);
            print_val_list(x->tabs, offset);
            if (!x->joins.empty()) print_node_list(x->joins, offset);
            if (x->cond) print_node(x->cond, offset);
            if (x->group_by) print_node(x->group_by, offset);
            if (x->having) print_node(x->having, offset);
            if (x->order) print_node(x->order, offset);
            if (x->limit) print_node(x->limit, offset);
        }

        // Sub-clauses
        else if (auto x = std::dynamic_pointer_cast<SetClause>(node)) {
            std::cout << "SET_CLAUSE\n";
            print_val(x->col_name, offset);
            print_node(x->val, offset);
        } else if (auto x = std::dynamic_pointer_cast<OrderBy>(node)) {
            std::cout << "ORDER_BY\n";
            print_node(x->cols, offset);
            static std::map<OrderByDir, std::string> dir_map{
                    {OrderBy_DEFAULT, "DEFAULT"},
                    {OrderBy_ASC, "ASC"},
                    {OrderBy_DESC, "DESC"},
            };
            print_val(dir_map.at(x->orderby_dir), offset);
        } else if (auto x = std::dynamic_pointer_cast<GroupBy>(node)) {
            std::cout << "GROUP_BY\n";
            print_node_list(x->cols, offset);
        } else if (auto x = std::dynamic_pointer_cast<LimitClause>(node)) {
            std::cout << "LIMIT\n";
            print_val(x->limit, offset);
            print_val(x->offset, offset);
        } else if (auto x = std::dynamic_pointer_cast<JoinExpr>(node)) {
            std::cout << "JOIN\n";
            print_val(jointype2str(x->type), offset);
            print_val(x->tab_name, offset);
            if (x->cond) print_node(x->cond, offset);
        }

        // Set statement
        else if (auto x = std::dynamic_pointer_cast<SetStmt>(node)) {
            std::cout << "SET\n";
            print_val(x->set_knob_type_ == EnableNestLoop ? "ENABLE_NESTLOOP" : "ENABLE_SORTMERGE", offset);
            print_val(x->bool_val_ ? "TRUE" : "FALSE", offset);
        }

        else {
            assert(0);
        }
    }
};

}
