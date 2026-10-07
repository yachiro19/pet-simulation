#!/usr/bin/env python3
"""
Анализ результатов PET симуляции
Запуск: python analyse.py
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
from pathlib import Path

# ============================================================
# Загрузка данных
# ============================================================
csv_path = Path("pet_data.csv")
if not csv_path.exists():
    print("ОШИБКА: файл pet_data.csv не найден!")
    exit(1)

data = pd.read_csv(csv_path)
print(f"Загружено событий: {len(data)}")
print(f"Столбцы: {list(data.columns)}")
print(data.head(3))


# ============================================================
# Парсинг столбцов с разделителем ";"
# ============================================================
def parse_column(series, dtype=float):
    result = []
    for val in series.dropna():
        try:
            result.extend([dtype(x) for x in str(val).split(";") if x.strip()])
        except:
            pass
    return result


all_crystal_ids = parse_column(data["CrystalIDs"], dtype=int)
all_energies = parse_column(data["Energies_keV"], dtype=float)
all_times = parse_column(data["Times_ns"], dtype=float)

# ============================================================
# Совпадения (coincidences) — два кристалла в одном событии
# Для PET нужно два противоположных кристалла (|ID1 - ID2| ~ N/2)
# ============================================================
N_crystals = 24
coincidences = []
for _, row in data.iterrows():
    try:
        ids = list(
            set([int(x) for x in str(row["CrystalIDs"]).split(";") if x.strip()])
        )
        if len(ids) >= 2:
            for i in range(len(ids)):
                for j in range(i + 1, len(ids)):
                    diff = abs(ids[i] - ids[j])
                    opposite = min(diff, N_crystals - diff)
                    coincidences.append(
                        {
                            "id1": ids[i],
                            "id2": ids[j],
                            "opposite_diff": opposite,
                            "energy": row["TotalEnergy_keV"],
                        }
                    )
    except:
        pass

coin_df = pd.DataFrame(coincidences)
true_coincidences = (
    coin_df[coin_df["opposite_diff"] >= N_crystals // 2 - 2]
    if len(coin_df) > 0
    else pd.DataFrame()
)

# ============================================================
# Построение графиков
# ============================================================
fig = plt.figure(figsize=(16, 12))
fig.suptitle("PET Simulation — LYSO Ring, 24 crystals", fontsize=15, fontweight="bold")
gs = gridspec.GridSpec(3, 3, figure=fig, hspace=0.45, wspace=0.35)

# --- 1. Энергетический спектр (полная энергия события) ---
ax1 = fig.add_subplot(gs[0, 0])
bins = np.linspace(0, 600, 80)
ax1.hist(
    data["TotalEnergy_keV"],
    bins=bins,
    color="steelblue",
    alpha=0.8,
    edgecolor="k",
    linewidth=0.3,
)
ax1.axvline(511, color="red", linestyle="--", linewidth=1.5, label="511 кэВ")
ax1.set_xlabel("Энергия (кэВ)")
ax1.set_ylabel("Число событий")
ax1.set_title("Энергетический спектр")
ax1.legend(fontsize=8)

# --- 2. Пик 511 кэВ крупно ---
ax2 = fig.add_subplot(gs[0, 1])
mask_511 = (data["TotalEnergy_keV"] > 400) & (data["TotalEnergy_keV"] < 600)
ax2.hist(
    data["TotalEnergy_keV"][mask_511],
    bins=40,
    color="tomato",
    alpha=0.8,
    edgecolor="k",
    linewidth=0.3,
)
ax2.axvline(511, color="darkred", linestyle="--", linewidth=1.5, label="511 кэВ")
ax2.set_xlabel("Энергия (кэВ)")
ax2.set_ylabel("Число событий")
ax2.set_title("Пик 511 кэВ (400–600 кэВ)")
ax2.legend(fontsize=8)

# --- 3. Число хитов на событие ---
ax3 = fig.add_subplot(gs[0, 2])
ax3.hist(
    data["NumGammaHits"],
    bins=range(1, data["NumGammaHits"].max() + 2),
    color="mediumseagreen",
    alpha=0.8,
    edgecolor="k",
    linewidth=0.3,
    align="left",
)
ax3.set_xlabel("Число хитов")
ax3.set_ylabel("Число событий")
ax3.set_title("Мультиплетность гамма-хитов")

# --- 4. Активность кристаллов ---
ax4 = fig.add_subplot(gs[1, 0])
if all_crystal_ids:
    ax4.hist(
        all_crystal_ids,
        bins=range(0, N_crystals + 1),
        color="darkorange",
        alpha=0.8,
        edgecolor="k",
        linewidth=0.3,
        align="left",
    )
ax4.set_xlabel("ID кристалла")
ax4.set_ylabel("Число хитов")
ax4.set_title("Карта активности кристаллов")
ax4.set_xticks(range(0, N_crystals, 4))

# --- 5. Число оптических фотонов ---
ax5 = fig.add_subplot(gs[1, 1])
ax5.hist(
    data["PhotonCount"], bins=40, color="gold", alpha=0.8, edgecolor="k", linewidth=0.3
)
ax5.set_xlabel("Число фотонов")
ax5.set_ylabel("Число событий")
ax5.set_title("Оптические фотоны на событие")
mean_ph = data["PhotonCount"].mean()
ax5.axvline(
    mean_ph, color="red", linestyle="--", linewidth=1.5, label=f"Среднее: {mean_ph:.0f}"
)
ax5.legend(fontsize=8)

# --- 6. Фотоны vs Энергия (линейность детектора) ---
ax6 = fig.add_subplot(gs[1, 2])
ax6.scatter(
    data["TotalEnergy_keV"], data["PhotonCount"], alpha=0.4, s=8, color="purple"
)
# Линейный фит
mask_fit = data["TotalEnergy_keV"] > 10
if mask_fit.sum() > 5:
    z = np.polyfit(data["TotalEnergy_keV"][mask_fit], data["PhotonCount"][mask_fit], 1)
    xf = np.linspace(
        data["TotalEnergy_keV"][mask_fit].min(),
        data["TotalEnergy_keV"][mask_fit].max(),
        100,
    )
    ax6.plot(
        xf,
        np.polyval(z, xf),
        "r-",
        linewidth=1.5,
        label=f"Фит: {z[0]:.1f}·E + {z[1]:.0f}",
    )
ax6.set_xlabel("Энергия (кэВ)")
ax6.set_ylabel("Число фотонов")
ax6.set_title("Линейность: фотоны vs энергия")
ax6.legend(fontsize=8)

# --- 7. Временной спектр хитов ---
ax7 = fig.add_subplot(gs[2, 0])
if all_times:
    ax7.hist(
        all_times, bins=60, color="slateblue", alpha=0.8, edgecolor="k", linewidth=0.3
    )
ax7.set_xlabel("Время (нс)")
ax7.set_ylabel("Число хитов")
ax7.set_title("Временной спектр хитов")

# --- 8. Coincidences — разница ID кристаллов ---
ax8 = fig.add_subplot(gs[2, 1])
if len(coin_df) > 0:
    ax8.hist(
        coin_df["opposite_diff"],
        bins=range(0, N_crystals // 2 + 2),
        color="crimson",
        alpha=0.8,
        edgecolor="k",
        linewidth=0.3,
        align="left",
    )
    ax8.axvline(
        N_crystals // 2,
        color="black",
        linestyle="--",
        linewidth=1.5,
        label=f"Противоположные (diff={N_crystals//2})",
    )
    ax8.set_xlabel("|ID1 - ID2| (мин. до N/2)")
    ax8.set_ylabel("Число пар")
    ax8.set_title("Распределение совпадений")
    ax8.legend(fontsize=8)
else:
    ax8.text(
        0.5,
        0.5,
        "Нет совпадений\n(нужно ≥2 кристалла\nна событие)",
        ha="center",
        va="center",
        transform=ax8.transAxes,
        fontsize=10,
    )
    ax8.set_title("Совпадения (coincidences)")

# --- 9. Полярная карта активности ---
ax9 = fig.add_subplot(gs[2, 2], projection="polar")
if all_crystal_ids:
    counts = np.bincount(all_crystal_ids, minlength=N_crystals)
    angles = np.linspace(0, 2 * np.pi, N_crystals, endpoint=False)
    width = 2 * np.pi / N_crystals * 0.8
    bars = ax9.bar(
        angles,
        counts,
        width=width,
        alpha=0.8,
        color="darkorange",
        edgecolor="k",
        linewidth=0.3,
    )
ax9.set_title("Активность кристаллов\n(полярная)", pad=15)
ax9.set_xticks(np.linspace(0, 2 * np.pi, N_crystals, endpoint=False))
ax9.set_xticklabels(
    [str(i) if i % 6 == 0 else "" for i in range(N_crystals)], fontsize=7
)

plt.savefig("pet_results.png", dpi=150, bbox_inches="tight")
plt.show()

# ============================================================
# Статистика в терминал
# ============================================================
print("\n" + "=" * 55)
print("           PET SIMULATION — STATISTICS")
print("=" * 55)
print(f"  Всего событий:              {len(data)}")
print(f"  С энерговыделением > 0:     {len(data[data['TotalEnergy_keV'] > 0])}")
print(f"  Средняя энергия:            {data['TotalEnergy_keV'].mean():.1f} кэВ")
print(
    f"  Событий у пика 511 кэВ:     {len(data[data['TotalEnergy_keV'].between(480, 540)])}"
)
print(f"  Среднее число хитов:        {data['NumGammaHits'].mean():.1f}")
print(f"  Среднее число фотонов:      {data['PhotonCount'].mean():.0f}")
print(f"  Макс. фотонов в событии:    {data['PhotonCount'].max()}")
if len(true_coincidences) > 0:
    print(f"  Истинных совпадений (LOR):  {len(true_coincidences)}")
print("=" * 55)
