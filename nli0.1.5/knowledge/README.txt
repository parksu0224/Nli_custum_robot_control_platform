RAG Knowledge Table Guide

1. Default file: module_knowledge.xlsx
2. One row is one unit of knowledge. Write module functions, safety rules, and general descriptions in separate rows.
3. Main columns
   - module_id: Actual identifier such as ARM01 or WHEEL01
   - function: Exact function name that the LLM is allowed to output
   - parameters: Allowed argument format
   - description: Function description
   - conditions: Execution conditions
   - safety: Prohibition and stop conditions
   - keywords: English expressions the user is likely to say
   - example: Exact command example
   - always_include: If TRUE, always include in every request
   - enabled: If FALSE, exclude the row from RAG

4. Create a new table
   Move to the project root in the App Lab terminal or SSH, then run:
       python python/create_knowledge_table.py

5. To use CSV, create a CSV with the same headers and change RAG_SOURCE_FILES in config.py.
       RAG_SOURCE_FILES = ("module_knowledge.csv",)

6. If the same content is duplicated in XLSX and CSV and both are added to RAG_SOURCE_FILES, duplicate search results will occur. Specify only the file that is actually used.
