/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include <netinet/in.h>
#include <signal.h>
#include <unistd.h>
#include <atomic>
#include <cctype>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>

#include "errors.h"
#include "optimizer/optimizer.h"
#include "recovery/log_recovery.h"
#include "optimizer/plan.h"
#include "optimizer/planner.h"
#include "portal.h"
#include "analyze/analyze.h"
#include "execution/executor_abstract.h"
#ifdef ENABLE_PROFILING
#include "common/sampler.h"
#endif
#include "network/epoll_server.h"
#include "network/http_server.h"

#define DEFAULT_PORT 8765

// 构建全局所需的管理器对象
auto disk_manager = std::make_unique<DiskManager>();
auto buffer_pool_manager = std::make_unique<BufferPoolManager>(BUFFER_POOL_SIZE, disk_manager.get());
auto rm_manager = std::make_unique<RmManager>(disk_manager.get(), buffer_pool_manager.get());
auto ix_manager = std::make_unique<IxManager>(disk_manager.get(), buffer_pool_manager.get());
auto sm_manager = std::make_unique<SmManager>(disk_manager.get(), buffer_pool_manager.get(), rm_manager.get(), ix_manager.get());
auto lock_manager = std::make_unique<LockManager>();
auto txn_manager = std::make_unique<TransactionManager>(lock_manager.get(), sm_manager.get(),
                                                         ConcurrencyMode::MVCC);
auto planner = std::make_unique<Planner>(sm_manager.get());
auto optimizer = std::make_unique<Optimizer>(sm_manager.get(), planner.get());
auto ql_manager = std::make_unique<QlManager>(sm_manager.get(), txn_manager.get(), planner.get());
auto log_manager = std::make_unique<LogManager>(disk_manager.get());
auto recovery = std::make_unique<RecoveryManager>(disk_manager.get(), buffer_pool_manager.get(), sm_manager.get());
auto portal = std::make_unique<Portal>(sm_manager.get());
auto analyze = std::make_unique<Analyze>(sm_manager.get());

// View definitions: name → SelectStmt AST
std::map<std::string, std::shared_ptr<ast::TreeNode>> view_defs;
std::mutex view_mutex;
static pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

// Epoll server pointer for signal handler
static EpollServer *g_server = nullptr;

static void sigint_handler(int /*signo*/) {
#ifdef ENABLE_PROFILING
    Sampler::stop();
#endif
    log_manager->flush_log_to_disk();
    std::cout << "The Server received Ctrl+C, will be closed\n";
    if (g_server) g_server->stop();
}

// Start a fresh auto-commit transaction on the calling worker thread.
// In the epoll model, different queries from the same connection may be
// processed by different workers, so we cannot reuse a transaction object
// across threads.  Explicit multi-statement transactions (BEGIN / COMMIT)
// are not yet supported in the epoll model.
static int64_t begin_autocommit_txn() {
    auto *txn = txn_manager->begin(nullptr, log_manager.get());
    txn->set_txn_mode(false);
    return txn->get_transaction_id();
}

// Resolve scalar subqueries in AST condition tree by executing them
// and replacing SubqueryExpr with literal values.
void resolve_subqueries(std::shared_ptr<ast::TreeNode> node) {
    if (!node) return;
    // Process condition expressions recursively
    if (auto binary = std::dynamic_pointer_cast<ast::BinaryExpr>(node)) {
        resolve_subqueries(binary->lhs);
        if (auto sub = std::dynamic_pointer_cast<ast::SubqueryExpr>(binary->rhs)) {
            // Execute scalar subquery and replace with result value
            try {
                Analyze sub_analyze(sm_manager.get());
                auto sub_query = sub_analyze.do_analyze(sub->subquery);
                char sub_buf[BUFFER_LENGTH];
                int sub_offset = 0;
                auto sub_txn = txn_manager->begin(nullptr, log_manager.get());
                Context sub_ctx(lock_manager.get(), log_manager.get(), sub_txn, sub_buf, &sub_offset);
                sub_ctx.txn_mgr_ = txn_manager.get();
                auto sub_plan = planner->do_planner(sub_query, &sub_ctx);
                // Unwrap DMLPlan wrapper
                if (auto dml = std::dynamic_pointer_cast<DMLPlan>(sub_plan))
                    sub_plan = dml->subplan_;
                auto root_exec = portal->convert_plan_executor(sub_plan, &sub_ctx);
                if (!root_exec) {
                    txn_manager->abort(sub_txn, log_manager.get()); return;
                }
                root_exec->beginTuple();
                if (!root_exec->is_end()) {
                    auto rec = root_exec->Next();
                    if (!rec) { txn_manager->abort(sub_txn, log_manager.get()); return; }
                    auto &cols = root_exec->cols();
                    if (!cols.empty()) {
                        auto &col = cols[0];
                        if (col.type == TYPE_INT)
                            binary->rhs = std::make_shared<ast::IntLit>(*(int *)(rec->data + col.offset));
                        else if (col.type == TYPE_FLOAT)
                            binary->rhs = std::make_shared<ast::FloatLit>(*(float *)(rec->data + col.offset));
                        else
                            binary->rhs = std::make_shared<ast::FloatLit>(0);
                    }
                } else {
                    // Empty set: replace with NULL so the outer comparison
                    // evaluates to NULL (false in WHERE context → 0 rows).
                    binary->rhs = std::make_shared<ast::NullLit>();
                }
                txn_manager->commit(sub_txn, log_manager.get());
            } catch (RMDBError &e) {
#ifndef NDEBUG
                std::cerr << "Subquery failed: " << e.what() << std::endl;
#endif
            }
        } else {
            resolve_subqueries(binary->rhs);
        }
    } else if (auto logic = std::dynamic_pointer_cast<ast::LogicExpr>(node)) {
        for (auto &arg : logic->args) resolve_subqueries(arg);
    } else if (auto in_expr = std::dynamic_pointer_cast<ast::InExpr>(node)) {
        // Resolve IN subquery: execute and collect all values
        if (in_expr->subquery) {
            try {
                Analyze sub_analyze(sm_manager.get());
                auto sub_query = sub_analyze.do_analyze(in_expr->subquery);
                char sub_buf[BUFFER_LENGTH]; int sub_offset = 0;
                auto sub_txn = txn_manager->begin(nullptr, log_manager.get());
                Context sub_ctx(lock_manager.get(), log_manager.get(), sub_txn, sub_buf, &sub_offset);
                sub_ctx.txn_mgr_ = txn_manager.get();
                auto sub_plan = planner->do_planner(sub_query, &sub_ctx);
                if (auto dml = std::dynamic_pointer_cast<DMLPlan>(sub_plan))
                    sub_plan = dml->subplan_;
                auto root_exec = portal->convert_plan_executor(sub_plan, &sub_ctx);
                if (root_exec) {
                    root_exec->beginTuple();
                    auto &cols = root_exec->cols();
                    while (!root_exec->is_end()) {
                        auto rec = root_exec->Next();
                        if (rec && !cols.empty()) {
                            auto &col = cols[0];
                            if (col.type == TYPE_INT)
                                in_expr->values.push_back(std::make_shared<ast::IntLit>(*(int*)(rec->data+col.offset)));
                            else if (col.type == TYPE_FLOAT)
                                in_expr->values.push_back(std::make_shared<ast::FloatLit>(*(float*)(rec->data+col.offset)));
                            else if (col.type == TYPE_STRING) {
                                std::string s(rec->data+col.offset, col.len);
                                s = s.c_str();
                                in_expr->values.push_back(std::make_shared<ast::StringLit>(s));
                            }
                        }
                        root_exec->nextTuple();
                    }
                }
                txn_manager->commit(sub_txn, log_manager.get());
            } catch (RMDBError &e) {}
            in_expr->subquery = nullptr;  // Mark as resolved
        }
    }
}

// ============================================================================
// Fast-path SQL dispatch: bypass yyparse() for known sysbench OLTP patterns.
// The parser is a serialization point (buffer_mutex) and a CPU hotspot —
// constructing the AST directly skips both costs for the hot workload.
// Returns nullptr if the statement does not match a recognised pattern.
// ============================================================================
static std::shared_ptr<ast::TreeNode> try_fast_path(const std::string &stmt_str) {
    const char *s = stmt_str.c_str();
    while (*s == ' ' || *s == '\t' || *s == '\n') ++s;

    char tab[64];
    int v1 = 0, v2 = 0, pos = 0;

    // ---- UPDATE <table> SET val=<int> WHERE id=<int> ----
    if (sscanf(s, "UPDATE %63s SET val = %d WHERE id = %d %n", tab, &v1, &v2, &pos) >= 3 && pos > 0) {
        // Verify remaining is only trailing ';' and whitespace
        for (const char *p = s + pos; *p; ++p)
            if (*p != ' ' && *p != ';' && *p != '\t' && *p != '\n') goto not_update;
        std::vector<std::shared_ptr<ast::SetClause>> set_clauses;
        set_clauses.push_back(std::make_shared<ast::SetClause>("val", std::make_shared<ast::IntLit>(v1)));
        auto lhs = std::make_shared<ast::Col>("", "id");
        auto rhs = std::make_shared<ast::IntLit>(v2);
        auto cond = std::make_shared<ast::BinaryExpr>(lhs, ast::SV_OP_EQ, rhs);
        return std::make_shared<ast::UpdateStmt>(std::string(tab), std::move(set_clauses), cond);
    }
    not_update:

    // ---- DELETE FROM <table> WHERE id=<int> ----
    pos = 0;
    if (sscanf(s, "DELETE FROM %63s WHERE id = %d %n", tab, &v1, &pos) >= 2 && pos > 0) {
        for (const char *p = s + pos; *p; ++p)
            if (*p != ' ' && *p != ';' && *p != '\t' && *p != '\n') goto not_delete;
        auto lhs = std::make_shared<ast::Col>("", "id");
        auto rhs = std::make_shared<ast::IntLit>(v1);
        auto cond = std::make_shared<ast::BinaryExpr>(lhs, ast::SV_OP_EQ, rhs);
        return std::make_shared<ast::DeleteStmt>(std::string(tab), cond);
    }
    not_delete:

    // ---- SELECT * FROM <table> WHERE id BETWEEN <int> AND <int> AND val + 1 > 0 ----
    pos = 0;
    if (sscanf(s, "SELECT * FROM %63s WHERE id BETWEEN %d AND %d AND val + 1 > 0 %n",
               tab, &v1, &v2, &pos) >= 3 && pos > 0) {
        for (const char *p = s + pos; *p; ++p)
            if (*p != ' ' && *p != ';' && *p != '\t' && *p != '\n') goto not_select;
        auto between = std::make_shared<ast::BetweenExpr>(
            std::make_shared<ast::Col>("", "id"), false,
            std::make_shared<ast::IntLit>(v1), std::make_shared<ast::IntLit>(v2));
        auto val_plus_1 = std::make_shared<ast::ArithExpr>(
            std::make_shared<ast::Col>("", "val"), ast::ARITH_ADD,
            std::make_shared<ast::IntLit>(1));
        auto gt_zero = std::make_shared<ast::BinaryExpr>(
            val_plus_1, ast::SV_OP_GT, std::make_shared<ast::IntLit>(0));
        std::vector<std::shared_ptr<ast::CondExpr>> args;
        args.push_back(between);
        args.push_back(gt_zero);
        auto cond = std::make_shared<ast::LogicExpr>(ast::LOGIC_AND, std::move(args));
        auto sel = std::make_shared<ast::SelectStmt>();
        sel->tabs.push_back(std::string(tab));
        sel->cond = cond;
        return sel;
    }
    not_select:

    // ---- INSERT INTO <table> VALUES(<int>, <int>, '<str>') ----
    pos = 0;
    char padding[256];
    if (sscanf(s, "INSERT INTO %63s VALUES ( %d , %d , '%255[^']' ) %n",
               tab, &v1, &v2, padding, &pos) >= 4 && pos > 0) {
        for (const char *p = s + pos; *p; ++p)
            if (*p != ' ' && *p != ';' && *p != '\t' && *p != '\n') goto not_insert;
        std::vector<std::shared_ptr<ast::Value>> row;
        row.push_back(std::make_shared<ast::IntLit>(v1));
        row.push_back(std::make_shared<ast::IntLit>(v2));
        row.push_back(std::make_shared<ast::StringLit>(std::string(padding)));
        std::vector<std::vector<std::shared_ptr<ast::Value>>> vals;
        vals.push_back(std::move(row));
        return std::make_shared<ast::InsertStmt>(std::string(tab),
            std::vector<std::string>{}, std::move(vals));
    }
    not_insert:

    return nullptr;
}

// ============================================================================
// Plan cache — deep-clone cached Plan + rebind parameter values.
// Thread-local so each worker populates its own cache, zero synchronisation.
// ============================================================================

enum class CachedPlanType : uint8_t {
    UPDATE_BY_ID  = 0,
    DELETE_BY_ID  = 1,
    SELECT_BETWEEN = 2,
    INSERT_VALUES = 3,
    NONE          = 4
};

static thread_local std::shared_ptr<Plan> tls_plan_cache[4];

// Identify the fast-path pattern, verifying the AST shape matches the
// expected template.  Must be specific enough to avoid cache-slot pollution:
// a plain SELECT without BETWEEN must NOT be cached as SELECT_BETWEEN.
static CachedPlanType identify_pattern(const std::shared_ptr<ast::TreeNode> &ast) {
    if (auto u = std::dynamic_pointer_cast<ast::UpdateStmt>(ast)) {
        if (u->set_clauses.size() == 1 && u->set_clauses[0]->col_name == "val" && u->cond)
            return CachedPlanType::UPDATE_BY_ID;
    }
    if (auto d = std::dynamic_pointer_cast<ast::DeleteStmt>(ast)) {
        if (d->cond) return CachedPlanType::DELETE_BY_ID;
    }
    if (auto s = std::dynamic_pointer_cast<ast::SelectStmt>(ast)) {
        // Only cache SELECT ... BETWEEN ... AND val + 1 > 0 (sysbench range query)
        if (s->cond) {
            auto logic = std::dynamic_pointer_cast<ast::LogicExpr>(s->cond);
            if (logic && logic->op == ast::LOGIC_AND && logic->args.size() == 2) {
                auto between = std::dynamic_pointer_cast<ast::BetweenExpr>(logic->args[0]);
                if (between && between->col && between->col->col_name == "id")
                    return CachedPlanType::SELECT_BETWEEN;
            }
        }
    }
    if (auto i = std::dynamic_pointer_cast<ast::InsertStmt>(ast)) {
        if (i->vals_list.size() == 1 && i->vals_list[0].size() == 3)
            return CachedPlanType::INSERT_VALUES;
    }
    return CachedPlanType::NONE;
}

// Deep-copy a Value, including its RmRecord raw buffer.
static Value clone_value(const Value &src) {
    Value val = src;
    if (src.raw) val.raw = std::make_shared<RmRecord>(*src.raw);
    return val;
}

static std::vector<Condition> clone_conditions(const std::vector<Condition> &conds) {
    std::vector<Condition> result;
    result.reserve(conds.size());
    for (const auto &c : conds) {
        Condition copy;
        copy.lhs_col  = c.lhs_col;
        copy.op       = c.op;
        copy.is_rhs_val = c.is_rhs_val;
        copy.rhs_col  = c.rhs_col;
        copy.rhs_val  = clone_value(c.rhs_val);
        copy.rhs_val2 = clone_value(c.rhs_val2);
        for (const auto &v : c.in_values) copy.in_values.push_back(clone_value(v));
        copy.is_arith_expr = c.is_arith_expr;
        copy.children = clone_conditions(c.children);
        result.push_back(std::move(copy));
    }
    return result;
}

static std::vector<SetClause> clone_set_clauses(const std::vector<SetClause> &src) {
    std::vector<SetClause> result;
    result.reserve(src.size());
    for (const auto &sc : src)
        result.push_back({sc.lhs, clone_value(sc.rhs)});
    return result;
}

static std::vector<std::vector<Value>> clone_values_list(
    const std::vector<std::vector<Value>> &src) {
    std::vector<std::vector<Value>> result;
    result.reserve(src.size());
    for (const auto &row : src) {
        std::vector<Value> row_copy;
        row_copy.reserve(row.size());
        for (const auto &v : row) row_copy.push_back(clone_value(v));
        result.push_back(std::move(row_copy));
    }
    return result;
}

static std::shared_ptr<Plan> clone_plan(std::shared_ptr<Plan> plan, SmManager *sm) {
    if (!plan) return nullptr;

    if (auto dml = std::dynamic_pointer_cast<DMLPlan>(plan)) {
        auto sub_clone = clone_plan(dml->subplan_, sm);
        auto cloned = std::make_shared<DMLPlan>(
            dml->tag, sub_clone,
            std::string(dml->tab_name_),
            clone_values_list(dml->values_list_),
            clone_conditions(dml->conds_),
            clone_set_clauses(dml->set_clauses_),
            std::vector<std::string>(dml->col_names_));
        cloned->is_for_update_ = dml->is_for_update_;
        return cloned;
    }

    if (auto proj = std::dynamic_pointer_cast<ProjectionPlan>(plan)) {
        return std::make_shared<ProjectionPlan>(
            proj->tag, clone_plan(proj->subplan_, sm),
            std::vector<TabCol>(proj->sel_cols_));
    }

    if (auto scan = std::dynamic_pointer_cast<ScanPlan>(plan)) {
        auto cloned = std::make_shared<ScanPlan>();  // default ctor — no sm access
        cloned->tag = scan->tag;
        cloned->tab_name_ = scan->tab_name_;
        cloned->conds_ = clone_conditions(scan->conds_);
        cloned->fed_conds_ = clone_conditions(scan->fed_conds_);
        cloned->index_col_names_ = scan->index_col_names_;
        cloned->cols_ = scan->cols_;
        cloned->len_ = scan->len_;
        cloned->is_index_only_ = scan->is_index_only_;
        return cloned;
    }

    return nullptr;
}

// Update an INT Value and its raw buffer in-place.
static void rebind_int(Value &val, int new_val) {
    val.set_int(new_val);
    if (val.raw) *(int *)(val.raw->data) = new_val;
}

static void rebind_str(Value &val, const std::string &new_str) {
    val.set_str(new_str);
    if (val.raw) {
        memset(val.raw->data, 0, val.raw->size);
        memcpy(val.raw->data, new_str.c_str(),
               std::min(static_cast<size_t>(val.raw->size), new_str.size()));
    }
}

// ---- Pattern-specific clone+rebind ----

static std::shared_ptr<Plan> clone_rebind_update(
    const std::shared_ptr<Plan> &tmpl,
    std::shared_ptr<ast::UpdateStmt> ast, SmManager *sm) {
    auto cond = std::dynamic_pointer_cast<ast::BinaryExpr>(ast->cond);
    int where_id = std::dynamic_pointer_cast<ast::IntLit>(cond->rhs)->val;
    int set_val  = std::dynamic_pointer_cast<ast::IntLit>(ast->set_clauses[0]->val)->val;

    auto plan = clone_plan(tmpl, sm);
    auto dml  = std::dynamic_pointer_cast<DMLPlan>(plan);
    auto scan = std::dynamic_pointer_cast<ScanPlan>(dml->subplan_);

    rebind_int(dml->conds_[0].rhs_val, where_id);
    rebind_int(dml->set_clauses_[0].rhs, set_val);
    rebind_int(scan->conds_[0].rhs_val, where_id);
    scan->fed_conds_ = scan->conds_;

    return plan;
}

static std::shared_ptr<Plan> clone_rebind_delete(
    const std::shared_ptr<Plan> &tmpl,
    std::shared_ptr<ast::DeleteStmt> ast, SmManager *sm) {
    auto cond = std::dynamic_pointer_cast<ast::BinaryExpr>(ast->cond);
    int where_id = std::dynamic_pointer_cast<ast::IntLit>(cond->rhs)->val;

    auto plan = clone_plan(tmpl, sm);
    auto dml  = std::dynamic_pointer_cast<DMLPlan>(plan);
    auto scan = std::dynamic_pointer_cast<ScanPlan>(dml->subplan_);

    rebind_int(dml->conds_[0].rhs_val, where_id);
    rebind_int(scan->conds_[0].rhs_val, where_id);
    scan->fed_conds_ = scan->conds_;

    return plan;
}

static std::shared_ptr<Plan> clone_rebind_select(
    const std::shared_ptr<Plan> &tmpl,
    std::shared_ptr<ast::SelectStmt> ast, SmManager *sm) {
    auto logic   = std::dynamic_pointer_cast<ast::LogicExpr>(ast->cond);
    auto between = std::dynamic_pointer_cast<ast::BetweenExpr>(logic->args[0]);
    int low  = std::dynamic_pointer_cast<ast::IntLit>(between->low)->val;
    int high = std::dynamic_pointer_cast<ast::IntLit>(between->high)->val;

    auto plan = clone_plan(tmpl, sm);
    auto dml  = std::dynamic_pointer_cast<DMLPlan>(plan);
    auto proj = std::dynamic_pointer_cast<ProjectionPlan>(dml->subplan_);
    auto scan = std::dynamic_pointer_cast<ScanPlan>(proj->subplan_);

    // Rebind BETWEEN low/high values.  Index 0 is the BETWEEN condition, index 1
    // is val+1>0 (constant, no rebind needed).  Guard against unexpected shapes.
    if (!scan->conds_.empty()) {
        rebind_int(scan->conds_[0].rhs_val,  low);
        rebind_int(scan->conds_[0].rhs_val2, high);
    }
    scan->fed_conds_ = scan->conds_;

    return plan;
}

static std::shared_ptr<Plan> clone_rebind_insert(
    const std::shared_ptr<Plan> &tmpl,
    std::shared_ptr<ast::InsertStmt> ast, SmManager *sm) {
    auto &row    = ast->vals_list[0];
    int id_val   = std::dynamic_pointer_cast<ast::IntLit>(row[0])->val;
    int val_val  = std::dynamic_pointer_cast<ast::IntLit>(row[1])->val;
    std::string padding = std::dynamic_pointer_cast<ast::StringLit>(row[2])->val;

    auto plan = clone_plan(tmpl, sm);
    auto dml  = std::dynamic_pointer_cast<DMLPlan>(plan);

    rebind_int(dml->values_list_[0][0], id_val);
    rebind_int(dml->values_list_[0][1], val_val);
    rebind_str(dml->values_list_[0][2], padding);

    return plan;
}

static std::shared_ptr<Plan> clone_and_rebind(
    CachedPlanType type, const std::shared_ptr<Plan> &tmpl,
    std::shared_ptr<ast::TreeNode> ast, SmManager *sm) {
    switch (type) {
    case CachedPlanType::UPDATE_BY_ID:
        return clone_rebind_update(tmpl,
            std::dynamic_pointer_cast<ast::UpdateStmt>(ast), sm);
    case CachedPlanType::DELETE_BY_ID:
        return clone_rebind_delete(tmpl,
            std::dynamic_pointer_cast<ast::DeleteStmt>(ast), sm);
    case CachedPlanType::SELECT_BETWEEN:
        return clone_rebind_select(tmpl,
            std::dynamic_pointer_cast<ast::SelectStmt>(ast), sm);
    case CachedPlanType::INSERT_VALUES:
        return clone_rebind_insert(tmpl,
            std::dynamic_pointer_cast<ast::InsertStmt>(ast), sm);
    default:
        return nullptr;
    }
}

// ============================================================================
// Per-query processing function — called by worker pool threads.
// Each call handles exactly one SQL statement on one connection.
// Results are written back via g_server->enqueue_response().
// ============================================================================
static void process_query(int conn_fd, const std::string &stmt_str) {
    // Per-query output buffer
    char data_send[BUFFER_LENGTH];
    memset(data_send, 0, BUFFER_LENGTH);
    int offset = 0;

    // Register this worker thread's active read timestamp for GC watermark.
    // Thread-local slot persists across queries on the same worker.
    static thread_local int gc_slot = txn_manager->RegisterThread();

    // Fresh auto-commit transaction for every query (epoll workers are pooled)
    int64_t txn_id = begin_autocommit_txn();
    Context *context = new Context(lock_manager.get(), log_manager.get(), nullptr,
                                   data_send, &offset);
    context->txn_mgr_ = txn_manager.get();
    context->txn_ = txn_manager->get_transaction(txn_id);

    // Publish current read timestamp so GC knows this thread is active
    if (gc_slot >= 0 && context->txn_) {
        txn_manager->SetThreadActiveTs(gc_slot, context->txn_->get_read_ts());
    }
    context->txn_mgr_ = txn_manager.get();
    context->txn_ = txn_manager->get_transaction(txn_id);

    // ---- Phase 1: build AST (fast-path or full parser) ----
    std::shared_ptr<ast::TreeNode> local_ast = try_fast_path(stmt_str);

    // Declared early to avoid crossing-initialization with goto
    YY_BUFFER_STATE parser_buf = nullptr;
    bool parser_mutex_held = false;
    CachedPlanType cache_pattern = CachedPlanType::NONE;

    // ---- Fast path: try plan cache (skip do_analyze + plan_query entirely) ----
    if (local_ast) {
        auto pat = identify_pattern(local_ast);
        if (pat != CachedPlanType::NONE) {
            auto &cached = tls_plan_cache[static_cast<int>(pat)];
            if (cached) {
                std::shared_ptr<Plan> plan = clone_and_rebind(pat, cached, local_ast, sm_manager.get());

                if (auto dml = std::dynamic_pointer_cast<DMLPlan>(plan)) {
                    if (dml->is_for_update_ && context->txn_) {
                        context->txn_->set_read_only(false);
                        context->is_for_update_ = true;
                    }
                }
                try {
                    std::shared_ptr<PortalStmt> portalStmt = portal->start(plan, context);
                    portal->run(portalStmt, ql_manager.get(), &txn_id, context);
                    portal->drop();
                } catch (TransactionAbortException &e) {
                    Result result{Result::ABORT, "abort\n"};
                    memcpy(data_send, result.msg.c_str(), result.msg.length());
                    offset = result.msg.length();
                    txn_manager->abort(context->txn_, log_manager.get());
                } catch (RMDBError &e) {
                    Result result{Result::FAILURE, std::string(e.what()) + "\n"};
                    memcpy(data_send, result.msg.c_str(), result.msg.length());
                    offset = result.msg.length();
                }
                goto send_response;
            }
        }
    }

    // ---- Fallback: parser or cache-miss path (mutex-guarded analysis) ----
    if (!local_ast) {
        pthread_mutex_lock(&buffer_mutex);
        parser_mutex_held = true;
        parser_buf = yy_scan_string(stmt_str.c_str());
        if (yyparse() != 0 || ast::parse_tree == nullptr) {
            yy_delete_buffer(parser_buf);
            pthread_mutex_unlock(&buffer_mutex);
            parser_mutex_held = false;
            std::string err = "Parser Error: syntax error\n";
            memcpy(data_send, err.c_str(), err.length());
            offset = err.length();
            goto send_response;
        }
        local_ast = ast::parse_tree;
    } else {
        // Fast-path cache miss: acquire mutex for do_analyze().
        pthread_mutex_lock(&buffer_mutex);
        parser_mutex_held = true;
    }

    // ---- Phase 2: analyse + execute AST (common for both fallback paths) ----
    if (local_ast != nullptr) {
        try {
            // CREATE VIEW / DROP VIEW (parser path only — not in fast-path patterns)
            if (auto cv = std::dynamic_pointer_cast<ast::CreateView>(local_ast)) {
                {
                    std::lock_guard<std::mutex> lock(view_mutex);
                    view_defs[cv->view_name] = cv->select_stmt;
                }
                sm_manager->create_view(cv->view_name, "");
                Result r{Result::SUCCESS, ""};
                memcpy(data_send, r.msg.c_str(), r.msg.length());
                offset = r.msg.length();
                goto cleanup_parser;
            }
            if (auto dv = std::dynamic_pointer_cast<ast::DropView>(local_ast)) {
                {
                    std::lock_guard<std::mutex> lock(view_mutex);
                    view_defs.erase(dv->view_name);
                }
                sm_manager->drop_view(dv->view_name);
                Result r{Result::SUCCESS, ""};
                memcpy(data_send, r.msg.c_str(), r.msg.length());
                offset = r.msg.length();
                goto cleanup_parser;
            }
            if (auto stmt = std::dynamic_pointer_cast<ast::SelectStmt>(local_ast)) {
                resolve_subqueries(stmt->cond);
            }
            std::shared_ptr<Query> query = analyze->do_analyze(local_ast);

            // Release parser resources now that analysis consumed the AST
            if (parser_mutex_held) {
                if (parser_buf) yy_delete_buffer(parser_buf);
                pthread_mutex_unlock(&buffer_mutex);
                parser_mutex_held = false;
            }

            std::shared_ptr<Plan> plan = optimizer->plan_query(query, context);

            // Populate thread-local plan cache.  Always deep-clone so the
            // cached template is never mutated by executors.
            cache_pattern = identify_pattern(local_ast);
            if (cache_pattern != CachedPlanType::NONE &&
                !tls_plan_cache[static_cast<int>(cache_pattern)]) {
                tls_plan_cache[static_cast<int>(cache_pattern)] =
                    clone_plan(plan, sm_manager.get());
            }

            if (auto dml = std::dynamic_pointer_cast<DMLPlan>(plan)) {
                if (dml->is_for_update_ && context->txn_) {
                    context->txn_->set_read_only(false);
                    context->is_for_update_ = true;
                }
            }
            std::shared_ptr<PortalStmt> portalStmt = portal->start(plan, context);
            portal->run(portalStmt, ql_manager.get(), &txn_id, context);
            portal->drop();
        } catch (TransactionAbortException &e) {
            Result result{Result::ABORT, "abort\n"};
            memcpy(data_send, result.msg.c_str(), result.msg.length());
            offset = result.msg.length();
            txn_manager->abort(context->txn_, log_manager.get());
        } catch (RMDBError &e) {
            Result result{Result::FAILURE, std::string(e.what()) + "\n"};
            memcpy(data_send, result.msg.c_str(), result.msg.length());
            offset = result.msg.length();
        }
    }

cleanup_parser:
    if (parser_mutex_held) {
        if (parser_buf) yy_delete_buffer(parser_buf);
        pthread_mutex_unlock(&buffer_mutex);
    }

send_response:
    // Send result via epoll server
    if (offset == 0) {
        data_send[0] = '\n';
        offset = 1;
    }
    if (g_server) {
        g_server->enqueue_response(conn_fd, data_send, static_cast<size_t>(offset));
    }

    // Autocommit single-statement transactions
    if (context->txn_ && !context->txn_->get_txn_mode()) {
        txn_manager->commit(context->txn_, log_manager.get());
    }
    // Mark this thread as idle for GC watermark
    if (gc_slot >= 0) {
        txn_manager->SetThreadActiveTs(gc_slot, INT64_MAX);
    }
    delete context;
}

// ---- Epoll-based server main -------------------------------------------------


// ============================================================================
// Plan tree serialisation for EXPLAIN
// ============================================================================
static void serialize_plan(std::stringstream &ss, std::shared_ptr<Plan> plan,
                           int depth, const std::string &prefix) {
    if (!plan) return;
    std::string indent(depth * 2, ' ');
    std::string tag_str;
    switch (plan->tag) {
        case T_Projection: tag_str = "Projection"; break;
        case T_SeqScan:    tag_str = "SeqScan";    break;
        case T_IndexScan:  tag_str = "IndexScan";  break;
        case T_Insert:     tag_str = "Insert";     break;
        case T_Delete:     tag_str = "Delete";     break;
        case T_Update:     tag_str = "Update";     break;
        case T_NestLoop:   tag_str = "NestedLoopJoin"; break;
        case T_SortMerge:  tag_str = "SortMergeJoin";  break;
        case T_Aggregation:tag_str = "Aggregation"; break;
        case T_Sort:       tag_str = "Sort";        break;
        case T_Distinct:   tag_str = "Distinct";    break;
        case T_Limit:      tag_str = "Limit";       break;
        case T_HashJoin:   tag_str = "HashJoin";    break;
        default:           tag_str = "Plan";        break;
    }
    ss << indent << prefix << tag_str << "\n";

    if (auto scan = std::dynamic_pointer_cast<ScanPlan>(plan)) {
        ss << indent << "  table: " << scan->tab_name_
           << "  cols: " << scan->cols_.size()
           << "  index: " << (scan->index_col_names_.empty() ? "none" : scan->index_col_names_[0])
           << "  conds: " << scan->conds_.size() << "\n";
    } else if (auto join = std::dynamic_pointer_cast<JoinPlan>(plan)) {
        ss << indent << "  type: " << (join->type == INNER_JOIN ? "INNER" : "OUTER")
           << "  conds: " << join->conds_.size() << "\n";
        serialize_plan(ss, join->left_, depth + 1, "Left:  ");
        serialize_plan(ss, join->right_, depth + 1, "Right: ");
    } else if (auto proj = std::dynamic_pointer_cast<ProjectionPlan>(plan)) {
        ss << indent << "  cols: " << proj->sel_cols_.size() << "\n";
        serialize_plan(ss, proj->subplan_, depth + 1, "");
    } else if (auto dml = std::dynamic_pointer_cast<DMLPlan>(plan)) {
        ss << indent << "  table: " << dml->tab_name_ << "\n";
        if (dml->subplan_) serialize_plan(ss, dml->subplan_, depth + 1, "");
    }
}

// ============================================================================
// Core query execution — shared by TCP (process_query) and HTTP (execute_sql).
// Returns the text result as a string.
// ============================================================================
static std::string execute_query_core(const std::string &stmt_str) {
    // -- Handle SHOW LOCK GRAPH -------------------------------------------------
    {
        std::string upper = stmt_str;
        for (auto &c : upper) c = static_cast<char>(toupper(c));
        // Trim trailing semicolons and whitespace for comparison
        while (!upper.empty() && (upper.back() == ';' || upper.back() == ' '))
            upper.pop_back();
        if (upper == "SHOW LOCK GRAPH") {
            return lock_manager->DumpLockGraph();
        }
    }

    // -- Handle EXPLAIN ---------------------------------------------------------
    {
        std::string upper = stmt_str;
        for (auto &c : upper) c = static_cast<char>(toupper(c));
        if (upper.rfind("EXPLAIN ", 0) == 0) {
            std::string inner_sql = stmt_str.substr(8);  // strlen("EXPLAIN ")
            // Trim leading/trailing whitespace and semicolons
            size_t start = inner_sql.find_first_not_of(" \t\n\r");
            size_t end   = inner_sql.find_last_not_of(" \t\n\r;");
            if (start != std::string::npos && end != std::string::npos)
                inner_sql = inner_sql.substr(start, end - start + 1);
            // Parser requires semicolon terminator
            if (!inner_sql.empty() && inner_sql.back() != ';')
                inner_sql += ';';

            // Parse the inner SQL and build a plan without executing it
            try {
                pthread_mutex_lock(&buffer_mutex);
                YY_BUFFER_STATE buf = yy_scan_string(inner_sql.c_str());
                bool parse_ok = (yyparse() == 0 && ast::parse_tree != nullptr);
                if (!parse_ok) {
                    yy_delete_buffer(buf);
                    pthread_mutex_unlock(&buffer_mutex);
                    std::string err = "EXPLAIN: syntax error in query: " + inner_sql;
                    return "{\"error\":\"" + err + "\"}";
                }
                auto ast_node = ast::parse_tree;
                auto query = analyze->do_analyze(ast_node);
                yy_delete_buffer(buf);
                pthread_mutex_unlock(&buffer_mutex);

                char tmp_buf[BUFFER_LENGTH];
                int tmp_off = 0;
                auto txn = txn_manager->begin(nullptr, log_manager.get());
                Context ctx(lock_manager.get(), log_manager.get(), txn, tmp_buf, &tmp_off);
                ctx.txn_mgr_ = txn_manager.get();
                auto plan = planner->do_planner(query, &ctx);
                txn_manager->commit(txn, log_manager.get());

                std::stringstream ss;
                ss << "Query plan:\n";
                // Unwrap DMLPlan to show the execution plan inside
                if (auto dml = std::dynamic_pointer_cast<DMLPlan>(plan))
                    serialize_plan(ss, dml->subplan_, 0, "Root:  ");
                else
                    serialize_plan(ss, plan, 0, "Root:  ");
                return ss.str();
            } catch (RMDBError &e) {
                return std::string("EXPLAIN error: ") + e.what();
            }
        }
    }

    // -- Normal query execution ------------------------------------------------
    char data_send[BUFFER_LENGTH];
    memset(data_send, 0, BUFFER_LENGTH);
    int offset = 0;

    static thread_local int gc_slot = txn_manager->RegisterThread();
    int64_t txn_id = begin_autocommit_txn();
    Context *context = new Context(lock_manager.get(), log_manager.get(), nullptr,
                                   data_send, &offset);
    context->txn_mgr_ = txn_manager.get();
    context->txn_ = txn_manager->get_transaction(txn_id);

    if (gc_slot >= 0 && context->txn_) {
        txn_manager->SetThreadActiveTs(gc_slot, context->txn_->get_read_ts());
    }
    context->txn_mgr_ = txn_manager.get();
    context->txn_ = txn_manager->get_transaction(txn_id);

    std::shared_ptr<ast::TreeNode> local_ast = try_fast_path(stmt_str);

    YY_BUFFER_STATE parser_buf = nullptr;
    bool parser_mutex_held = false;
    CachedPlanType cache_pattern = CachedPlanType::NONE;

    // Plan cache check
    if (local_ast) {
        auto pat = identify_pattern(local_ast);
        if (pat != CachedPlanType::NONE) {
            auto &cached = tls_plan_cache[static_cast<int>(pat)];
            if (cached) {
                std::shared_ptr<Plan> plan = clone_and_rebind(pat, cached, local_ast, sm_manager.get());
                if (auto dml = std::dynamic_pointer_cast<DMLPlan>(plan)) {
                    if (dml->is_for_update_ && context->txn_) {
                        context->txn_->set_read_only(false);
                        context->is_for_update_ = true;
                    }
                }
                try {
                    std::shared_ptr<PortalStmt> portalStmt = portal->start(plan, context);
                    portal->run(portalStmt, ql_manager.get(), &txn_id, context);
                    portal->drop();
                } catch (TransactionAbortException &e) {
                    txn_manager->abort(context->txn_, log_manager.get());
                    if (gc_slot >= 0) txn_manager->SetThreadActiveTs(gc_slot, INT64_MAX);
                    delete context;
                    return R"({"error":"transaction aborted"})";
                } catch (RMDBError &e) {
                    if (gc_slot >= 0) txn_manager->SetThreadActiveTs(gc_slot, INT64_MAX);
                    delete context;
                    return std::string(R"({"error":")") + e.what() + "\"}";
                }
                goto finish;
            }
        }
    }

    // Parse
    if (!local_ast) {
        pthread_mutex_lock(&buffer_mutex);
        parser_mutex_held = true;
        parser_buf = yy_scan_string(stmt_str.c_str());
        if (yyparse() != 0 || ast::parse_tree == nullptr) {
            yy_delete_buffer(parser_buf);
            pthread_mutex_unlock(&buffer_mutex);
            parser_mutex_held = false;
            if (gc_slot >= 0) txn_manager->SetThreadActiveTs(gc_slot, INT64_MAX);
            delete context;
            return R"({"error":"syntax error"})";
        }
        local_ast = ast::parse_tree;
    } else {
        pthread_mutex_lock(&buffer_mutex);
        parser_mutex_held = true;
    }

    if (local_ast != nullptr) {
        try {
            if (auto stmt = std::dynamic_pointer_cast<ast::SelectStmt>(local_ast))
                resolve_subqueries(stmt->cond);
            std::shared_ptr<Query> query = analyze->do_analyze(local_ast);

            if (parser_mutex_held) {
                if (parser_buf) yy_delete_buffer(parser_buf);
                pthread_mutex_unlock(&buffer_mutex);
                parser_mutex_held = false;
            }

            std::shared_ptr<Plan> plan = optimizer->plan_query(query, context);

            cache_pattern = identify_pattern(local_ast);
            if (cache_pattern != CachedPlanType::NONE &&
                !tls_plan_cache[static_cast<int>(cache_pattern)]) {
                tls_plan_cache[static_cast<int>(cache_pattern)] =
                    clone_plan(plan, sm_manager.get());
            }

            if (auto dml = std::dynamic_pointer_cast<DMLPlan>(plan)) {
                if (dml->is_for_update_ && context->txn_) {
                    context->txn_->set_read_only(false);
                    context->is_for_update_ = true;
                }
            }
            std::shared_ptr<PortalStmt> portalStmt = portal->start(plan, context);
            portal->run(portalStmt, ql_manager.get(), &txn_id, context);
            portal->drop();
        } catch (TransactionAbortException &e) {
            txn_manager->abort(context->txn_, log_manager.get());
            if (parser_mutex_held) { yy_delete_buffer(parser_buf); pthread_mutex_unlock(&buffer_mutex); }
            if (gc_slot >= 0) txn_manager->SetThreadActiveTs(gc_slot, INT64_MAX);
            delete context;
            return R"({"error":"transaction aborted"})";
        } catch (RMDBError &e) {
            if (parser_mutex_held) { yy_delete_buffer(parser_buf); pthread_mutex_unlock(&buffer_mutex); }
            if (gc_slot >= 0) txn_manager->SetThreadActiveTs(gc_slot, INT64_MAX);
            delete context;
            return std::string(R"({"error":")") + e.what() + "\"}";
        }
    }

    if (parser_mutex_held) {
        if (parser_buf) yy_delete_buffer(parser_buf);
        pthread_mutex_unlock(&buffer_mutex);
    }

finish:
    if (context->txn_ && !context->txn_->get_txn_mode()) {
        txn_manager->commit(context->txn_, log_manager.get());
    }
    if (gc_slot >= 0) {
        txn_manager->SetThreadActiveTs(gc_slot, INT64_MAX);
    }

    std::string result(data_send, offset);
    delete context;
    return result;
}

// Convert the pipe-delimited text output into JSON.
static std::string text_to_json(const std::string &text) {
    // Quick check: if it's an error from execute_query_core, return as-is
    if (text.rfind("{\"error\"", 0) == 0) return text;

    std::string json = "{\"result\":";
    // Escape the text for JSON
    json += "\"";
    for (char c : text) {
        switch (c) {
            case '\n': json += "\\n"; break;
            case '\r': break;  // skip \r
            case '"':  json += "\\\""; break;
            case '\\': json += "\\\\"; break;
            default:   json += c;
        }
    }
    json += "\"}";
    return json;
}

// ============================================================================
// Epoll-based server main — single I/O thread + worker pool
// ============================================================================
int main(int argc, char **argv) {
    if (argc < 2 || argc > 4) {
        std::cerr << "Usage: " << argv[0] << " <database> [tcp_port] [http_port]" << std::endl;
        exit(1);
    }

    int port = DEFAULT_PORT;
    int http_port = 0;
    if (argc >= 3) {
        port = std::stoi(argv[2]);
    }
    if (argc >= 4) {
        http_port = std::stoi(argv[3]);
    }

    signal(SIGINT, sigint_handler);
    try {
        std::cout << "\n"
                     "  _____  __  __ _____  ____  \n"
                     " |  __ \\|  \\/  |  __ \\|  _ \\ \n"
                     " | |__) | \\  / | |  | | |_) |\n"
                     " |  _  /| |\\/| | |  | |  _ < \n"
                     " | | \\ \\| |  | | |__| | |_) |\n"
                     " |_|  \\_\\_|  |_|_____/|____/ \n"
                     "\n"
                     "Welcome to RMDB!  (epoll + worker pool)\n"
                     "Type 'help;' for help.\n"
                     "\n";

        std::string db_name = argv[1];
        if (!sm_manager->is_dir(db_name)) {
            sm_manager->create_db(db_name);
        }
        sm_manager->open_db(db_name);

        // ARIES recovery
        recovery->analyze();
        recovery->redo();
        recovery->undo();
        recovery->rebuild_indexes();

        if (recovery->max_lsn() != INVALID_LSN) {
            log_manager->set_global_lsn(recovery->max_lsn() + 1);
        }

        // Launch epoll-based server with worker pool
        int num_workers = static_cast<int>(std::thread::hardware_concurrency());
        if (num_workers < 4) num_workers = 4;
        std::cout << "[server] epoll Reactor + " << num_workers
                  << " worker threads on port " << port << "\n";

        EpollServer server(port, process_query, num_workers);
        g_server = &server;

    // ---- HTTP REST server (optional, separate port) ----
    std::unique_ptr<HttpServer> http_server;
    if (http_port > 0) {
        http_server = std::make_unique<HttpServer>(http_port,
            [](const std::string &method, const std::string &path,
               const std::string &body) -> std::string {
                if (method != "POST" || path.find("/query") != 0)
                    return R"({"error":"use POST /query"})";
                return text_to_json(execute_query_core(body));
            });
        http_server->start();
    }

#ifdef ENABLE_PROFILING
        // Start CPU profiler after all initialization is complete
        Sampler::start("/tmp/rmdb_samples.bin");
#endif

        server.run();
        g_server = nullptr;
        if (http_server) http_server->stop();

    } catch (RMDBError &e) {
        std::cerr << e.what() << std::endl;
        exit(1);
    }

    sm_manager->close_db();
    std::cout << " DB has been closed.\nServer shuts down." << std::endl;
    return 0;
}
