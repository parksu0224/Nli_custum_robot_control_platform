# Hugging Face API Migration Guide

## 1. Prepare a Token

Create a fine-grained token on Hugging Face and allow permission to call Inference Providers.

Create the following file inside the project.

```text
secrets/hf_token.txt
```

Put only one token line in the file.

```text
hf_xxxxxxxxxxxxxxxxx
```

## 2. Configure the Model and System Message

Change them in `python/config.py`.

```python
HF_MODEL = "Qwen/Qwen2.5-7B-Instruct:fastest"
SYSTEM_MESSAGE = """Put the full system message here"""
```

Append `:fastest` to automatically select a fast provider, or `:cheapest` to prioritize cost.

## 3. RAG Table

Default file:

```text
knowledge/module_knowledge.xlsx
```

Use one row as one unit of knowledge.

Main columns:

- `module_id`: Actual module identifier
- `function`: Allowed function name
- `parameters`: Allowed argument format
- `description`: Function description
- `conditions`: Execution conditions
- `safety`: Safety and prohibition conditions
- `keywords`: Search terms for user expressions
- `example`: Exact output example
- `always_include`: If TRUE, include in every request
- `enabled`: If FALSE, exclude from search

To recreate the table with the initial examples:

```bash
python python/create_knowledge_table.py
```

To use CSV, create a CSV with the same headers and modify `config.py`.

```python
RAG_SOURCE_FILES = (
    "module_knowledge.csv",
)
```

## 4. Execution Flow

```text
C++/STT input
→ LLM request queue
→ Search related XLSX/CSV rows
→ System message + RAG + recent conversation
→ Hugging Face Chat Completions API
→ Parse NLI code
→ Deliver ③ command to C++
→ ⑥ narration TTS
```

## 5. Main Files

- `python/hf_client.py`: API calls, retries, conversation memory, response processing
- `python/rag.py`: XLSX/CSV/TXT loading and related-row search
- `python/config.py`: Token, model, system message, and RAG settings
- `knowledge/module_knowledge.xlsx`: Robot knowledge table
