import os
import re
import faiss
from pathlib import Path
from dotenv import load_dotenv
from sentence_transformers import SentenceTransformer
from openai import OpenAI

# --------------------- Configuration ---------------------
BASE_DIR = Path("/home/amd/nithin/KunitGen/main_test_dir")  # ✅ Fixed missing quote
MODEL_NAME = "qwen/qwen3-coder-480b-a35b-instruct"
TEMPERATURE = 0.4
MAX_TOKENS = 8192
MAX_RETRIES = 3

VECTOR_INDEX = BASE_DIR / "code_index.faiss"
VECTOR_MAP = BASE_DIR / "file_map.txt"

# --------------------- Environment ------------------------
load_dotenv()
API_KEY = os.environ.get("NVIDIA_API_KEY")
if not API_KEY:
    raise ValueError("NVIDIA_API_KEY environment variable not set.")

client = OpenAI(base_url="https://integrate.api.nvidia.com/v1", api_key=API_KEY)

# --------------------- Embedding + Index Setup ---------------
def build_faiss_index():
    """Build FAISS index from available .c files if missing."""
    code_dir = BASE_DIR / "reference_testcases"
    files = list(code_dir.rglob("*.c"))
    if not files:
        print(f"⚠️ No .c files found under {code_dir}")
        return None, []
    print(f"📦 Building FAISS index from {len(files)} C source files...")
    model = SentenceTransformer("all-MiniLM-L6-v2")
    texts = [f.read_text(errors="ignore") for f in files]
    embeddings = model.encode(texts, show_progress_bar=True)
    dim = embeddings.shape[1]
    index = faiss.IndexFlatL2(dim)
    index.add(embeddings)
    faiss.write_index(index, str(VECTOR_INDEX))
    VECTOR_MAP.write_text("\n".join(str(p) for p in files))
    print(f"✅ Saved index: {VECTOR_INDEX}")
    print(f"✅ Saved map: {VECTOR_MAP}")
    return index, [str(p) for p in files]

# --------------------- Load or Build Index -------------------
print("🔍 Loading embedding model and FAISS index...")
embed_model = SentenceTransformer("all-MiniLM-L6-v2")

if not VECTOR_INDEX.exists() or not VECTOR_MAP.exists():
    index, file_map = build_faiss_index()
else:
    index = faiss.read_index(str(VECTOR_INDEX))
    file_map = VECTOR_MAP.read_text().splitlines()
    print(f"✅ Loaded FAISS index from {VECTOR_INDEX}")

# --------------------- Retrieval -----------------------------
def retrieve_context(query_text: str, top_k: int = 3):
    """Retrieve top-k relevant code snippets."""
    if index is None:
        return ["// Retrieval skipped (no FAISS index available)"]
    query_emb = embed_model.encode([query_text])
    distances, indices = index.search(query_emb, top_k)
    results = []
    for idx in indices[0]:
        if idx < len(file_map):
            p = Path(file_map[idx])
            if p.exists():
                text = p.read_text(errors="ignore")
                results.append(f"// From {p}\n{text[:1500]}")
    return results

# --------------------- Model Query -------------------------
def query_model(prompt: str):
    try:
        completion = client.chat.completions.create(
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

# --------------------- File Helpers ------------------------
def safe_read(p: Path, fallback="// Missing file"):
    return p.read_text(encoding="utf-8") if p.exists() else fallback

def load_context_files():
    print("📂 Loading reference KUnit tests...")
    ref_dir = BASE_DIR / "reference_testcases"
    return {
        "sample_code1": safe_read(ref_dir / "kunit_test1.c"),
        "sample_code2": safe_read(ref_dir / "kunit_test2.c"),
        "sample_code3": safe_read(ref_dir / "kunit_test3.c"),
    }

# --------------------- Test Generation ---------------------
def generate_test_for_function(func_file: Path):
    func_code = func_file.read_text()
    test_name = f"{func_file.stem}_kunit_test"
    out_dir = BASE_DIR / "generated_tests"
    out_dir.mkdir(parents=True, exist_ok=True)  # ✅ Ensure directory exists
    out_file = out_dir / f"{test_name}.c"

    ctx = load_context_files()
    retrieved = retrieve_context(func_code, top_k=3)
    retrieved_text = "\n\n".join(retrieved)

    for attempt in range(1, MAX_RETRIES + 1):
        print(f"\n🔹 Attempt {attempt}/{MAX_RETRIES} for {func_file.name}")
        prompt = f"""
You are a senior Linux kernel developer generating KUnit tests.

## Function to test
{func_code}

## Retrieved Similar Code
{retrieved_text}

## Reference Code
{ctx["sample_code2"]}

Rules:
- Include required headers correctly
- Use kunit_kzalloc for allocations
- Use KUNIT_EXPECT_* macros
- Do not mock target functions
- Output should be a compilable KUnit test C file
- Do not include explanations, only C code
"""
        generated = query_model(prompt)

        # ✅ Debug logs
        print(f"Generated content length: {len(generated)}")
        print(f"Output path: {out_file.resolve()}")

        if len(generated.strip()) == 0 or generated.startswith("// Error"):
            print(f"⚠️ Generation failed: {generated}")
            continue

        out_file.write_text(generated)
        print(f"✅ Generated test file: {out_file}")
        break

# --------------------- Main Entry ---------------------------
def main():
    print(f"--- 🚀 Starting RAG-based KUnit Test Generator in {BASE_DIR} ---")
    func_dir = BASE_DIR / "test_functions"
    files = list(func_dir.glob("*.c"))
    if not files:
        print(f"❌ No .c files in {func_dir}")
        return
    for f in files:
        generate_test_for_function(f)
    print("\n--- ✅ All functions processed. ---")

if __name__ == "__main__":
    main()
