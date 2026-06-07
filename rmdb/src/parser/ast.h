/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

/*
 * ast.h — 抽象语法树（AST）节点定义
 *
 * 这是整个 SQL 解析层的核心头文件，定义了所有语法树节点类型。
 *
 * 设计要点（学习数据库内核的关键知识）：
 *   1. 使用继承体系组织 AST 节点：TreeNode 是所有节点的基类
 *   2. 表达式 / 条件表达式分别形成独立的继承树，方便语义分析和优化器处理
 *   3. 使用 shared_ptr 管理节点生命周期，避免内存泄漏
 *   4. SemValue 是 yacc/lex 之间传递值的"语义值联合体"
 *
 * 继承结构一览：
 *   TreeNode
 *   ├── Help / ShowTables / TxnBegin / ...        （简单命令，无参数）
 *   ├── TypeLen / ColDef / Field                    （类型系统）
 *   ├── Expr                                        （表达式基类）
 *   │   ├── Value                                   （字面量基类）
 *   │   │   ├── IntLit / FloatLit / StringLit / BoolLit / NullLit
 *   │   ├── Col                                     （列引用）
 *   │   ├── AggExpr                                 （聚合函数: COUNT/SUM/AVG/MAX/MIN）
 *   │   └── ArithExpr                               （算术运算: + - * / %）
 *   ├── CondExpr                                    （条件表达式基类）
 *   │   ├── BinaryExpr                              （二元比较: a = b, x > 1）
 *   │   ├── LogicExpr                               （逻辑运算: AND/OR/NOT）
 *   │   ├── UnaryCondExpr                           （一元条件: IS NULL）
 *   │   ├── LikeExpr                                （LIKE 模式匹配）
 *   │   ├── BetweenExpr                             （BETWEEN 范围判断）
 *   │   └── InExpr                                  （IN 列表判断）
 *   ├── CreateTable / DropTable / DescTable / ...  （DDL 语句）
 *   ├── InsertStmt / DeleteStmt / UpdateStmt        （DML 写语句）
 *   └── SelectStmt                                  （SELECT 查询，最复杂）
 */

#pragma once

#include <map>
#include <vector>
#include <string>
#include <memory>

// ============================================================================
// 第一部分：枚举定义
// ============================================================================

// JOIN 类型
enum JoinType {
    INNER_JOIN, LEFT_JOIN, RIGHT_JOIN, FULL_JOIN
};

namespace ast {

// ---- 语义值的基础类型 ----
enum SvType {
    SV_TYPE_INT, SV_TYPE_FLOAT, SV_TYPE_STRING, SV_TYPE_BOOL, SV_TYPE_NULL
};

// ---- 比较运算符（扩展版，支持 LIKE/BETWEEN/IN/NULL 判断）----
enum SvCompOp {
    // 基础比较
    SV_OP_EQ, SV_OP_NE, SV_OP_LT, SV_OP_GT, SV_OP_LE, SV_OP_GE,
    // 模式匹配
    SV_OP_LIKE, SV_OP_NOT_LIKE,
    // 范围判断
    SV_OP_BETWEEN, SV_OP_NOT_BETWEEN,
    // 列表归属
    SV_OP_IN, SV_OP_NOT_IN,
    // NULL 判断
    SV_OP_IS_NULL, SV_OP_IS_NOT_NULL
};

// ---- 排序方向 ----
enum OrderByDir {
    OrderBy_DEFAULT, OrderBy_ASC, OrderBy_DESC
};

// ---- SET 语句的配置项 ----
enum SetKnobType {
    EnableNestLoop, EnableSortMerge
};

// ---- 逻辑运算符（AND / OR / NOT）----
enum LogicOp {
    LOGIC_AND, LOGIC_OR, LOGIC_NOT
};

// ---- 聚合函数类型 ----
enum AggType {
    AGG_COUNT, AGG_SUM, AGG_AVG, AGG_MAX, AGG_MIN
};

// ---- 算术运算符 ----
enum ArithOp {
    ARITH_ADD, ARITH_SUB, ARITH_MUL, ARITH_DIV, ARITH_MOD, ARITH_NEG
};

// ============================================================================
// 第二部分：AST 节点 — 基类
// ============================================================================

struct TreeNode {
    virtual ~TreeNode() = default;   // 必须虚析构，保证多态删除安全
};

// ============================================================================
// 第三部分：无需参数的命令节点
// ============================================================================

struct Help       : public TreeNode {};
struct ShowTables : public TreeNode {};
struct TxnBegin   : public TreeNode {};
struct TxnCommit  : public TreeNode {};
struct TxnAbort   : public TreeNode {};
struct TxnRollback: public TreeNode {};

// ============================================================================
// 第四部分：类型系统
// ============================================================================

/*
 * TypeLen: 描述列的类型及其长度
 *   例如 INT          → type=SV_TYPE_INT,    len=4
 *        FLOAT        → type=SV_TYPE_FLOAT,  len=4
 *        CHAR(20)     → type=SV_TYPE_STRING, len=20
 *
 * 为什么把类型和长度放在一起？
 *   在 SQL 解析中，列定义总是 "类型 + 可选长度"（如 CHAR(10)），
 *   将它们打包成一个节点可以简化后续处理。
 */
struct TypeLen : public TreeNode {
    SvType type;
    int len;
    TypeLen(SvType type_, int len_) : type(type_), len(len_) {}
};

// forward declaration for ColDef's default_val_ member
struct Value;

// Field 是列定义的基类（目前只有 ColDef，未来可扩展约束等）
struct Field : public TreeNode {};

struct ColDef : public Field {
    std::string col_name;
    std::shared_ptr<TypeLen> type_len;
    bool not_null_;
    bool primary_key_ = false;
    bool auto_increment_ = false;
    std::shared_ptr<Value> default_val_;  // DEFAULT value (nullptr if none)
    ColDef(std::string col_name_, std::shared_ptr<TypeLen> type_len_,
           bool not_null_ = false, std::shared_ptr<Value> default_val_ = nullptr)
        : col_name(std::move(col_name_)), type_len(std::move(type_len_)),
          not_null_(not_null_), default_val_(std::move(default_val_)) {}
};

// ============================================================================
// 第五部分：表达式系统
// ============================================================================

/*
 * 设计说明：
 *   表达式分为两类——"值表达式"（Expr）和"条件表达式"（CondExpr）。
 *   这对应 SQL 中两类不同的语法位置：
 *     - SELECT 后的列、VALUES 子句、SET 赋值 → Expr（用于计算值）
 *     - WHERE/HAVING/ON 子句               → CondExpr（用于过滤，结果为布尔）
 *
 *   为什么分开？
 *     1. 类型安全：WHERE 子句中不能出现 "SELECT 1+2 FROM t"，但可以出现 "WHERE a = 1+2"
 *     2. 语义分析：条件表达式最终产生布尔值，值表达式产生具体数据
 *     3. 优化器友好：条件表达式可以直接下推、重写，值表达式需要更多上下文
 */

struct Expr : public TreeNode {};
struct CondExpr : public TreeNode {};

// ---- 字面量 ----

struct Value : public Expr {
    virtual std::string to_string() const = 0;
};

struct IntLit : public Value {
    int val;
    IntLit(int val_) : val(val_) {}
    std::string to_string() const override { return std::to_string(val); }
};

struct FloatLit : public Value {
    float val;
    FloatLit(float val_) : val(val_) {}
    std::string to_string() const override { return std::to_string(val); }
};

struct StringLit : public Value {
    std::string val;
    StringLit(std::string val_) : val(std::move(val_)) {}
    std::string to_string() const override { return val; }
};

struct BoolLit : public Value {
    bool val;
    BoolLit(bool val_) : val(val_) {}
    std::string to_string() const override { return val ? "TRUE" : "FALSE"; }
};

/*
 * ConcatExpr: CONCAT(expr, expr, ...) 字符串拼接
 */
struct ConcatExpr : public Expr {
    std::vector<std::shared_ptr<Expr>> args;
    ConcatExpr(std::vector<std::shared_ptr<Expr>> args_) : args(std::move(args_)) {}
};

/*
 * SubqueryExpr: 子查询表达式  (SELECT ...)
 */
struct SubqueryExpr : public Expr {
    std::shared_ptr<TreeNode> subquery;
    SubqueryExpr(std::shared_ptr<TreeNode> sub) : subquery(std::move(sub)) {}
};

/*
 * NullLit: NULL 字面量
 *   为什么是一个独立节点而不是在 Value 中加个 is_null 字段？
 *     1. SQL 中 NULL 是类型无关的——可以出现在任何需要值的地方
 *     2. 独立节点使得类型推导阶段可以更好地处理 NULL 的类型传播
 *        （例如 INSERT INTO t VALUES (NULL) 中 NULL 的类型由目标列决定）
 */
struct NullLit : public Value {
    std::string to_string() const override { return "NULL"; }
};

// ---- 列引用 ----

struct Col : public Expr {
    std::string tab_name;   // 表名（可为空，表示未限定）
    std::string col_name;   // 列名
    Col(std::string tab_name_, std::string col_name_)
        : tab_name(std::move(tab_name_)), col_name(std::move(col_name_)) {}
};

// ---- 聚合函数 ----

/*
 * AggExpr: SQL 聚合函数调用
 *   例如: COUNT(*), SUM(a), AVG(b.x), MAX(price), MIN(quantity)
 *
 *   为什么把聚合函数放在表达式层而不是条件层？
 *     聚合函数产生的是一个值（如 SUM(a) 返回整数），
 *     它们出现在 SELECT 子句和 HAVING 子句中，本质上是值表达式。
 *     真正的条件（如 HAVING SUM(a) > 100）是由 BinaryExpr 对聚合结果做比较。
 */
struct AggExpr : public Expr {
    AggType agg_type;
    std::string col_name;   // 聚合的列名，COUNT(*)时为空
    bool is_star;           // true 表示 COUNT(*)
    AggExpr(AggType type, std::string col, bool star = false)
        : agg_type(type), col_name(std::move(col)), is_star(star) {}
};

// ---- 算术表达式 ----

/*
 * ArithExpr: 算术运算
 *   例如: a + 1, price * quantity, -discount
 *
 *   一元运算（如取负 -x）：rhs 为 nullptr
 *   二元运算（如 a+b）  ：lhs 和 rhs 都有值
 */
struct ArithExpr : public Expr {
    std::shared_ptr<Expr> lhs;
    ArithOp op;
    std::shared_ptr<Expr> rhs;   // 单目运算时为 nullptr
    ArithExpr(std::shared_ptr<Expr> lhs_, ArithOp op_,
              std::shared_ptr<Expr> rhs_ = nullptr)
        : lhs(std::move(lhs_)), op(op_), rhs(std::move(rhs_)) {}
};

// ============================================================================
// 第六部分：条件表达式系统（WHERE / HAVING / ON）
// ============================================================================

/*
 * 条件表达式树的设计：
 *
 *   在完整的 SQL 中，WHERE 子句是一个任意的布尔表达式：
 *     WHERE (a = 1 OR b = 2) AND c IS NOT NULL AND d LIKE 'prefix%'
 *
 *   这需要一棵树而非平坦的列表来表示。CondExpr 的各个子类形成这棵树的节点。
 *
 *   为什么不用 vector<BinaryExpr> 然后全部 AND 起来？
 *     1. 无法表达 OR 语义
 *     2. 无法表达嵌套条件（括号改变优先级）
 *     3. NOT 需要额外的标记
 *     4. LIKE / BETWEEN / IN / IS NULL 无法融入
 *
 *   逻辑预算符优先级（从低到高）：
 *     OR  <  AND  <  NOT
 *   这将在 yacc.y 的语法规则中通过递归定义实现。
 */

// AND / OR / NOT 逻辑组合
struct LogicExpr : public CondExpr {
    LogicOp op;
    std::vector<std::shared_ptr<CondExpr>> args;
    LogicExpr(LogicOp op_, std::vector<std::shared_ptr<CondExpr>> args_)
        : op(op_), args(std::move(args_)) {}
};

// 二元比较: expr op expr (col/agg/value op col/agg/value)
struct BinaryExpr : public CondExpr {
    std::shared_ptr<Expr> lhs;
    SvCompOp op;
    std::shared_ptr<Expr> rhs;
    BinaryExpr(std::shared_ptr<Expr> lhs_, SvCompOp op_, std::shared_ptr<Expr> rhs_)
        : lhs(std::move(lhs_)), op(op_), rhs(std::move(rhs_)) {}
};

// IS NULL / IS NOT NULL
struct UnaryCondExpr : public CondExpr {
    std::shared_ptr<Col> col;
    SvCompOp op;   // SV_OP_IS_NULL 或 SV_OP_IS_NOT_NULL
    UnaryCondExpr(std::shared_ptr<Col> col_, SvCompOp op_)
        : col(std::move(col_)), op(op_) {}
};

// LIKE / NOT LIKE
struct LikeExpr : public CondExpr {
    std::shared_ptr<Col> col;
    bool not_like;
    std::string pattern;
    LikeExpr(std::shared_ptr<Col> col_, bool not_like_, std::string pattern_)
        : col(std::move(col_)), not_like(not_like_), pattern(std::move(pattern_)) {}
};

// BETWEEN / NOT BETWEEN
struct BetweenExpr : public CondExpr {
    std::shared_ptr<Col> col;
    bool not_between;
    std::shared_ptr<Value> low;
    std::shared_ptr<Value> high;
    BetweenExpr(std::shared_ptr<Col> col_, bool not_between_,
                std::shared_ptr<Value> low_, std::shared_ptr<Value> high_)
        : col(std::move(col_)), not_between(not_between_),
          low(std::move(low_)), high(std::move(high_)) {}
};

// IN / NOT IN (值列表)
struct InExpr : public CondExpr {
    std::shared_ptr<Col> col;
    bool not_in;
    std::vector<std::shared_ptr<Value>> values;
    std::shared_ptr<TreeNode> subquery;  // IN (SELECT ...)
    InExpr(std::shared_ptr<Col> col_, bool not_in_,
           std::vector<std::shared_ptr<Value>> values_,
           std::shared_ptr<TreeNode> subquery_ = nullptr)
        : col(std::move(col_)), not_in(not_in_), values(std::move(values_)), subquery(std::move(subquery_)) {}
};

// ============================================================================
// 第七部分：DDL 语句节点
// ============================================================================

struct CreateTable : public TreeNode {
    std::string tab_name;
    std::vector<std::shared_ptr<Field>> fields;
    CreateTable(std::string tab_name_, std::vector<std::shared_ptr<Field>> fields_)
        : tab_name(std::move(tab_name_)), fields(std::move(fields_)) {}
};

struct DropTable : public TreeNode {
    std::string tab_name;
    DropTable(std::string tab_name_) : tab_name(std::move(tab_name_)) {}
};

/*
 * DescTable: DESCRIBE / DESC 表名  —— 查看表结构
 */
struct DescTable : public TreeNode {
    std::string tab_name;
    DescTable(std::string tab_name_) : tab_name(std::move(tab_name_)) {}
};

struct CreateIndex : public TreeNode {
    std::string tab_name;
    std::vector<std::string> col_names;
    CreateIndex(std::string tab_name_, std::vector<std::string> col_names_)
        : tab_name(std::move(tab_name_)), col_names(std::move(col_names_)) {}
};

struct DropIndex : public TreeNode {
    std::string tab_name;
    std::vector<std::string> col_names;
    DropIndex(std::string tab_name_, std::vector<std::string> col_names_)
        : tab_name(std::move(tab_name_)), col_names(std::move(col_names_)) {}
};

struct CreateView : public TreeNode {
    std::string view_name;
    std::shared_ptr<TreeNode> select_stmt;
    CreateView(std::string name, std::shared_ptr<TreeNode> stmt)
        : view_name(std::move(name)), select_stmt(std::move(stmt)) {}
};

struct DropView : public TreeNode {
    std::string view_name;
    DropView(std::string name) : view_name(std::move(name)) {}
};

// ============================================================================
// 第八部分：DML 语句节点
// ============================================================================

// ---- 辅助子句 ----

struct SetClause : public TreeNode {
    std::string col_name;
    std::shared_ptr<Value> val;
    SetClause(std::string col_name_, std::shared_ptr<Value> val_)
        : col_name(std::move(col_name_)), val(std::move(val_)) {}
};

struct OrderBy : public TreeNode {
    std::shared_ptr<Col> cols;
    OrderByDir orderby_dir;
    OrderBy(std::shared_ptr<Col> cols_, OrderByDir dir_)
        : cols(std::move(cols_)), orderby_dir(dir_) {}
};

/*
 * GroupBy: GROUP BY 子句
 *   例如: GROUP BY a, b
 */
struct GroupBy : public TreeNode {
    std::vector<std::shared_ptr<Col>> cols;
    GroupBy(std::vector<std::shared_ptr<Col>> cols_) : cols(std::move(cols_)) {}
};

/*
 * LimitClause: LIMIT 子句
 *   例如: LIMIT 10        → limit=10, offset=0
 *         LIMIT 10 OFFSET 5 → limit=10, offset=5
 */
struct LimitClause : public TreeNode {
    int limit;
    int offset;
    LimitClause(int limit_, int offset_ = 0) : limit(limit_), offset(offset_) {}
};

// ---- 写操作语句 ----

struct InsertStmt : public TreeNode {
    std::string tab_name;
    std::vector<std::string> col_names;  // optional column list
    std::vector<std::vector<std::shared_ptr<Value>>> vals_list;
    InsertStmt(std::string tab_name_, std::vector<std::string> col_names_,
               std::vector<std::vector<std::shared_ptr<Value>>> vals_list_)
        : tab_name(std::move(tab_name_)), col_names(std::move(col_names_)), vals_list(std::move(vals_list_)) {}
};

struct DeleteStmt : public TreeNode {
    std::string tab_name;
    std::shared_ptr<CondExpr> cond;     // WHERE 条件（可为 nullptr 表示无条件删除）
    DeleteStmt(std::string tab_name_, std::shared_ptr<CondExpr> cond_)
        : tab_name(std::move(tab_name_)), cond(std::move(cond_)) {}
};

struct UpdateStmt : public TreeNode {
    std::string tab_name;
    std::vector<std::shared_ptr<SetClause>> set_clauses;
    std::shared_ptr<CondExpr> cond;     // WHERE 条件（可为 nullptr）
    UpdateStmt(std::string tab_name_,
               std::vector<std::shared_ptr<SetClause>> set_clauses_,
               std::shared_ptr<CondExpr> cond_)
        : tab_name(std::move(tab_name_)), set_clauses(std::move(set_clauses_)),
          cond(std::move(cond_)) {}
};

// ---- JOIN 表达式 ----

/*
 * JoinExpr: 连接表达式
 *   例如: INNER JOIN b ON a.id = b.a_id
 *         LEFT JOIN c ON b.id = c.b_id
 *
 *   为什么把 JOIN 单独建模而不是放在 tableList 里？
 *     1. JOIN 有类型（INNER/LEFT/RIGHT/FULL），需要区分
 *     2. JOIN 有自己的 ON 条件，是完整的 CondExpr 树
 *     3. 多表 JOIN 形成一棵二叉树（left-deep join tree），
 *        optimizer 可以在此基础上做 join order 优化
 */
struct JoinExpr : public TreeNode {
    std::string tab_name;                     // 要 JOIN 的表名
    std::shared_ptr<CondExpr> cond;           // ON 条件（可为空表示无 ON）
    JoinType type;                            // 连接类型
    std::string alias;                        // 表别名（可为空）
    JoinExpr(std::string tab_name_, std::shared_ptr<CondExpr> cond_, JoinType type_,
             std::string alias_ = "")
        : tab_name(std::move(tab_name_)), cond(std::move(cond_)), type(type_),
          alias(std::move(alias_)) {}
};

// ---- SELECT 查询语句 ----

/*
 * SelectStmt: 最核心的 DML 节点
 *
 * 支持的完整 SELECT 语法：
 *   SELECT [DISTINCT] select_list
 *   FROM table_list
 *   [JOIN ... ON ...]
 *   [WHERE condition]
 *   [GROUP BY col_list]
 *   [HAVING condition]
 *   [ORDER BY col [ASC|DESC]]
 *   [LIMIT n [OFFSET m]]
 *
 * 字段说明：
 *   - cols:     SELECT 子句中的投影列（普通列引用）
 *   - aggs:     聚合函数列表（COUNT/SUM/AVG/MAX/MIN）
 *   - tabs:     FROM 子句中的表名列表（简单形式）
 *   - joins:    JOIN 子句列表（连接形式）
 *   - cond:     WHERE 条件（CondExpr 树）
 *   - group_by: GROUP BY 子句
 *   - having:   HAVING 条件（CondExpr 树，只能引用 GROUP BY 列和聚合函数）
 *   - order:    ORDER BY 子句
 *   - limit:    LIMIT 子句
 *   - has_distinct: 是否有 DISTINCT 关键字
 *   - is_agg:   是否包含聚合函数（决定是否需要 GROUP BY 处理）
 */
struct SelectStmt : public TreeNode {
    std::vector<std::shared_ptr<Col>> cols;
    std::vector<std::shared_ptr<AggExpr>> aggs;
    std::vector<std::shared_ptr<Expr>> exprs;  // arithmetic and other expressions
    std::vector<std::string> tabs;
    std::vector<std::shared_ptr<JoinExpr>> joins;
    // alias → real table name
    std::map<std::string, std::string> table_aliases;
    std::shared_ptr<CondExpr> cond;
    std::shared_ptr<GroupBy> group_by;
    std::shared_ptr<CondExpr> having;
    std::shared_ptr<OrderBy> order;
    std::shared_ptr<LimitClause> limit;
    bool has_distinct;
    bool is_agg;
    bool is_for_update;

    SelectStmt() : has_distinct(false), is_agg(false), is_for_update(false) {}

    SelectStmt(std::vector<std::shared_ptr<Col>> cols_,
               std::vector<std::string> tabs_,
               std::shared_ptr<CondExpr> cond_,
               std::shared_ptr<OrderBy> order_)
        : cols(std::move(cols_)), tabs(std::move(tabs_)), cond(std::move(cond_)),
          order(std::move(order_)),
          has_distinct(false), is_agg(false), is_for_update(false) {}
};

// ---- SET 配置语句 ----

struct SetStmt : public TreeNode {
    SetKnobType set_knob_type_;
    bool bool_val_;
    SetStmt(SetKnobType type, bool bool_value)
        : set_knob_type_(type), bool_val_(bool_value) {}
};

// ============================================================================
// 第九部分：语义值联合体（SemValue）
// ============================================================================

/*
 * SemValue 是 yacc 和 lex 之间传递值的"胶水"结构。
 *
 * 在设计一个数据库内核的解析器时，SemValue 的设计至关重要：
 *   - yacc 的每个语法符号（终结符和非终结符）都需要一个类型
 *   - 终结符的值由 lex 填充（如 VALUE_INT 的 sv_int）
 *   - 非终结符的值由 yacc 规则中的 $$ = ...  赋值（如 stmt 的 sv_node）
 *
 * %token <sv_int>  VALUE_INT    的含义：
 *   当 lex 返回 VALUE_INT 时，yacc 从 yylval->sv_int 读取这个整数值
 *
 * %type <sv_node> stmt           的含义：
 *   语法规则中 stmt 的值（$$）可以作为一个 TreeNode 节点被引用（$1, $2, ...）
 *
 * 为什么使用 union-like struct 而不是 C 的 union？
 *   1. C++ 的 union 不支持带有析构函数的类型（如 std::string, shared_ptr）
 *   2. 使用 struct 可以同时持有多种类型的值（虽然增加了内存占用）
 *   3. 在实际数据库系统中，通常会使用 std::variant 或自定义 tagged union
 *
 * 实践中，每个语法动作只用这个 struct 中的一个或少数几个字段。
 * 未使用的字段会被默认初始化（int→0, float→0.0, shared_ptr→nullptr 等）。
 */
struct SemValue {
    // ---- 基础类型 ----
    int sv_int;
    float sv_float;
    std::string sv_str;
    bool sv_bool;
    OrderByDir sv_orderby_dir;
    std::vector<std::string> sv_strs;

    // ---- 通用节点指针 ----
    std::shared_ptr<TreeNode> sv_node;

    // ---- 枚举值 ----
    SvCompOp sv_comp_op;
    LogicOp  sv_logic_op;
    AggType  sv_agg_type;
    ArithOp  sv_arith_op;

    // ---- 类型/字段 ----
    std::shared_ptr<TypeLen> sv_type_len;
    std::shared_ptr<Field>   sv_field;
    std::vector<std::shared_ptr<Field>> sv_fields;

    // ---- 表达式 ----
    std::shared_ptr<Expr> sv_expr;
    std::vector<std::shared_ptr<Expr>> sv_exprs;

    // ---- 值 ----
    std::shared_ptr<Value> sv_val;
    std::vector<std::shared_ptr<Value>> sv_vals;
    std::vector<std::vector<std::shared_ptr<Value>>> sv_vals_list;

    // ---- 列 ----
    std::shared_ptr<Col> sv_col;
    std::vector<std::shared_ptr<Col>> sv_cols;

    // ---- SET 子句 ----
    std::shared_ptr<SetClause> sv_set_clause;
    std::vector<std::shared_ptr<SetClause>> sv_set_clauses;

    // ---- 条件表达式 ----
    std::shared_ptr<CondExpr> sv_cond;

    // ---- ORDER BY / GROUP BY / LIMIT ----
    std::shared_ptr<OrderBy>     sv_orderby;
    std::shared_ptr<GroupBy>     sv_groupby;
    std::shared_ptr<LimitClause> sv_limit;

    // ---- 聚合 ----
    std::shared_ptr<AggExpr> sv_agg;
    std::vector<std::shared_ptr<AggExpr>> sv_aggs;

    // ---- JOIN ----
    std::shared_ptr<JoinExpr> sv_join;
    std::vector<std::shared_ptr<JoinExpr>> sv_joins;

    // ---- SET 配置 ----
    SetKnobType sv_setKnobType;
};

/*
 * parse_tree 是解析的最终产物。
 * yyparse() 成功返回 0 后，parse_tree 指向根 AST 节点。
 * 调用者通过 dynamic_pointer_cast 判断具体类型。
 *
 * 例如:
 *   if (auto stmt = dynamic_pointer_cast<SelectStmt>(parse_tree)) {
 *       // 处理 SELECT 语句
 *   }
 */
extern std::shared_ptr<ast::TreeNode> parse_tree;

}  // namespace ast

// 告知 yacc：语义值的类型是 ast::SemValue
#define YYSTYPE ast::SemValue
