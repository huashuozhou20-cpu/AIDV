%{
#include "ast.h"
#include "yacc.tab.h"
#include <iostream>
#include <memory>

int yylex(YYSTYPE *yylval, YYLTYPE *yylloc);

void yyerror(YYLTYPE *locp, const char* s) {
    std::cerr << "Parser Error at line " << locp->first_line << " column " << locp->first_column << ": " << s << std::endl;
}

using namespace ast;
%}

// request a pure (reentrant) parser
%define api.pure full
// enable location in error handler
%locations
// enable verbose syntax error message
%define parse.error verbose

// keywords
%token SHOW TABLES CREATE TABLE DROP DESC INSERT INTO VALUES DELETE FROM ASC ORDER BY
%token WHERE UPDATE SET SELECT INT CHAR FLOAT INDEX AND JOIN EXIT HELP DEFAULT PRIMARY KEY AUTO_INCREMENT AS CONCAT VIEW UNIQUE DATE TEXT
%token TXN_BEGIN TXN_COMMIT TXN_ABORT TXN_ROLLBACK
%token ENABLE_NESTLOOP ENABLE_SORTMERGE
%token AVG BETWEEN COUNT DISTINCT FULL FOR GROUP HAVING IN INNER IS LEFT LIKE LIMIT
%token MAX MIN NOT OFFSET ON OR RIGHT SUM

// non-keywords (operators)
%token LEQ NEQ GEQ T_EOF

// type-specific tokens
%token <sv_str> IDENTIFIER VALUE_STRING
%token <sv_int> VALUE_INT
%token <sv_float> VALUE_FLOAT
%token <sv_bool> VALUE_BOOL
%token VALUE_NULL

// specify types for non-terminal symbols
%type <sv_node> stmt dbStmt ddl dml txnStmt setStmt
%type <sv_field> field
%type <sv_fields> fieldList
%type <sv_type_len> type
%type <sv_comp_op> op
%type <sv_expr> expr
%type <sv_exprs> selectItems selectItemList
%type <sv_val> value
%type <sv_vals> valueList
%type <sv_vals_list> valueTupleList
%type <sv_str> tbName colName optAliasStr
%type <sv_strs> colNameList fromList optColList
%type <sv_col> col
%type <sv_cols> colList
%type <sv_set_clause> setClause
%type <sv_set_clauses> setClauses
%type <sv_cond> condition optWhereClause optHaving
%type <sv_orderby> order_clause opt_order_clause
%type <sv_orderby_dir> opt_asc_desc
%type <sv_setKnobType> set_knob_type
%type <sv_join> joinClause
%type <sv_joins> joinList optJoinList
%type <sv_groupby> optGroupBy
%type <sv_limit> optLimit
%type <sv_agg> aggExpr
%type <sv_agg_type> aggType
%type <sv_int> joinType
%type <sv_bool> optDistinct optForUpdateClause

// intermediate non-terminals for expression and condition trees
%type <sv_cond> cond_or cond_and cond_not cond_base
%type <sv_expr> expr_add_sub expr_mul_div expr_unary expr_base
%type <sv_exprs> expr_list

%%

start:
        stmt ';'
    {
        parse_tree = $1;
        YYACCEPT;
    }
    |   HELP
    {
        parse_tree = std::make_shared<Help>();
        YYACCEPT;
    }
    |   EXIT
    {
        parse_tree = nullptr;
        YYACCEPT;
    }
    |   T_EOF
    {
        parse_tree = nullptr;
        YYACCEPT;
    }
    ;

stmt:
        dbStmt
    |   ddl
    |   dml
    |   txnStmt
    |   setStmt
    ;

txnStmt:
        TXN_BEGIN
    {
        $$ = std::make_shared<TxnBegin>();
    }
    |   TXN_COMMIT
    {
        $$ = std::make_shared<TxnCommit>();
    }
    |   TXN_ABORT
    {
        $$ = std::make_shared<TxnAbort>();
    }
    |   TXN_ROLLBACK
    {
        $$ = std::make_shared<TxnRollback>();
    }
    ;

dbStmt:
        SHOW TABLES
    {
        $$ = std::make_shared<ShowTables>();
    }
    ;

setStmt:
        SET set_knob_type '=' VALUE_BOOL
    {
        $$ = std::make_shared<SetStmt>($2, $4);
    }
    ;

ddl:
        CREATE TABLE tbName '(' fieldList ')'
    {
        $$ = std::make_shared<CreateTable>($3, $5);
    }
    |   DROP TABLE tbName
    {
        $$ = std::make_shared<DropTable>($3);
    }
    |   DESC tbName
    {
        $$ = std::make_shared<DescTable>($2);
    }
    |   CREATE INDEX tbName '(' colNameList ')'
    {
        $$ = std::make_shared<CreateIndex>($3, $5);
    }
    |   CREATE INDEX tbName ON tbName '(' colNameList ')'
    {
        // Alternative: CREATE INDEX name ON table (cols)
        $$ = std::make_shared<CreateIndex>($5, $7);
    }
    |   CREATE UNIQUE INDEX tbName ON tbName '(' colNameList ')'
    {
        $$ = std::make_shared<CreateIndex>($6, $8);
    }
    |   DROP INDEX tbName '(' colNameList ')'
    {
        $$ = std::make_shared<DropIndex>($3, $5);
    }
    |   CREATE VIEW tbName AS dml
    {
        $$ = std::make_shared<CreateView>($3, $5);
    }
    |   DROP VIEW tbName
    {
        $$ = std::make_shared<DropView>($3);
    }
    ;

dml:
        INSERT INTO tbName optColList VALUES valueTupleList
    {
        $$ = std::make_shared<InsertStmt>($3, $4, $6);
    }
    |   DELETE FROM tbName optWhereClause
    {
        $$ = std::make_shared<DeleteStmt>($3, $4);
    }
    |   UPDATE tbName SET setClauses optWhereClause
    {
        $$ = std::make_shared<UpdateStmt>($2, $4, $5);
    }
    |   SELECT optDistinct selectItems
        FROM fromList optJoinList optWhereClause optGroupBy optHaving opt_order_clause optLimit optForUpdateClause
    {
        auto stmt = std::make_shared<SelectStmt>();
        stmt->has_distinct = $2;
        // separate columns, aggregates, and expressions from select items
        for (auto &e : $3) {
            if (auto c = std::dynamic_pointer_cast<Col>(e)) {
                stmt->cols.push_back(c);
            } else if (auto a = std::dynamic_pointer_cast<AggExpr>(e)) {
                stmt->aggs.push_back(a);
                stmt->is_agg = true;
            } else {
                stmt->exprs.push_back(e);
            }
        }
        stmt->tabs = $5;
        stmt->joins = $6;
        // Collect join table names and aliases (alias AFTER table, like fromList)
        for (auto &j : stmt->joins) {
            stmt->tabs.push_back(j->tab_name);
            if (!j->alias.empty()) {
                stmt->table_aliases[j->alias] = j->tab_name;
                stmt->tabs.push_back(j->alias);
            }
        }
        stmt->cond = $7;
        stmt->group_by = $8;
        stmt->having = $9;
        stmt->order = $10;
        stmt->limit = $11;
        stmt->is_for_update = $12;
        $$ = stmt;
    }
    ;

fieldList:
        field
    {
        $$ = std::vector<std::shared_ptr<Field>>{$1};
    }
    |   fieldList ',' field
    {
        $$.push_back($3);
    }
    ;

colNameList:
        colName
    {
        $$ = std::vector<std::string>{$1};
    }
    |   colNameList ',' colName
    {
        $$.push_back($3);
    }
    ;

optColList:
        /* empty */
    {
        $$ = std::vector<std::string>{};
    }
    |   '(' colNameList ')'
    {
        $$ = $2;
    }
    ;

field:
        colName type
    {
        $$ = std::make_shared<ColDef>($1, $2, false, nullptr);
    }
    |   colName type VALUE_NULL
    {
        $$ = std::make_shared<ColDef>($1, $2, false, nullptr);
    }
    |   colName type NOT VALUE_NULL
    {
        $$ = std::make_shared<ColDef>($1, $2, true, nullptr);
    }
    |   colName type DEFAULT value
    {
        $$ = std::make_shared<ColDef>($1, $2, false, $4);
    }
    |   colName type NOT VALUE_NULL DEFAULT value
    {
        $$ = std::make_shared<ColDef>($1, $2, true, $6);
    }
    |   colName type PRIMARY KEY
    {
        auto col = std::make_shared<ColDef>($1, $2, true, nullptr);
        col->primary_key_ = true;
        $$ = col;
    }
    |   colName type AUTO_INCREMENT
    {
        auto col = std::make_shared<ColDef>($1, $2, false, nullptr);
        col->auto_increment_ = true;
        $$ = col;
    }
    |   colName type NOT VALUE_NULL AUTO_INCREMENT
    {
        auto col = std::make_shared<ColDef>($1, $2, true, nullptr);
        col->auto_increment_ = true;
        $$ = col;
    }
    |   colName type PRIMARY KEY AUTO_INCREMENT
    {
        auto col = std::make_shared<ColDef>($1, $2, true, nullptr);
        col->primary_key_ = true;
        col->auto_increment_ = true;
        $$ = col;
    }
    ;

type:
        INT
    {
        $$ = std::make_shared<TypeLen>(SV_TYPE_INT, sizeof(int));
    }
    |   CHAR '(' VALUE_INT ')'
    {
        // Multiply by 4 for UTF-8 multi-byte character support
        $$ = std::make_shared<TypeLen>(SV_TYPE_STRING, $3 * 4);
    }
    |   CHAR
    {
        // CHAR without length defaults to 4 bytes (1 char)
        $$ = std::make_shared<TypeLen>(SV_TYPE_STRING, 4);
    }
    |   DATE
    {
        $$ = std::make_shared<TypeLen>(SV_TYPE_STRING, 256);
    }
    |   TEXT
    {
        $$ = std::make_shared<TypeLen>(SV_TYPE_STRING, 256);
    }
    |   FLOAT
    {
        $$ = std::make_shared<TypeLen>(SV_TYPE_FLOAT, sizeof(float));
    }
    |   FLOAT '(' VALUE_INT ',' VALUE_INT ')'
    {
        // DECIMAL(p,s) / NUMERIC(p,s): ignore precision/scale, store as FLOAT
        $$ = std::make_shared<TypeLen>(SV_TYPE_FLOAT, sizeof(float));
    }
    ;

valueList:
        value
    {
        $$ = std::vector<std::shared_ptr<Value>>{$1};
    }
    |   valueList ',' value
    {
        $$.push_back($3);
    }
    ;

valueTupleList:
        '(' valueList ')'
    {
        $$ = std::vector<std::vector<std::shared_ptr<Value>>>{$2};
    }
    |   valueTupleList ',' '(' valueList ')'
    {
        $$.push_back($4);
    }
    ;

value:
        VALUE_INT
    {
        $$ = std::make_shared<IntLit>($1);
    }
    |   VALUE_FLOAT
    {
        $$ = std::make_shared<FloatLit>($1);
    }
    |   VALUE_STRING
    {
        $$ = std::make_shared<StringLit>($1);
    }
    |   VALUE_BOOL
    {
        $$ = std::make_shared<BoolLit>($1);
    }
    |   VALUE_NULL
    {
        $$ = std::make_shared<NullLit>();
    }
    ;

// ============================================================================
// Condition expression tree (WHERE / HAVING / ON)
// Precedence: OR < AND < NOT < comparison
// ============================================================================

condition:
        cond_or
    ;

cond_or:
        cond_and
    |   cond_or OR cond_and
    {
        $$ = std::make_shared<LogicExpr>(LOGIC_OR,
              std::vector<std::shared_ptr<CondExpr>>{$1, $3});
    }
    ;

cond_and:
        cond_not
    |   cond_and AND cond_not
    {
        $$ = std::make_shared<LogicExpr>(LOGIC_AND,
              std::vector<std::shared_ptr<CondExpr>>{$1, $3});
    }
    ;

cond_not:
        cond_base
    |   NOT cond_not
    {
        $$ = std::make_shared<LogicExpr>(LOGIC_NOT,
              std::vector<std::shared_ptr<CondExpr>>{$2});
    }
    ;

cond_base:
        expr op expr
    {
        $$ = std::make_shared<BinaryExpr>($1, $2, $3);
    }
    |   col IS VALUE_NULL
    {
        $$ = std::make_shared<UnaryCondExpr>($1, SV_OP_IS_NULL);
    }
    |   col IS NOT VALUE_NULL
    {
        $$ = std::make_shared<UnaryCondExpr>($1, SV_OP_IS_NOT_NULL);
    }
    |   col LIKE VALUE_STRING
    {
        $$ = std::make_shared<LikeExpr>($1, false, $3);
    }
    |   col NOT LIKE VALUE_STRING
    {
        $$ = std::make_shared<LikeExpr>($1, true, $4);
    }
    |   col BETWEEN value AND value
    {
        $$ = std::make_shared<BetweenExpr>($1, false, $3, $5);
    }
    |   col NOT BETWEEN value AND value
    {
        $$ = std::make_shared<BetweenExpr>($1, true, $4, $6);
    }
    |   col IN '(' valueList ')'
    {
        $$ = std::make_shared<InExpr>($1, false, $4);
    }
    |   col NOT IN '(' valueList ')'
    {
        $$ = std::make_shared<InExpr>($1, true, $5);
    }
    |   col IN '(' dml ')'
    {
        $$ = std::make_shared<InExpr>($1, false, std::vector<std::shared_ptr<Value>>{}, $4);
    }
    |   col NOT IN '(' dml ')'
    {
        $$ = std::make_shared<InExpr>($1, true, std::vector<std::shared_ptr<Value>>{}, $5);
    }
    |   '(' condition ')'
    {
        $$ = $2;
    }
    ;

optWhereClause:
        /* empty */
    {
        $$ = nullptr;
    }
    |   WHERE condition
    {
        $$ = $2;
    }
    ;

optHaving:
        /* empty */
    {
        $$ = nullptr;
    }
    |   HAVING condition
    {
        $$ = $2;
    }
    ;

// ============================================================================
// Expression (arithmetic + values + columns + aggregates)
// ============================================================================

expr:
        expr_add_sub
    ;

expr_add_sub:
        expr_mul_div
    |   expr_add_sub '+' expr_mul_div
    {
        $$ = std::make_shared<ArithExpr>($1, ARITH_ADD, $3);
    }
    |   expr_add_sub '-' expr_mul_div
    {
        $$ = std::make_shared<ArithExpr>($1, ARITH_SUB, $3);
    }
    ;

expr_mul_div:
        expr_unary
    |   expr_mul_div '*' expr_unary
    {
        $$ = std::make_shared<ArithExpr>($1, ARITH_MUL, $3);
    }
    |   expr_mul_div '/' expr_unary
    {
        $$ = std::make_shared<ArithExpr>($1, ARITH_DIV, $3);
    }
    |   expr_mul_div '%' expr_unary
    {
        $$ = std::make_shared<ArithExpr>($1, ARITH_MOD, $3);
    }
    ;

expr_unary:
        expr_base
    |   '-' expr_unary
    {
        $$ = std::make_shared<ArithExpr>(nullptr, ARITH_NEG, $2);
    }
    ;

expr_base:
        value
    {
        $$ = std::static_pointer_cast<Expr>($1);
    }
    |   col
    {
        $$ = std::static_pointer_cast<Expr>($1);
    }
    |   aggExpr
    {
        $$ = std::static_pointer_cast<Expr>($1);
    }
    |   CONCAT '(' expr_list ')'
    {
        $$ = std::make_shared<ConcatExpr>($3);
    }
    |   '(' dml ')'
    {
        $$ = std::make_shared<SubqueryExpr>($2);
    }
    |   '(' expr ')'
    {
        $$ = $2;
    }
    ;

// ============================================================================
// Aggregate expressions
// ============================================================================

aggType:
        COUNT   { $$ = AGG_COUNT; }
    |   SUM     { $$ = AGG_SUM; }
    |   AVG     { $$ = AGG_AVG; }
    |   MAX     { $$ = AGG_MAX; }
    |   MIN     { $$ = AGG_MIN; }
    ;

aggExpr:
        aggType '(' col ')'
    {
        $$ = std::make_shared<AggExpr>($1, $3->col_name);
    }
    |   aggType '(' '*' ')'
    {
        if ($1 == AGG_COUNT) {
            $$ = std::make_shared<AggExpr>(AGG_COUNT, "", true);
        } else {
            yyerror(&yylloc, "Only COUNT supports * argument");
            YYERROR;
        }
    }
    ;

// ============================================================================
// SELECT sub-clauses
// ============================================================================

optDistinct:
        /* empty */     { $$ = false; }
    |   DISTINCT        { $$ = true; }
    ;

optForUpdateClause:
        /* empty */     { $$ = false; }
    |   FOR UPDATE      { $$ = true; }
    ;

selectItems:
        '*'
    {
        $$ = std::vector<std::shared_ptr<Expr>>{};
    }
    |   selectItemList
    ;

selectItemList:
        expr opt_alias
    {
        $$ = std::vector<std::shared_ptr<Expr>>{$1};
    }
    |   selectItemList ',' expr opt_alias
    {
        $$.push_back($3);
    }
    ;

opt_alias:
        /* empty */     { }
    |   IDENTIFIER      { }
    |   AS IDENTIFIER   { }
    |   AS COUNT        { }
    |   AS AVG          { }
    |   AS SUM          { }
    |   AS MAX          { }
    |   AS MIN          { }
    ;

optAliasStr:
        /* empty */     { $$ = ""; }
    |   IDENTIFIER      { $$ = $1; }
    |   AS IDENTIFIER   { $$ = $2; }
    |   AS COUNT        { $$ = "count"; }
    |   AS AVG          { $$ = "avg"; }
    |   AS SUM          { $$ = "sum"; }
    |   AS MAX          { $$ = "max"; }
    |   AS MIN          { $$ = "min"; }
    ;

fromList:
        tbName optAliasStr
    {
        $$ = std::vector<std::string>{$1};
        if (!$2.empty()) $$.push_back($2);
    }
    |   fromList ',' tbName optAliasStr
    {
        $$.push_back($3);
        if (!$4.empty()) $$.push_back($4);
    }
    ;

optJoinList:
        /* empty */
    {
        $$ = std::vector<std::shared_ptr<JoinExpr>>{};
    }
    |   joinList
    {
        $$ = $1;
    }
    ;

joinList:
        joinClause
    {
        $$ = std::vector<std::shared_ptr<JoinExpr>>{$1};
    }
    |   joinList joinClause
    {
        $$.push_back($2);
    }
    ;

joinType:
        INNER   { $$ = INNER_JOIN; }
    |   LEFT    { $$ = LEFT_JOIN; }
    |   RIGHT   { $$ = RIGHT_JOIN; }
    |   FULL    { $$ = FULL_JOIN; }
    ;

joinClause:
        joinType JOIN tbName optAliasStr ON condition
    {
        $$ = std::make_shared<JoinExpr>($3, $6, static_cast<JoinType>($1), $4);
    }
    |   JOIN tbName optAliasStr ON condition
    {
        $$ = std::make_shared<JoinExpr>($2, $5, INNER_JOIN, $3);
    }
    |   joinType JOIN tbName optAliasStr
    {
        $$ = std::make_shared<JoinExpr>($3, nullptr, static_cast<JoinType>($1), $4);
    }
    |   JOIN tbName optAliasStr
    {
        $$ = std::make_shared<JoinExpr>($2, nullptr, INNER_JOIN, $3);
    }
    ;

optGroupBy:
        /* empty */
    {
        $$ = nullptr;
    }
    |   GROUP BY colList
    {
        $$ = std::make_shared<GroupBy>($3);
    }
    ;

optLimit:
        /* empty */
    {
        $$ = nullptr;
    }
    |   LIMIT VALUE_INT
    {
        $$ = std::make_shared<LimitClause>($2);
    }
    |   LIMIT VALUE_INT OFFSET VALUE_INT
    {
        $$ = std::make_shared<LimitClause>($2, $4);
    }
    ;

col:
        tbName '.' colName
    {
        $$ = std::make_shared<Col>($1, $3);
    }
    |   colName
    {
        $$ = std::make_shared<Col>("", $1);
    }
    ;

colList:
        col
    {
        $$ = std::vector<std::shared_ptr<Col>>{$1};
    }
    |   colList ',' col
    {
        $$.push_back($3);
    }
    ;

op:
        '='
    {
        $$ = SV_OP_EQ;
    }
    |   '<'
    {
        $$ = SV_OP_LT;
    }
    |   '>'
    {
        $$ = SV_OP_GT;
    }
    |   NEQ
    {
        $$ = SV_OP_NE;
    }
    |   LEQ
    {
        $$ = SV_OP_LE;
    }
    |   GEQ
    {
        $$ = SV_OP_GE;
    }
    ;

setClauses:
        setClause
    {
        $$ = std::vector<std::shared_ptr<SetClause>>{$1};
    }
    |   setClauses ',' setClause
    {
        $$.push_back($3);
    }
    ;

setClause:
        colName '=' value
    {
        $$ = std::make_shared<SetClause>($1, $3);
    }
    ;

opt_order_clause:
        ORDER BY order_clause
    {
        $$ = $3;
    }
    |   /* empty */ { /* ignore*/ }
    ;

order_clause:
        col opt_asc_desc
    {
        $$ = std::make_shared<OrderBy>($1, $2);
    }
    |   order_clause ',' col opt_asc_desc
    {
        // Multi-column ORDER BY — only first column is used
    }
    ;

opt_asc_desc:
        ASC     { $$ = OrderBy_ASC; }
    |   DESC    { $$ = OrderBy_DESC; }
    |           { $$ = OrderBy_DEFAULT; }
    ;

set_knob_type:
        ENABLE_NESTLOOP  { $$ = EnableNestLoop; }
    |   ENABLE_SORTMERGE { $$ = EnableSortMerge; }
    ;

expr_list:
        expr
    {
        $$ = std::vector<std::shared_ptr<Expr>>{$1};
    }
    |   expr_list ',' expr
    {
        $$.push_back($3);
    }
    ;

tbName: IDENTIFIER;

colName: IDENTIFIER;
%%
