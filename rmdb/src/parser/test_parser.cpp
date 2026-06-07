/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */
#undef NDEBUG

#include <cassert>

#include "parser.h"

int main() {
    std::vector<std::string> sqls = {
        // ================================================================
        // 基础命令
        // ================================================================
        "show tables;",
        "desc tb;",
        "help;",
        "exit;",
        "",  // EOF

        // ================================================================
        // DDL: 建表 / 删表 / 索引
        // ================================================================
        "create table tb (a int, b float, c char(4));",
        "drop table tb;",
        "create index tb(a);",
        "create index tb(a, b, c);",
        "drop index tb(a, b, c);",
        "drop index tb(b);",

        // ================================================================
        // DML: INSERT
        // ================================================================
        "insert into tb values (1, 3.14, 'pi');",
        "insert into tb values (TRUE, FALSE);",
        "insert into tb values (NULL);",

        // ================================================================
        // DML: DELETE
        // ================================================================
        "delete from tb where a = 1;",

        // ================================================================
        // DML: UPDATE
        // ================================================================
        "update tb set a = 1, b = 2.2, c = 'xyz' where x = 2 and y < 1.1 and z > 'abc';",

        // ================================================================
        // SELECT: 基础
        // ================================================================
        "select * from tb;",
        "select a from tb;",
        "select a, b, c from tb;",
        "select tb.a, tb.b from tb;",

        // ================================================================
        // SELECT: WHERE 比较运算符
        // ================================================================
        "select * from tb where a = 1;",
        "select * from tb where a <> 1;",
        "select * from tb where a < 1;",
        "select * from tb where a > 1;",
        "select * from tb where a <= 1;",
        "select * from tb where a >= 1;",
        "select * from tb where a < b;",                          // 列-列比较
        "select * from tb where x <> 2 and y >= 3. and z <= '123' and b < tb.a;",

        // ================================================================
        // SELECT: 算术表达式
        // ================================================================
        "select a + 1 from tb;",
        "select a - 1 from tb;",
        "select a * 2 from tb;",
        "select a / 2 from tb;",
        "select a % 2 from tb;",
        "select -a from tb;",                                     // 一元取负
        "select a + b * 2 from tb;",                              // 运算符优先级
        "select (a + b) * c from tb;",                            // 括号改变优先级
        "select a + b, c - d from tb;",

        // ================================================================
        // SELECT: 算术表达式在 WHERE 中（RHS 可以是表达式）
        // ================================================================
        "select a from tb where a = b + 1;",

        // ================================================================
        // SELECT: 聚合函数
        // ================================================================
        "select COUNT(*) from tb;",
        "select COUNT(a) from tb;",
        "select SUM(a) from tb;",
        "select AVG(a) from tb;",
        "select MAX(a) from tb;",
        "select MIN(a) from tb;",
        "select COUNT(a), SUM(b), AVG(c), MAX(d), MIN(e) from tb;",

        // ================================================================
        // SELECT: NULL 判断
        // ================================================================
        "select a from tb where b IS NULL;",
        "select a from tb where b IS NOT NULL;",

        // ================================================================
        // SELECT: LIKE 模式匹配
        // ================================================================
        "select a from tb where b LIKE 'prefix%';",
        "select a from tb where b NOT LIKE 'prefix%';",

        // ================================================================
        // SELECT: BETWEEN 范围判断
        // ================================================================
        "select a from tb where b BETWEEN 1 AND 10;",
        "select a from tb where b NOT BETWEEN 1 AND 10;",

        // ================================================================
        // SELECT: IN 列表判断
        // ================================================================
        "select a from tb where b IN (1, 2, 3);",
        "select a from tb where b NOT IN (1, 2, 3);",
        "select a from tb where b IN ('x', 'y', 'z');",

        // ================================================================
        // SELECT: 逻辑运算 OR / NOT
        // ================================================================
        "select a from tb where x = 1 OR y = 2;",
        "select a from tb where NOT x = 1;",
        "select a from tb where (x = 1 OR y = 2) AND z = 3;",
        "select a from tb where x = 1 AND (y = 2 OR z = 3);",

        // ================================================================
        // SELECT: DISTINCT
        // ================================================================
        "select DISTINCT a from tb;",
        "select DISTINCT a, b from tb;",

        // ================================================================
        // SELECT: ORDER BY
        // ================================================================
        "select a from tb order by a;",
        "select a from tb order by a ASC;",
        "select a from tb order by a DESC;",

        // ================================================================
        // SELECT: LIMIT / OFFSET
        // ================================================================
        "select a from tb LIMIT 10;",
        "select a from tb LIMIT 10 OFFSET 5;",

        // ================================================================
        // SELECT: 多表 FROM
        // ================================================================
        "select x.a, y.b from x, y where x.a = y.b and c = d;",

        // ================================================================
        // SELECT: JOIN (隐式 / 各方向)
        // ================================================================
        "select x.a, y.b from x join y where x.a = y.b and c = d;",
        "select a.x, b.y from a JOIN b ON a.id = b.id;",
        "select a.x, b.y from a INNER JOIN b ON a.id = b.id;",
        "select a.x, b.y from a LEFT JOIN b ON a.id = b.id;",
        "select a.x, b.y from a RIGHT JOIN b ON a.id = b.id;",
        "select a.x, b.y from a FULL JOIN b ON a.id = b.id;",
        // 多表 JOIN
        "select a.x, b.y, c.z from a JOIN b ON a.id = b.id JOIN c ON b.id = c.id;",

        // ================================================================
        // SELECT: GROUP BY / HAVING
        // ================================================================
        "select a, COUNT(*) from tb GROUP BY a;",
        "select a, SUM(b) from tb GROUP BY a HAVING a > 1;",
        "select a, b from tb GROUP BY a, b;",

        // ================================================================
        // SELECT: 复杂组合
        // ================================================================
        "select DISTINCT a, COUNT(*) from tb where b > 1 GROUP BY a HAVING a > 1 ORDER BY a DESC LIMIT 10;",

        // ================================================================
        // SELECT: WHERE 中布尔值
        // ================================================================
        "select a from tb where b = TRUE;",
        "select a from tb where b = FALSE;",

        // ================================================================
        // 事务语句
        // ================================================================
        "begin;",
        "commit;",
        "abort;",
        "rollback;",

        // ================================================================
        // SET 配置语句
        // ================================================================
        "set ENABLE_NESTLOOP = TRUE;",
        "set ENABLE_SORTMERGE = FALSE;",
    };
    for (auto &sql : sqls) {
        std::cout << sql << std::endl;
        YY_BUFFER_STATE buf = yy_scan_string(sql.c_str());
        assert(yyparse() == 0);
        if (ast::parse_tree != nullptr) {
            ast::TreePrinter::print(ast::parse_tree);
            yy_delete_buffer(buf);
            std::cout << std::endl;
        } else {
            std::cout << "exit/EOF" << std::endl;
        }
    }
    ast::parse_tree.reset();
    return 0;
}
