import numpy as np
import pandas as pd
from pathlib import Path


# Settings

N = 5000
SEED = None                 # Set to None for different data each run
K_TIME = 1000            # Scaling constant (ms)

# test ranges for independent variables
MASS_G_MIN, MASS_G_MAX = 50.0, 500.0        # in g
TEMP_K_MIN, TEMP_K_MAX = 253.0, 323.0       # K
HEAD_M_MIN, HEAD_M_MAX = 0.1, 0.4           # meters

# random noise multiplier range
NOISE_MEAN, NOISE_SD = 0, 128


# Generate Data
rng = np.random.default_rng(SEED)

mass_g = rng.uniform(MASS_G_MIN, MASS_G_MAX, N)
temp_k = rng.uniform(TEMP_K_MIN, TEMP_K_MAX, N)
head_m = rng.uniform(HEAD_M_MIN, HEAD_M_MAX, N)
noise = rng.normal(NOISE_MEAN, NOISE_SD, N)  # dont print to CSV

mass_kg = mass_g / 1000.0

# Time model (ms) with normal additie noise
time_ms = K_TIME * (mass_kg * mass_kg * mass_kg * head_m * head_m * temp_k) + noise

# dataframe creatiion
df = pd.DataFrame({
    "mass_g": mass_g,
    "temperature_K": temp_k,
    "head_height_m": head_m,
    "dispense_time_ms": time_ms
})

# formatting (round values)
df = df.round({
    "mass_g": 2,
    "temperature_K": 2,
    "head_height_m": 4,
    "dispense_time_ms": 2
})


# Save File to Script Folder
script_dir = Path(__file__).parent
file_path = script_dir / "synthetic_dispense_data.csv"

df.to_csv(file_path, index=False)

print(df.head())
print("\nDispense time summary (ms):")
print(df["dispense_time_ms"].describe())
