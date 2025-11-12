import os
import faiss
import subprocess
from pathlib import Path
from dotenv import load_dotenv
from openai import OpenAI
import numpy as np
import re

# --------------------- CONFIG ---------------------
BASE_DIR = Path(r"D:\AIprojects\llms\KunitGen\main_test_dir")
MODEL_NAME = "qwen/qwen3-coder-480b-a35b-instruc"
EMBED_MODEL = "text-embedding-3-small"
TEMPERATURE = 0.4
MAX_TOKENS = 8192
MAX_RETRIES = 3

VECTOR_INDEX = BASE_DIR / "code_index.faiss"
VECTOR_MAP = BASE_DIR / "file_map.txt"

load_dotenv()

NVIDIA_API_KEY = os.getenv("NVIDIA_API_KEY")
OPENAI_API_KEY = os.getenv("OPENAI_API_KEY")
#snithink-proj-kZVfqkvRRSWZ8fqn1tSwK_Am0aolO0jpvKgDfMn1gvOrHUB6vM7a2kRkgTmVkbENy8yLvzO5XET3BlbkFJI23KHMm6RRA8L3qj0icuiWLOj3pi4iEfJRlIMoKJRU8-JBOASYwH-f5y-gUmpmxzn2cjjFjbYA
if not NVIDIA_API_KEY or not OPENAI_API_KEY:
    raise ValueError("Both NVIDIA_API_KEY and OPENAI_API_KEY must be set in .env")

# OpenAI client for embeddings
openai_client = OpenAI(api_key=OPENAI_API_KEY)
# NVIDIA client for generation
nvidia_client = OpenAI(base_url="https://integrate.api.nvidia.com/v1", api_key=NVIDIA_API_KEY)

# --------------------- BUILD INDEX ---------------------
def build_faiss_index():
    """Create FAISS index from C and H files using OpenAI embeddings."""
    code_dir = BASE_DIR / "reference_testcases"
    files = list(code_dir.rglob("*.c")) + list(code_dir.rglob("*.h"))
    if not files:
        print("⚠️ No source files found for indexing.")
        return None, []

    print(f"📦 Building FAISS index from {len(files)} files using OpenAI embeddings...")
    texts = [f.read_text(errors="ignore") for f in files]
    embeddings = []

    for i, text in enumerate(texts):
        emb = (
            openai_client.embeddings.create(
                model=EMBED_MODEL, input=text[:8000]  # truncate to stay within limits
            )
            .data[0]
            .embedding
        )
        embeddings.append(emb)
        print(f"   → Embedded {files[i].name}")

    embeddings = np.array(embeddings).astype("float32")
    dim = embeddings.shape[1]
    index = faiss.IndexFlatL2(dim)
    index.add(embeddings)
    faiss.write_index(index, str(VECTOR_INDEX))
    VECTOR_MAP.write_text("\n".join(str(p) for p in files))
    print(f"✅ Saved index: {VECTOR_INDEX}")
    print(f"✅ Saved map: {VECTOR_MAP}")
    return index, [str(p) for p in files]

# --------------------- LOAD OR BUILD INDEX ---------------------
if not VECTOR_INDEX.exists() or not VECTOR_MAP.exists():
    index, file_map = build_faiss_index()
else:
    index = faiss.read_index(str(VECTOR_INDEX))
    file_map = VECTOR_MAP.read_text().splitlines()
    print(f"✅ Loaded FAISS index ({len(file_map)} files).")

# --------------------- RETRIEVAL ---------------------
def retrieve_context(query: str, top_k=3):
    """Retrieve top-k most relevant code snippets using OpenAI embeddings."""
    query_emb = (
        openai_client.embeddings.create(model=EMBED_MODEL, input=query[:8000])
        .data[0]
        .embedding
    )
    query_emb = np.array([query_emb]).astype("float32")
    distances, indices = index.search(query_emb, top_k)
    results = []
    for idx in indices[0]:
        if 0 <= idx < len(file_map):
            p = Path(file_map[idx])
            if p.exists():
                text = p.read_text(errors="ignore")
                results.append(f"// From {p}\n{text[:1500]}")
    return results

# --------------------- MODEL QUERY ---------------------
def query_model(prompt: str):
    """Query NVIDIA Llama model."""
    try:
        completion = nvidia_client.chat.completions.create(
            model=MODEL_NAME,
            messages=[{"role": "user", "content": prompt}],
            temperature=TEMPERATURE,
            max_tokens=MAX_TOKENS,
        )
        return (
            completion.choices[0]
            .message.content.replace("```c", "")
            .replace("```", "")
            .strip()
        )
    except Exception as e:
        return f"// Error: {e}"

# --------------------- GENERATION ---------------------
def generate_test_for_function(func_file: Path):
    func_code = func_file.read_text(errors="ignore")
    retrieved = retrieve_context(func_code, top_k=3)
    retrieved_text = "\n\n".join(retrieved)

    out_dir = BASE_DIR / "generated_tests"
    out_dir.mkdir(parents=True, exist_ok=True)
    out_file = out_dir / f"{func_file.stem}_kunit_test.c"

    print(f"\n🧠 Generating KUnit test for {func_file.name}...")

    for attempt in range(1, MAX_RETRIES + 1):
        prompt = f"""
You are a senior Linux kernel developer generating a KUnit test.

## Function to test
{func_code}

## Retrieved Reference Code
{retrieved_text}

Rules:
- Use correct headers
- Use kunit_kzalloc for memory
- Use KUNIT_EXPECT_* macros
- No mocks for target functions
- Output: only valid C code, no text explanations
"""
        generated = query_model(prompt)
        out_file.write_text(generated, encoding="utf-8")
        print(f"✅ Generated: {out_file}")
        break

# --------------------- MAKEFILE GENERATION ---------------------
def create_makefile():
    makefile_path = BASE_DIR / "generated_tests" / "Makefile"
    content = """
obj-m += kunit_generated_tests.o

kunit_generated_tests-objs := $(patsubst %.c,%.o,$(wildcard *_kunit_test.c))

all:
\tmake -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
\tmake -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
"""
    makefile_path.write_text(content.strip(), encoding="utf-8")
    print(f"🧩 Makefile created at {makefile_path}")

# --------------------- COMPILE TESTS ---------------------
def compile_tests():
    print("⚙️ Compiling generated tests using Makefile...")
    gen_dir = BASE_DIR / "generated_tests"
    result = subprocess.run(["make"], cwd=gen_dir, capture_output=True, text=True)
    if result.returncode == 0:
        print("✅ Compilation successful.")
    else:
        print("❌ Compilation failed:")
        print(result.stderr)

# --------------------- MAIN ---------------------
def main():
    print(f"--- 🚀 Starting KUnit Test Generator in {BASE_DIR} ---")
    func_dir = BASE_DIR / "test_functions"
    if not func_dir.exists():
        print(f"❌ Missing directory: {func_dir}")
        return
    files = list(func_dir.glob("*.c"))
    if not files:
        print(f"❌ No .c files found in {func_dir}")
        return

    for func_file in files:
        generate_test_for_function(func_file)

    create_makefile()
    compile_tests()

    print("\n--- ✅ All functions processed and compiled. ---")

if __name__ == "__main__":
    main()
