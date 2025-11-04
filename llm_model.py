import os
import subprocess
from pathlib import Path
import re
import dspy  # ✅ new
from dotenv import load_dotenv
from KunitGeneration.model_interface.prompts.unittest_kunit_prompts import kunit_generation_prompt


# ---------------- DSPy Signature ----------------
class KUnitSignature(dspy.Signature):
    """Generate a compilable KUnit test for given C source code and compile errors."""
    source_code: str = dspy.InputField(desc="C source file content")
    error_logs: str = dspy.InputField(desc="Compilation errors from previous attempt")
    kunit_test_code: str = dspy.OutputField(desc="Final KUnit test C source file")


# ---------------- DSPy-based Module ----------------
class DSPyKUnitModule:
    def __init__(self, model_name="meta/llama-3.1-405b-instruct", temperature=0.3):
        load_dotenv()
        self.api_key = os.environ.get("NVIDIA_API_KEY")
        if not self.api_key:
            raise ValueError("NVIDIA_API_KEY not set in environment.")
        
        # Initialize DSPy LLM backend
        dspy.settings.configure(
            lm=dspy.LM(
                "openai/gpt-4o-mini",  # or replace with your NVIDIA-integrated model
                api_key=self.api_key,
                temperature=temperature,
                max_tokens=8192,
            )
        )
        self.model = dspy.Predict(KUnitSignature)

    def generate(self, source_code: str, error_logs: str) -> str:
        """Generate KUnit test via DSPy declarative model."""
        response = self.model(source_code=source_code, error_logs=error_logs)
        return response.kunit_test_code.strip()


# ---------------- Main Test Generator ----------------
class KUnitTestGenerator:
    """
    Generate, compile, and validate KUnit tests for Linux kernel C functions.
    Uses DSPy for declarative LLM test generation.
    """

    def __init__(self, main_test_dir: Path, model_name: str, temperature: float, max_retries: int = 3):
        if not main_test_dir.is_dir():
            raise FileNotFoundError(f"❌ Test directory does not exist: {main_test_dir}")

        self.base_dir = main_test_dir
        self.functions_dir = self.base_dir / "test_functions"
        self.output_dir = self.base_dir / "generated_tests"
        self.sample_files = [
            self.base_dir / "reference_testcases" / "kunit_test1.c",
            self.base_dir / "reference_testcases" / "kunit_test2.c",
            self.base_dir / "reference_testcases" / "kunit_test3.c",
        ]
        self.error_log_file = self.base_dir / "compilation_log" / "clean_compile_errors.txt"

        self.model_name = model_name
        self.temperature = temperature
        self.max_retries = max_retries

        self.generator = DSPyKUnitModule(model_name, temperature)

    # ---------------- Context Loader ----------------
    def _load_context_files(self) -> dict:
        print("📖 Loading reference test cases and previous error logs...")
        context = {}
        def safe_read(p: Path, fallback="// Missing file"):
            return p.read_text(encoding="utf-8") if p.exists() else fallback

        context["sample_code1"] = safe_read(self.sample_files[0])
        context["sample_code2"] = safe_read(self.sample_files[1])
        context["sample_code3"] = safe_read(self.sample_files[2])
        context["error_logs"] = safe_read(self.error_log_file, "// No previous error logs")

        return context

    # ---------------- Kernel Integration ----------------
    def _update_makefile(self, test_name: str):
        makefile_path = Path("/home/amd/linux/drivers/gpio/Makefile")
        if not makefile_path.exists():
            print(f"⚠️  Makefile not found at '{makefile_path}' — skipping.")
            return

        config_name = test_name.upper()
        entry = f"obj-$(CONFIG_{config_name}) += {test_name}.o"
        text = makefile_path.read_text(encoding="utf-8")
        pattern = re.compile(r'^(obj-\$\(\s*CONFIG_[A-Z0-9_]+\s*\)\s*\+=\s*\w+\.o)', re.MULTILINE)

        updated_text = re.sub(pattern, r'# \1  # commented by KUnitGen', text)
        if updated_text != text:
            makefile_path.write_text(updated_text, encoding="utf-8")
            print("📝 Commented existing Makefile entries.")
        if entry not in updated_text:
            with open(makefile_path, "a", encoding="utf-8") as f:
                f.write("\n" + entry + "\n")
            print(f"🧩 Added Makefile entry: {entry}")

    def _update_kconfig(self, test_name: str):
        main_kconfig = Path("/home/amd/linux/drivers/gpio/Kconfig")
        backup_path = main_kconfig.with_suffix(".KunitGen_backup")
        config_name = test_name.upper()
        kconfig_entry = (
            f"\nconfig {config_name}\n"
            f'\tbool "KUnit test for {test_name}"\n'
            f"\tdepends on KUNIT\n"
            f"\tdefault n\n"
        )

        if not main_kconfig.exists():
            print(f"❌ Kconfig not found: {main_kconfig}")
            return False
        if not backup_path.exists():
            backup_path.write_text(main_kconfig.read_text(encoding="utf-8"))
            print(f"📦 Created Kconfig backup: {backup_path}")

        lines = main_kconfig.read_text(encoding="utf-8").splitlines(keepends=True)
        if any(f"config {config_name}" in line for line in lines):
            print(f"ℹ️ Entry already exists for {config_name}.")
            return True

        new_lines, inserted = [], False
        for line in lines:
            if (('source "drivers/pinctrl/actions/Kconfig"' in line) or ('endif' in line)) and not inserted:
                new_lines.append(kconfig_entry)
                inserted = True
            new_lines.append(line)

        main_kconfig.write_text("".join(new_lines), encoding="utf-8")
        print(f"✅ Added Kconfig entry for {config_name}")
        return True

    def _update_test_config(self, test_name: str):
        cfg_path = Path("/home/amd/linux/my_gpio.config")
        if not cfg_path.exists():
            print(f"⚠️  my_gpio.config not found — skipping.")
            return
        config_line = f"CONFIG_{test_name.upper()}=y"
        cfg_text = cfg_path.read_text(encoding="utf-8")
        if config_line not in cfg_text:
            with open(cfg_path, "a", encoding="utf-8") as f:
                f.write("\n" + config_line + "\n")
            print(f"🧩 Enabled {config_line} in my_gpio.config.")

    # ---------------- Compilation Check ----------------
    def _compile_and_check(self) -> bool:
        print("⚙️  Building kernel to validate test...")
        kernel_dir = Path("/home/amd/linux")
        cmd = (
            f"cp /home/amd/nithin/KunitGen/main_test_dir/generated_tests/*.c /home/amd/linux/drivers/gpio && "
            f"./tools/testing/kunit/kunit.py run --kunitconfig=my_gpio.config --arch=x86_64 --raw_output > "
            f"/home/amd/nithin/KunitGen/main_test_dir/compilation_log/compile_error.txt 2>&1"
        )
        subprocess.run(cmd, shell=True, cwd=kernel_dir)
        if not self.error_log_file.exists():
            print(f"❌ Log not found: {self.error_log_file}")
            return False

        log_lines = self.error_log_file.read_text(encoding="utf-8").splitlines()
        seen, error_blocks = set(), []
        i = 0
        while i < len(log_lines):
            line = log_lines[i]
            if re.search(r"(error:|fatal error:)", line, re.IGNORECASE):
                parts = line.split(':')
                clean_error = next(( ':'.join(parts[j:]).strip() for j, p in enumerate(parts) if 'error' in p.lower()), line.strip())
                if clean_error not in seen:
                    seen.add(clean_error)
                    code_line = log_lines[i+1].rstrip() if i + 1 < len(log_lines) and re.match(r"\s+\S", log_lines[i+1]) else ""
                    if code_line:
                        error_blocks.append(f"{clean_error}\n{code_line}")
                    else:
                        error_blocks.append(clean_error)
            i += 1

        extracted = "\n\n".join(error_blocks) or "No explicit error lines found."
        extracted_log = self.error_log_file.parent / "clean_compile_errors.txt"
        extracted_log.write_text(extracted, encoding="utf-8")

        if error_blocks:
            print(f"❌ Compilation failed. Found {len(error_blocks)} errors.")
            return False
        print("✅ Compilation successful.")
        return True

    # ---------------- Main Loop ----------------
    def generate_test_for_function(self, func_file_path: Path):
        func_code = func_file_path.read_text(encoding="utf-8")
        test_name = f"{func_file_path.stem}_kunit_test"
        out_file = self.output_dir / f"{test_name}.c"
        context = self._load_context_files()

        for attempt in range(1, self.max_retries + 1):
            print(f"\n🔹 Attempt {attempt}/{self.max_retries} — Generating test for {func_file_path.name}")
            generated_test = self.generator.generate(func_code, context["error_logs"])
            out_file.write_text(generated_test, encoding="utf-8")

            self._update_makefile(test_name)
            self._update_kconfig(test_name)
            self._update_test_config(test_name)

            if self._compile_and_check():
                print(f"✅ Test '{test_name}' compiled successfully.")
                return
            else:
                if self.error_log_file.exists():
                    context["error_logs"] = self.error_log_file.read_text(encoding="utf-8")
                print("🔁 Retrying with updated compile errors...")

        print(f"❌ Failed to produce compilable test for {func_file_path.name} after {self.max_retries} attempts.")

    def run(self):
        print(f"--- 🚀 Starting KUnit Test Generation in '{self.base_dir}' ---")
        self.output_dir.mkdir(parents=True, exist_ok=True)
        self.error_log_file.parent.mkdir(parents=True, exist_ok=True)

        func_files = list(self.functions_dir.glob("*.c"))
        if not func_files:
            print(f"❌ No C files found in '{self.functions_dir}'")
            return

        for func_file in func_files:
            self.generate_test_for_function(func_file)

        print("\n--- ✅ All tests processed successfully. ---")
