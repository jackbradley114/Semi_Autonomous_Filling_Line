import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from pathlib import Path


from sklearn.model_selection import train_test_split
from sklearn.preprocessing import PolynomialFeatures
from sklearn.linear_model import LinearRegression
from sklearn.pipeline import Pipeline
from sklearn.metrics import r2_score, mean_squared_error, mean_absolute_error


# function to limit significant_figures figures
def significant_figures(x, sigfigs=3):
    """Format a number to a given number of significant figures."""
    return f"{x:.{sigfigs}g}"


# test constants and parameters
SEED = 0
TEST_SIZE = 0.20

# Polynomial order selection
POLY_DEGREES = [2, 3, 4, 5, 6, 7, 8, 9, 10]

CSV_NAME = "dispense_data.csv"
independent_variables = ["mass_g", "temperature_K", "head_height_m"]
dependent_variable = "dispense_time_ms"

# graph n points and  figs for fixed variable values
SWEEP_POINTS = 250
SIGFIGS = 4


# Load dataset
script_dir = Path(__file__).parent
csv_path = script_dir / CSV_NAME
df = pd.read_csv(csv_path)

X = df[independent_variables].to_numpy()
y = df[dependent_variable].to_numpy()

X_train, X_test, y_train, y_test = train_test_split(
    X, y, test_size=TEST_SIZE, random_state=SEED
)

# quantiles for fixed values from training data for 1-d plots

quartiles = np.array([0.0, 0.25, 0.5, 0.75, 1.0])
levels = np.quantile(X_train, quartiles, axis=0)

# Indices
mass_index = independent_variables.index("mass_g")
temperature_index = independent_variables.index("temperature_K")
head_height_index = independent_variables.index("head_height_m")

# Baseline values of training dataset
baseline = np.median(X_train, axis=0)


# evaluation metrics for test-train comparisons and predictions for time (r2,rmse,mae)
def evaluation_model(model, X_test, y_test):
    y_prediction = model.predict(X_test)
    r2 = r2_score(y_test, y_prediction)
    rmse = np.sqrt(mean_squared_error(y_test, y_prediction))
    mae = mean_absolute_error(y_test, y_prediction)
    return y_prediction, r2, rmse, mae



# 1) Fit + evaluate all polynomial models
polynomial_results = []
for degree in POLY_DEGREES:
    poly_model = Pipeline([
        ("poly", PolynomialFeatures(degree=degree, include_bias=False)),
        ("linreg", LinearRegression())
    ])
    poly_model.fit(X_train, y_train)
    y_prediction, r2, rmse, mae = evaluation_model(poly_model, X_test, y_test)
    result = {
        "name": f"deg={degree}",
        "degree": degree,
        "model": poly_model,
        "r2": r2,
        "rmse": rmse,
        "mae": mae,
        "y_prediction": y_prediction
    }
    polynomial_results.append(result)

# Print metrics for all polynomial models
print("\nPolynomial model comparison (test set):")
print("Model                |    R^2    |   RMSE (ms)  |   MAE (ms)")
for r in polynomial_results:
    print(f"{r['name']:<20} | {r['r2']:>8.6f} | {r['rmse']:>12.4f} | {r['mae']:>10.4f}")


# 2) Decide best model
# priority metricchosen as lowest RMSE on test set

best_result = min(polynomial_results, key=lambda r: r["rmse"])

print("\nBest polynomial model selected:")
print(f"  Model  : {best_result['name']}")
print(f"  Degree : {best_result['degree']}")
print(f"  R^2    : {best_result['r2']:.6f}")
print(f"  RMSE   : {best_result['rmse']:.4f} ms")
print(f"  MAE    : {best_result['mae']:.4f} ms")

best_model = best_result["model"]
best_y_pred = best_result["y_prediction"]

# 3) Predicted versus actual plot
# Only for best model

plt.figure(figsize=(6, 6))

plt.scatter(y_test, best_y_pred, alpha=0.6)

min_val = min(y_test.min(), best_y_pred.min())
max_val = max(y_test.max(), best_y_pred.max())
plt.plot([min_val, max_val], [min_val, max_val])

plt.title(
    f"Polynomial Model: {best_result['name']}\n"
    f"R²={best_result['r2']:.4f}, RMSE={best_result['rmse']:.2f}, MAE={best_result['mae']:.2f}"
)
plt.xlabel("Actual (ms)")
plt.ylabel("Predicted (ms)")
plt.tight_layout()
plt.show()



# 4) 2D slice plots only for chosen model

Temperature_fixed = baseline[temperature_index]
h_fixed = baseline[head_height_index]
m_fixed = baseline[mass_index]

fig_slices = plt.figure(figsize=(14, 4.5))
fig_slices.suptitle(
    f"Best Model ({best_result['name']})",
    y=1.05
)

# Graph A: temperature fixed, vary mass, multiple head heights
axA = fig_slices.add_subplot(1, 3, 1)
mass_sweep = np.linspace(levels[0, mass_index], levels[-1, mass_index], SWEEP_POINTS)

for level_index in range(5):
    h_val = levels[level_index, head_height_index]

    X_grid = np.tile(baseline, (SWEEP_POINTS, 1))
    X_grid[:, mass_index] = mass_sweep
    X_grid[:, head_height_index] = h_val
    X_grid[:, temperature_index] = Temperature_fixed

    y_grid = best_model.predict(X_grid)
    axA.plot(mass_sweep, y_grid, label=f"h = {significant_figures(h_val, SIGFIGS)} m")

axA.set_title(f"Mass sweep\n(Temperature fixed at {significant_figures(Temperature_fixed, SIGFIGS)} K)")
axA.set_xlabel("mass (g)")
axA.set_ylabel("dispensetime (ms)")
axA.legend()

# Graph B: head height fixed, vary mass, multiple temperatures
axB = fig_slices.add_subplot(1, 3, 2)
mass_sweep = np.linspace(levels[0, mass_index], levels[-1, mass_index], SWEEP_POINTS)

for level_index in range(5):
    T_val = levels[level_index, temperature_index]

    X_grid = np.tile(baseline, (SWEEP_POINTS, 1))
    X_grid[:, mass_index] = mass_sweep
    X_grid[:, temperature_index] = T_val
    X_grid[:, head_height_index] = h_fixed

    y_grid = best_model.predict(X_grid)
    axB.plot(mass_sweep, y_grid, label=f"T = {significant_figures(T_val, SIGFIGS)} K")

axB.set_title(f"Mass sweep\n(head height fixed at {significant_figures(h_fixed, SIGFIGS)} m)")
axB.set_xlabel("mass (g)")
axB.set_ylabel("dispense time (ms)")
axB.legend()

# Graph C: mass fixed, vary temperature, multiple head heights
axC = fig_slices.add_subplot(1, 3, 3)
temp_sweep = np.linspace(levels[0, temperature_index], levels[-1, temperature_index], SWEEP_POINTS)

for level_index in range(5):
    h_val = levels[level_index, head_height_index]

    X_grid = np.tile(baseline, (SWEEP_POINTS, 1))
    X_grid[:, temperature_index] = temp_sweep
    X_grid[:, head_height_index] = h_val
    X_grid[:, mass_index] = m_fixed

    y_grid = best_model.predict(X_grid)
    axC.plot(temp_sweep, y_grid, label=f"h = {significant_figures(h_val, SIGFIGS)} m")

axC.set_title(f"Temperature sweep\n(mass fixed at {significant_figures(m_fixed, SIGFIGS)} g)")
axC.set_xlabel("temperature (K)")
axC.set_ylabel("dispense time (ms)")
axC.legend()

plt.tight_layout()
plt.show()



# 5) contour plots tri-figure

plt.rcParams.update({
    "font.size": 12,
    "axes.titlesize": 13,
    "axes.labelsize": 12,
    "legend.fontsize": 10
})

global_min = np.min(y)
global_max = np.max(y)
levels_contour = np.linspace(global_min, global_max, 50)

fig = plt.figure(figsize=(18, 5))
fig.suptitle(f"2D Contour Relationships — Best Model ({best_result['name']})", y=1.05)


# A) graph 1 (Temperature fixed at median)

ax1 = fig.add_subplot(1, 3, 1)

Temperature_fixed = baseline[temperature_index]

mass_values = np.linspace(levels[0, mass_index], levels[-1, mass_index], 80)
head_values = np.linspace(levels[0, head_height_index], levels[-1, head_height_index], 80)

M, H = np.meshgrid(mass_values, head_values)

X_grid = np.zeros((M.size, 3))
X_grid[:, mass_index] = M.ravel()
X_grid[:, head_height_index] = H.ravel()
X_grid[:, temperature_index] = Temperature_fixed

Z = best_model.predict(X_grid).reshape(M.shape)

contour1 = ax1.contourf(
    M, H, Z,
    levels=levels_contour,
    cmap="coolwarm",
    vmin=global_min,
    vmax=global_max
)

ax1.contour(M, H, Z, levels=30, colors='black', linewidths=0.5)

cbar1 = plt.colorbar(contour1, ax=ax1)
cbar1.set_label("dispense time (ms)")

mask = np.abs(X[:, temperature_index] - Temperature_fixed) < 5
ax1.scatter(
    X[mask, mass_index],
    X[mask, head_height_index],
    c=y[mask],
    cmap="berlin",
    vmin=global_min,
    vmax=global_max,
    alpha=0,
    s=10,
    edgecolors="none"
)

ax1.set_title(f"Temperature fixed at {significant_figures(Temperature_fixed, SIGFIGS)} K")
ax1.set_xlabel("mass (g)")
ax1.set_ylabel("head height (m)")


# B) graph 2 (Head height fixed at median)

ax2 = fig.add_subplot(1, 3, 2)

h_fixed = baseline[head_height_index]

mass_values = np.linspace(levels[0, mass_index], levels[-1, mass_index], 80)
temp_values = np.linspace(levels[0, temperature_index], levels[-1, temperature_index], 80)

M, T = np.meshgrid(mass_values, temp_values)

X_grid = np.zeros((M.size, 3))
X_grid[:, mass_index] = M.ravel()
X_grid[:, temperature_index] = T.ravel()
X_grid[:, head_height_index] = h_fixed

Z = best_model.predict(X_grid).reshape(M.shape)


contour2 = ax2.contourf(
    M, T, Z,
    levels=levels_contour,
    cmap="coolwarm",
    vmin=global_min,
    vmax=global_max
)

ax2.contour(M, T, Z, levels=30, colors='black', linewidths=0.5)

cbar2 = plt.colorbar(contour2, ax=ax2)
cbar2.set_label("dispense time (ms)")

mask = np.abs(X[:, head_height_index] - h_fixed) < 0.02
ax2.scatter(
    X[mask, mass_index],
    X[mask, temperature_index],
    c=y[mask],
    cmap="berlin",
    vmin=global_min,
    vmax=global_max,
    alpha=0,
    s=10,
    edgecolors="none"
)

ax2.set_title(f"head height fixed at {significant_figures(h_fixed, SIGFIGS)} m")
ax2.set_xlabel("mass (g)")
ax2.set_ylabel("temperature (K)")



# C) grpah 3 (mass fixed at median)

ax3 = fig.add_subplot(1, 3, 3)

m_fixed = baseline[mass_index]

temp_values = np.linspace(levels[0, temperature_index], levels[-1, temperature_index], 80)
head_values = np.linspace(levels[0, head_height_index], levels[-1, head_height_index], 80)

T, H = np.meshgrid(temp_values, head_values)

X_grid = np.zeros((T.size, 3))
X_grid[:, temperature_index] = T.ravel()
X_grid[:, head_height_index] = H.ravel()
X_grid[:, mass_index] = m_fixed

Z = best_model.predict(X_grid).reshape(T.shape)


contour3 = ax3.contourf(
    T, H, Z,
    levels=levels_contour,
    cmap="coolwarm",
    vmin=global_min,
    vmax=global_max
)

ax3.contour(T, H, Z, levels=30, colors='black', linewidths=0.5)

cbar3 = plt.colorbar(contour3, ax=ax3)
cbar3.set_label("dispense time (ms)")

mask = np.abs(X[:, mass_index] - m_fixed) < 10
ax3.scatter(
    X[mask, temperature_index],
    X[mask, head_height_index],
    c=y[mask],
    cmap="berlin",
    vmin=global_min,
    vmax=global_max,
    alpha=0,
    s=10,
    edgecolors="none"
)

ax3.set_title(f"mass fixed at {significant_figures(m_fixed, SIGFIGS)} g")
ax3.set_xlabel("temperature (K)")
ax3.set_ylabel("head height (m)")


plt.tight_layout()
plt.show()


# Print polynomial expression for best model

poly_step = best_model.named_steps["poly"]
linreg_step = best_model.named_steps["linreg"]

feature_names = poly_step.get_feature_names_out(independent_variables)
coefficients = linreg_step.coef_
intercept = linreg_step.intercept_

print("\nPolynomial expression for selected best model:")
print(f"Model order: {best_result['degree']}")
print(f"dispense_time_ms = {intercept:.1f}", end="")

for coef, term in zip(coefficients, feature_names):
    if abs(coef) < 1e-12:
        continue

    readable_term = term.replace(" ", " * ")

    if coef >= 0:
        print(f" + ({coef:.1f})*({readable_term})", end="")
    else:
        print(f" - ({abs(coef):.1f})*({readable_term})", end="")

print("\n")
