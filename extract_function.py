import os
import re
import requests

# 1. Download the raw source file
RAW_URL = ("https://raw.githubusercontent.com/torvalds/linux/master/drivers/pinctrl/pinctrl-amd.c")
response = requests.get(RAW_URL)
if response.status_code != 200:
    raise RuntimeError(f"Failed to fetch file, status code: {response.status_code}")
source = response.text

# 2. Regex to extract functions with their full bodies
# This regex attempts to capture return type + name + params and the body correctly (balanced braces).
pattern = re.compile(
    r'^\s*(?P<prototype>(?:static\s+)?[a-zA-Z_][\w\s\*]+?\s+[a-zA-Z_]\w*\s*\([^)]*\))'  # signature
    r'\s*\{', re.MULTILINE)
# Now we'll parse manually to get full body with brace count.
functions = []
for match in pattern.finditer(source):
    start = match.start()
    sig = match.group('prototype').strip()
    name_match = re.search(r'([a-zA-Z_]\w*)\s*\(', sig)
    func_name = name_match.group(1) if name_match else f"func_{start}"
    # Now find the matching closing brace manually:
    brace_count = 0
    in_body = False
    end = start
    for i, ch in enumerate(source[start:], start=start):
        if ch == '{':
            brace_count += 1
            in_body = True
        elif ch == '}':
            brace_count -= 1
        if in_body and brace_count == 0:
            end = i + 1
            break
    snippet = source[start:end]
    functions.append((func_name, snippet))

# 3. Create output folder
output_dir = 'functions_extract'
os.makedirs(output_dir, exist_ok=True)

# 4. Write each function to its own file
for func_name, snippet in functions:
    safe_name = re.sub(r'\W+', '_', func_name)
    file_path = os.path.join(output_dir, f"{safe_name}.c")
    with open(file_path, 'w', encoding='utf-8') as f:
        f.write(snippet)

print(f"Extracted {len(functions)} functions into '{output_dir}' directory.")
