"""AI Chat endpoint — natural language to SQL with conversational responses."""
import re
from fastapi import APIRouter
from pydantic import BaseModel
from openai import OpenAI
from core import config
from core.schema import build_system_prompt
from core.database import execute_sql

router = APIRouter()


class ChatRequest(BaseModel):
    message: str

CONVERSATION_PROMPT = """你是一个友好的数据库助手，帮助不懂 SQL 的用户查询数据库。

## 回复格式
你的回复分两部分，用 `---SQL---` 分隔：
第一部分：用中文向用户解释你理解了什么、打算怎么查（1-2句话即可）。
第二部分：只写一条纯 SQL 语句，不要加任何 markdown 格式。

示例回复：
我帮你查询所有员工的信息，包括他们的姓名、部门和工资。
---SQL---
SELECT * FROM emp;

## 规则
- 如果用户的描述不明确，先推测最可能的含义，生成 SQL 的同时简单说明你的推测。
- 如果用户问的是"能查什么""有什么数据""怎么用"，不要生成 SQL，直接友好地告诉用户当前有哪些表、每张表有什么字段。
- 绝对只用上面列出的真实表和列名。
"""

@router.post("/chat")
def api_chat(req: ChatRequest):
    message = req.message.strip()
    if not message:
        return {"status": "error", "message": "消息不能为空", "columns": [], "data": [], "execution_time_ms": 0}
    if not config.LLM_API_KEY:
        return {"status": "error", "message": "未配置 LLM_API_KEY 环境变量", "columns": [], "data": [], "execution_time_ms": 0}

    client = OpenAI(base_url=config.LLM_BASE_URL, api_key=config.LLM_API_KEY)
    schema = build_system_prompt(force_refresh=True)
    full_prompt = CONVERSATION_PROMPT + "\n\n" + schema

    messages = [
        {"role": "system", "content": full_prompt},
        {"role": "user", "content": message},
    ]

    # --- Generate + retry loop (up to 3 attempts) ---
    MAX_RETRIES = 3
    for attempt in range(MAX_RETRIES):
        try:
            resp = client.chat.completions.create(
                model=config.LLM_MODEL,
                messages=messages,
                temperature=0.3,
            )
            content = resp.choices[0].message.content.strip()
        except Exception as e:
            return {"status": "error", "message": f"AI 调用失败: {str(e)}", "columns": [], "data": [], "execution_time_ms": 0}

        # Parse response
        explanation = ""
        sql = ""
        if "---SQL---" in content:
            parts = content.split("---SQL---", 1)
            explanation = parts[0].strip()
            sql = parts[1].strip()
            sql = re.sub(r"^```(?:sql)?\s*|\s*```$", "", sql).strip()
        else:
            explanation = content

        if not sql:
            return {
                "status": "success",
                "columns": [], "data": [],
                "execution_time_ms": 0,
                "explanation": explanation,
            }

        if sql.upper().startswith("ERROR:"):
            return {"status": "error", "message": sql, "columns": [], "data": [], "execution_time_ms": 0}

        result = execute_sql(sql)

        if result["status"] == "success":
            result["generated_sql"] = sql
            result["explanation"] = explanation
            return result

        # SQL failed — feed error back to LLM for retry
        if attempt < MAX_RETRIES - 1:
            err_msg = result.get("message", "Unknown error")
            retry_hint = (
                f"\n\n⚠️ 上面这条 SQL 执行失败了，错误信息：{err_msg}\n"
                f"注意：这个数据库不支持 COUNT(DISTINCT ...)、子查询。"
                f"如果需要去重计数，请分两步：先用 SELECT DISTINCT 查出去重结果，"
                f"然后回答\"查询到 N 条不重复记录\"。"
                f"请重新生成能正确执行的 SQL。"
            )
            messages.append({"role": "assistant", "content": content})
            messages.append({"role": "user", "content": retry_hint})

    # All retries exhausted
    result["generated_sql"] = sql
    result["explanation"] = explanation + f"\n(已尝试 {MAX_RETRIES} 次，SQL 仍执行失败)"
    return result
