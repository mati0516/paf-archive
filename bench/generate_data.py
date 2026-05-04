import os
import random
import string

DATA_DIR = r"h:\paf-archive\bench\data"
FILE_COUNT = 200000
MIN_SIZE = 1024       # 1KB
MAX_SIZE = 16384      # 16KB

if not os.path.exists(DATA_DIR):
    os.makedirs(DATA_DIR)

print(f"Generating {FILE_COUNT} files in {DATA_DIR}...")

# Pre-generate some random data to reuse (faster than generating per file)
sample_data = "".join(random.choices(string.ascii_letters + string.digits, k=MAX_SIZE)).encode()

for i in range(FILE_COUNT):
    file_path = os.path.join(DATA_DIR, f"file_{i:06d}.bin")
    size = random.randint(MIN_SIZE, MAX_SIZE)
    # Using a slice of pre-generated data for speed
    with open(file_path, "wb") as f:
        f.write(sample_data[:size])
    
    if (i + 1) % 20000 == 0:
        print(f"  Progress: {i + 1}/{FILE_COUNT} files created.")

print("Data generation complete.")
