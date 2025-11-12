import os
import re
import faiss
import subprocess
from pathlib import Path
from dotenv import load_dotenv
from sentence_transformers import SentenceTransformer
from openai import OpenAI

# --------------------- Configuration ---------------------
BASE_DIR = Path("/home/amd/nithin/KunitGen/main_test_dir")
LINUX_DIR = Path("/home/amd/linux")
MODEL_NAME = "meta/llama-3.1-70b-instruct"   # Example: adjust for your API
TEMPERATURE = 0.4
MAX_RETRIES = 3
MAX_TOKENS = 8192
VECTOR_INDEX = Path("code_index.faiss")
VECTOR_MAP = Path("file_map.txt")

# --------------------- Environment ------------------------
load_dotenv()
API_KEY = os.environ.get("NVIDIA_API_KEY")
if not API_KEY:
    raise ValueError("NVIDIA_API_KEY environment variable not set.")

client = OpenAI(base_url="https://integrate.api.nvidia.com/v1", api_key=API_KEY)

# --------------------- Embedding + Retrieval ---------------
print("🔍 Loading embedding model and FAISS index...")
embed_model = SentenceTransformer("all-MiniLM-L6-v2")
index = faiss.read_index(str(VECTOR_INDEX))
file_map = VECTOR_MAP.read_text().splitlines()

def retrieve_context(query_text: str, top_k: int = 3):
    """Retrieve top-k relevant code snippets."""
    query_emb = embed_model.encode([query_text])
    distances, indices = index.search(query_emb, top_k)
    results = []
    for idx in indices[0]:
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
        return completion.choices[0].message.content.replace("```c", "").replace("```", "").strip()
    except Exception as e:
        return f"// Error: {e}"

# --------------------- File Helpers ------------------------
def safe_read(p: Path, fallback="// Missing file"):
    return p.read_text(encoding="utf-8") if p.exists() else fallback

def load_context_files():
    print("📂 Loading reference KUnit tests and logs...")
    ref_dir = BASE_DIR / "reference_testcases"
    log_file = BASE_DIR / "compilation_log" / "compile_error.txt"
    return {
        "sample_code1": safe_read(ref_dir / "kunit_test1.c"),
        "sample_code2": safe_read(ref_dir / "kunit_test2.c"),
        "sample_code3": safe_read(ref_dir / "kunit_test3.c"),
        "error_logs": safe_read(log_file, "// No previous error logs"),
    }

# --------------------- Kernel File Updates -----------------
def update_makefile(test_name: str):
    makefile_path = LINUX_DIR / "drivers/platform/x86/amd/pmf/Makefile"
    if not makefile_path.exists():
        print("⚠️ Makefile missing, skipping update.")
        return
    config_name = test_name.upper()
    entry = f"obj-$(CONFIG_{config_name}) += {test_name}.o"
    text = makefile_path.read_text()
    pattern = re.compile(r"^(obj-\$\(\s*CONFIG_[A-Z0-9_]+\s*\)\s*\+=\s*\w+\.o)", re.MULTILINE)
    updated_text = re.sub(pattern, r"# \1  # commented by KUnitGen", text)
    if updated_text != text:
        makefile_path.write_text(updated_text)
        print("📝 Commented previous Makefile entries.")
    if entry not in updated_text:
        with open(makefile_path, "a") as f:
            f.write("\n" + entry + "\n")
        print(f"🧩 Added to Makefile: {entry}")

def update_kconfig(test_name: str):
    kpath = LINUX_DIR / "drivers/platform/x86/amd/pmf/Kconfig"
    backup = kpath.with_suffix(".KunitGen_backup")
    if not kpath.exists():
        print(f"❌ Kconfig not found at {kpath}")
        return
    if not backup.exists():
        backup.write_text(kpath.read_text())
        print(f"📦 Backup created: {backup}")
    config_name = test_name.upper()
    entry = (
        f"\nconfig {config_name}\n"
        f'\tbool "KUnit test for {test_name}"\n'
        f"\tdepends on KUNIT\n"
        f"\tdefault n\n"
    )
    lines = kpath.read_text().splitlines(keepends=True)
    if any(f"config {config_name}" in l for l in lines):
        print(f"ℹ️ Kconfig entry for {config_name} exists.")
        return
    new_lines, inserted = [], False
    for line in lines:
        if ("endif" in line or "source" in line) and not inserted:
            new_lines.append(entry)
            inserted = True
        new_lines.append(line)
    kpath.write_text("".join(new_lines))
    print(f"✅ Added Kconfig entry for {config_name}")

def update_test_config(test_name: str):
    cfg = LINUX_DIR / "my_pmf.config"
    if not cfg.exists():
        print("⚠️ my_pmf.config missing.")
        return
    line = f"CONFIG_{test_name.upper()}=y"
    text = cfg.read_text()
    if line not in text:
        with open(cfg, "a") as f:
            f.write("\n" + line + "\n")
        print(f"🧩 Enabled {line} in my_pmf.config")

# --------------------- Compilation Check -------------------
def compile_and_check():
    print("⚙️ Running kernel build check...")
    kernel_dir = LINUX_DIR
    cmd = (
        f"cp {BASE_DIR}/generated_tests/*.c {LINUX_DIR}/drivers/gpio && "
        f"./tools/testing/kunit/kunit.py run --kunitconfig=my_gpio.config --arch=x86_64 --raw_output > "
        f"{BASE_DIR}/compilation_log/compile_error.txt 2>&1"
    )
    subprocess.run(cmd, shell=True, cwd=kernel_dir)
    log_file = BASE_DIR / "compilation_log" / "compile_error.txt"
    if not log_file.exists():
        print("❌ compile_error.txt not found")
        return False
    lines = log_file.read_text().splitlines()
    errors, seen = [], set()
    for i, line in enumerate(lines):
        if re.search(r"(error:|fatal error:)", line, re.IGNORECASE):
            msg = line.split("error:")[-1].strip() if "error:" in line else line.strip()
            if msg not in seen:
                seen.add(msg)
                errors.append(msg)
    clean_log = BASE_DIR / "compilation_log" / "clean_compile_errors.txt"
    clean_log.write_text("\n".join(errors) if errors else "No errors.")
    if errors:
        print(f"❌ Compilation failed with {len(errors)} unique errors.")
        return False
    print("✅ Compilation successful.")
    return True

# --------------------- Test Generation ---------------------
def generate_test_for_function(func_file: Path):
    func_code = func_file.read_text()
    test_name = f"{func_file.stem}_kunit_test"
    out_file = BASE_DIR / "generated_tests" / f"{test_name}.c"
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

## Previous Errors
{ctx["error_logs"]}

Rules:
- Include headers correctly
- Use kunit_kzalloc for allocations
- Avoid previous compile errors
- Include "gpio-amdpt.c" for each testcase
- Use KUNIT_EXPECT_* macros
- Do not mock target functions
Output: compilable C KUnit test file only
"""
        generated = query_model(prompt)
        out_file.write_text(generated)

        update_makefile(test_name)
        update_kconfig(test_name)
        update_test_config(test_name)

        if compile_and_check():
            print(f"✅ Successfully built {test_name} on attempt {attempt}")
            return
        else:
            err_file = BASE_DIR / "compilation_log" / "compile_error.txt"
            if err_file.exists():
                ctx["error_logs"] = err_file.read_text()
            print("🔁 Retrying with error feedback...")

    print(f"❌ Failed to compile {func_file.name} after {MAX_RETRIES} attempts.")

# --------------------- Main Entry ---------------------------
def main():
    print(f"--- 🚀 Starting RAG-based KUnit Generator in {BASE_DIR} ---")
    (BASE_DIR / "generated_tests").mkdir(parents=True, exist_ok=True)
    (BASE_DIR / "compilation_log").mkdir(parents=True, exist_ok=True)
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
