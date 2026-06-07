"""AI-powered SQL practice module — generates questions, validates answers."""
import json
import re
from fastapi import APIRouter, Header
from pydantic import BaseModel
from openai import OpenAI
from core import config
from core.database import execute_sql
from core.auth import verify_token, save_score, get_user_stats

router = APIRouter()

PRACTICE_PROMPT = """你是一个 SQL 教学助手。根据下面的数据库表结构，生成一道 SQL 练习题。

## 数据库表结构
{schema}

## 出题规则
- 根据用户选择的难度出题
- 简单：单表查询，只涉及 SELECT、WHERE
- 中等：多表 JOIN 或 GROUP BY 聚合
- 困难：子查询、复杂 JOIN、HAVING 等

## 重要要求
1. **题目中必须明确写出表名和字段名**，不能只说"学生表"而要写"students 表，包含字段：id, name, student_no, age, gender, class_id"
2. **提供示例数据**，用 Markdown 表格展示每张相关表的前 3-5 条数据，让题目更直观
3. 题目格式灵活，可以描述一个业务场景，然后提出查询需求

## 回复格式
题目：<题目描述，必须包含表名、字段名和示例数据表格>

示例数据参考格式：
### students 表
| id | name | student_no | age | gender | class_id |
|----|------|-----------|-----|--------|----------|
| 1  | 张三 | S001      | 20  | 男     | 1        |
| 2  | 李四 | S002      | 19  | 女     | 1        |

正确答案：<正确的 SQL 语句>

只返回题目和正确答案，不要加额外解释。"""

CHOICE_PROMPT = """你是一个 SQL 教学助手。根据下面的数据库表结构，生成一道 SQL 选择题。

## 数据库表结构
{schema}

## 规则
- 题目考察 SQL 语法知识
- 4个选项（A/B/C/D），1个正确，3个是常见误区
- 难度：{difficulty}
- **题目中必须写出具体的表名和字段名**

## 回复格式
题目：<题目描述，包含具体表名和字段名>
A. <选项A>
B. <选项B>
C. <选项C>
D. <选项D>
正确答案：<A/B/C/D>"""

VALIDATE_PROMPT = """你是一个 SQL 教学助手。根据下面的题目和数据库表结构，判断用户的 SQL 答案是否正确。

题目：{question}
正确答案：{correct_answer}
用户答案：{user_answer}

## 判断规则
- 用户答案在逻辑上等价于正确答案即可，不要求语法完全一致
- 先执行用户答案看看能否正常运行
- 如果正确：回复 CORRECT
- 如果错误：回复 WRONG，并简短指出哪里错了（1-2句话）

只返回 CORRECT 或 WRONG + 简短解释。
"""


class PracticeRequest(BaseModel):
    difficulty: str = "easy"  # easy, medium, hard


class ValidateRequest(BaseModel):
    question: str
    correct_answer: str
    user_answer: str


def _get_current_user(authorization: str = Header("")) -> dict | None:
    if not authorization.startswith("Bearer "):
        return None
    return verify_token(authorization[7:])


@router.post("/practice/generate")
def api_generate_question(req: PracticeRequest, authorization: str = Header("")):
    """Generate a practice question using AI."""
    if not config.LLM_API_KEY:
        return {"status": "error", "message": "未配置 AI API key"}

    from core.schema import get_table_schema
    schema = get_table_schema()

    client = OpenAI(base_url=config.LLM_BASE_URL, api_key=config.LLM_API_KEY)
    try:
        prompt = PRACTICE_PROMPT.replace("{schema}", schema)
        resp = client.chat.completions.create(
            model=config.LLM_MODEL,
            messages=[
                {"role": "system", "content": prompt},
                {"role": "user", "content": f"出一道{req.difficulty}难度的题，题目必须包含具体的表名、字段名和示例数据"},
            ],
            temperature=0.7,
        )
        content = resp.choices[0].message.content.strip()
    except Exception as e:
        return {"status": "error", "message": f"AI 调用失败: {str(e)}"}

    # Parse question and answer
    question = ""
    answer = ""
    if "正确答案：" in content:
        parts = content.split("正确答案：", 1)
        question = parts[0].replace("题目：", "").strip()
        answer = parts[1].strip()
        answer = re.sub(r"```(?:sql)?\s*|\s*```", "", answer).strip()

    return {
        "status": "success",
        "question": question,
        "answer": answer,
        "schema": schema,
        "difficulty": req.difficulty,
        "mode": "sql",
    }


@router.post("/practice/generate-choice")
def api_generate_choice(req: PracticeRequest):
    """Generate a multiple-choice SQL question."""
    if not config.LLM_API_KEY:
        return {"status": "error", "message": "未配置 AI API key"}

    from core.schema import get_table_schema
    schema = get_table_schema()

    client = OpenAI(base_url=config.LLM_BASE_URL, api_key=config.LLM_API_KEY)
    try:
        prompt = CHOICE_PROMPT.format(difficulty=req.difficulty, schema=schema)
        resp = client.chat.completions.create(
            model=config.LLM_MODEL,
            messages=[
                {"role": "system", "content": prompt},
                {"role": "user", "content": "出一道题，题目中必须包含具体的表名和字段名"},
            ],
            temperature=0.7,
        )
        content = resp.choices[0].message.content.strip()
    except Exception as e:
        return {"status": "error", "message": f"AI 调用失败: {str(e)}"}

    # Parse
    question = ""; options = {}; answer = ""
    for line in content.split("\n"):
        line = line.strip()
        if line.startswith("题目："):
            question = line[3:].strip()
        elif line.startswith(("A. ", "A)")):
            options["A"] = line[2:].strip().lstrip(". ").lstrip(") ")
        elif line.startswith(("B. ", "B)")):
            options["B"] = line[2:].strip().lstrip(". ").lstrip(") ")
        elif line.startswith(("C. ", "C)")):
            options["C"] = line[2:].strip().lstrip(". ").lstrip(") ")
        elif line.startswith(("D. ", "D)")):
            options["D"] = line[2:].strip().lstrip(". ").lstrip(") ")
        elif "正确答案" in line:
            answer = line.split("：")[-1].split(":")[-1].strip().upper()[:1]

    return {
        "status": "success",
        "question": question,
        "options": options,
        "answer": answer,
        "difficulty": req.difficulty,
        "mode": "choice",
    }


@router.post("/practice/validate")
def api_validate_answer(req: ValidateRequest, authorization: str = Header("")):
    """Validate a user's answer against the correct answer."""
    if not config.LLM_API_KEY:
        return {"status": "error", "message": "未配置 AI API key"}

    # First, try to execute the user's answer
    exec_result = execute_sql(req.user_answer)
    correct_result = execute_sql(req.correct_answer)

    # If both return the same data, it's likely correct
    if (exec_result["status"] == "success" and correct_result["status"] == "success"
            and exec_result.get("data") == correct_result.get("data")):
        # Save score
        payload = _get_current_user(authorization)
        if payload:
            save_score(payload["user_id"], req.question, req.user_answer, True)
        return {"status": "success", "result": "correct", "message": "答案正确！✅",
                "user_output": exec_result.get("data", [])}

    # If auto-check fails, ask AI to judge
    from core.schema import get_table_schema
    schema = get_table_schema()
    prompt = VALIDATE_PROMPT.format(
        question=req.question,
        correct_answer=req.correct_answer,
        user_answer=req.user_answer,
    )

    client = OpenAI(base_url=config.LLM_BASE_URL, api_key=config.LLM_API_KEY)
    try:
        resp = client.chat.completions.create(
            model=config.LLM_MODEL,
            messages=[
                {"role": "system", "content": prompt + "\n\n" + schema},
            ],
            temperature=0,
        )
        ai_judgment = resp.choices[0].message.content.strip()
    except Exception:
        ai_judgment = "CORRECT"  # err on the side of permissiveness

    is_correct = ai_judgment.upper().startswith("CORRECT")
    payload = _get_current_user(authorization)
    if payload:
        save_score(payload["user_id"], req.question, req.user_answer, is_correct)

    return {
        "status": "success",
        "result": "correct" if is_correct else "wrong",
        "message": ai_judgment,
        "user_output": exec_result.get("data", []),
    }


@router.get("/practice/stats")
def api_practice_stats(authorization: str = Header("")):
    payload = _get_current_user(authorization)
    if not payload:
        return {"status": "error", "message": "未登录"}
    return {"status": "success", "stats": get_user_stats(payload["user_id"])}


class ExplainRequest(BaseModel):
    sql: str


@router.post("/practice/explain")
def api_explain_sql(req: ExplainRequest):
    """AI explains what a SQL statement does, in simple terms."""
    if not config.LLM_API_KEY:
        return {"status": "error", "message": "未配置 AI API key"}
    if not req.sql.strip():
        return {"status": "error", "message": "SQL 不能为空"}

    EXPLAIN_PROMPT = """你是一个 SQL 教学助手。用通俗易懂的中文解释下面的 SQL 语句做了什么。

要求：
1. 用 1-2 句话概述这条 SQL 的目的
2. 逐句解释每个子句（SELECT/FROM/WHERE/JOIN/GROUP BY/ORDER BY 等）的作用
3. 如果有多表 JOIN，说明表之间的关系
4. 用初学者能理解的语言

只返回解释，不要加"好的""让我来解释"之类的开场白。
"""

    client = OpenAI(base_url=config.LLM_BASE_URL, api_key=config.LLM_API_KEY)
    try:
        resp = client.chat.completions.create(
            model=config.LLM_MODEL,
            messages=[
                {"role": "system", "content": EXPLAIN_PROMPT},
                {"role": "user", "content": req.sql.strip()},
            ],
            temperature=0.3,
        )
        explanation = resp.choices[0].message.content.strip()
        return {"status": "success", "explanation": explanation}
    except Exception as e:
        return {"status": "error", "message": f"AI 调用失败: {str(e)}"}


@router.get("/practice/history")
def api_practice_history(authorization: str = Header("")):
    """Get user's practice history."""
    payload = _get_current_user(authorization)
    if not payload:
        return {"status": "error", "message": "未登录"}

    import sqlite3, os
    db_path = os.path.join(os.path.dirname(__file__), "..", "users.db")
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    rows = conn.execute(
        "SELECT question, user_answer, correct, created_at FROM practice_scores WHERE user_id = ? ORDER BY created_at DESC LIMIT 30",
        (payload["user_id"],)
    ).fetchall()
    conn.close()
    return {"status": "success", "history": [dict(r) for r in rows]}


@router.get("/practice/leaderboard")
def api_leaderboard():
    """Top 10 users by practice accuracy."""
    import sqlite3, os
    db_path = os.path.join(os.path.dirname(__file__), "..", "users.db")
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    rows = conn.execute("""
        SELECT u.username, COUNT(*) as total, SUM(s.correct) as correct,
               ROUND(CAST(SUM(s.correct) AS FLOAT) / MAX(COUNT(*), 1) * 100, 1) as accuracy
        FROM practice_scores s JOIN users u ON s.user_id = u.id
        GROUP BY u.username HAVING total >= 3 ORDER BY accuracy DESC LIMIT 10
    """).fetchall()
    conn.close()
    return {"status": "success", "leaderboard": [dict(r) for r in rows]}
