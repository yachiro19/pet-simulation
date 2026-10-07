#!/usr/bin/env python3
"""
Анализ режима tof: временное разрешение совпадений (CTR) и различимость
положений источника вдоль линии отклика.

Запуск:
    python analysis/tof/tof_analysis.py results/tof
    python analysis/tof/tof_analysis.py results/tof --out results/tof/analysis --ewin 425 650

Читает все tof_*.csv в каталоге (их пишет PET_simulation -m tof) и сохраняет
в каталог --out (по умолчанию <данные>/analysis):
    summary.md          — сводные таблицы для отчёта
    by_config.csv       — по конфигурациям (материал x форма)
    by_position.csv     — по конфигурациям и положениям источника
    dt_by_position.png  — распределения t1 - t2 для каждого положения
    mean_vs_z.png       — сдвиг пика t1 - t2 в зависимости от положения источника
    ctr_comparison.png  — CTR по материалам и формам
    cherenkov.png       — вклад черенковских и сцинтилляционных фотонов

Обозначения: модуль 0 — при z < 0 (время t1), модуль 1 — при z > 0 (время t2).
Источник при +z ближе к модулю 1, поэтому t1 - t2 = 2z/c ≈ +6.67 пс/мм.
"""

import argparse
import sys
from pathlib import Path

import matplotlib
import numpy as np
import pandas as pd

C_MM_PER_PS = 0.299792458              # скорость света, мм/пс
EXPECTED_SLOPE = 2.0 / C_MM_PER_PS     # ожидаемый сдвиг t1 - t2, пс/мм (≈ 6.67)
BOOTSTRAP = 100

# Оценки временной отметки кристалла (по столбцам CSV)
ESTIMATORS = {
    "first": "первый фотон",
    "mean:3": "среднее первых 3",
    "cher": "первый черенковский",
    "scint": "первый сцинтилляционный",
}
MAIN_ESTIMATOR = "first"

# Палитра: ink — нейтральные цвета текста и осей; SERIES — первые три слота
# категориальной палитры (проверены на различимость, в т.ч. для дальтоников);
# RAMP — порядковая синяя шкала для положений источника
INK, INK_2, GRID, SURFACE = "#0b0b0b", "#52514e", "#e4e3df", "#fcfcfb"
SERIES = ["#2a78d6", "#eb6834", "#1baf7a"]
RAMP = ["#86b6ef", "#6da7ec", "#5598e7", "#3987e5", "#2a78d6", "#256abf",
        "#1c5cab", "#184f95", "#104281", "#0d366b"]


# ============================================================
#  Загрузка
# ============================================================
def load(data_dir):
    files = sorted(data_dir.glob("tof_*.csv"))
    if not files:
        sys.exit(f"В {data_dir} нет файлов tof_*.csv")
    frames = []
    for f in files:
        d = pd.read_csv(f, dtype={"types": str})
        d["file"] = f.name
        frames.append(d)
    df = pd.concat(frames, ignore_index=True)
    df["config"] = config_labels(df)
    print(f"Загружено {len(files)} файлов, {len(df)} строк, конфигураций: {df.config.nunique()}")
    return df


def config_labels(df):
    """Подпись конфигурации: материал + форма (+ параметры, которые меняются в данных)."""
    def shape(row):
        if row["shape"] == "box":
            return "box"
        end = "SiPM" if row["narrow_end"] == "sipm" else "ист."
        return f"{row['shape']} {row['taper']:g} ({end})"

    label = df.material + " " + df.apply(shape, axis=1)
    varying = {"wrapping": "", "crystal_mm": " мм", "length_mm": " мм", "distance_mm": " мм",
               "sptr_ps": " пс SPTR", "pde_scale": "×PDE", "time_cut_ns": " нс",
               "pde_model": "", "source": ""}
    for col, unit in varying.items():
        if df[col].nunique() > 1:
            label = label + ", " + df[col].astype(str) + unit
    return label


# ============================================================
#  Отбор совпадений и временные отметки
# ============================================================
def timestamp(side, estimator):
    if estimator == "first":
        return side["t01_ps"]
    if estimator == "cher":
        return side["t_first_cher_ps"]
    if estimator == "scint":
        return side["t_first_scint_ps"]
    kind, k = estimator.split(":")
    cols = [f"t{i:02d}_ps" for i in range(1, int(k) + 1)]
    if kind == "kth":
        return side[cols[-1]]
    return side[cols].mean(axis=1, skipna=False)  # mean:K — нужны все K фотонов


def coincidences(df, edep_min):
    """События, где сработало ровно по одному кристаллу в каждом модуле.
    Сработавший кристалл — энерговыделение >= edep_min."""
    fired = df[df.edep_keV >= edep_min]
    counts = fired.groupby(["file", "event", "module"]).size().unstack(fill_value=0)
    for m in (0, 1):
        if m not in counts:
            counts[m] = 0
    good = counts[(counts[0] == 1) & (counts[1] == 1)].index
    fired = fired.set_index(["file", "event"])
    side1 = fired[fired.module == 0].loc[good]
    side2 = fired[fired.module == 1].loc[good]
    pairs = side1.join(side2, lsuffix="_1", rsuffix="_2")
    pairs = pairs.rename(columns={"config_1": "config", "src_z_mm_1": "src_z_mm",
                                  "material_1": "material", "shape_1": "shape"})
    return pairs.reset_index()


def delta_t(pairs, estimator):
    t1 = timestamp(pairs.filter(regex="_1$").rename(columns=lambda c: c[:-2]), estimator)
    t2 = timestamp(pairs.filter(regex="_2$").rename(columns=lambda c: c[:-2]), estimator)
    return (t1 - t2).to_numpy()


# ============================================================
#  FWHM, положение пика, бутстрэп
# ============================================================
def core_width(x, fraction=0.25):
    """Ширина кратчайшего интервала, содержащего долю fraction событий. В отличие от
    межквартильного размаха не зависит от хвостов: у гауссианы это ≈ 0.64σ (FWHM ≈ 3.7×)."""
    xs = np.sort(x)
    m = max(int(fraction * len(xs)), 5)
    return float(np.min(xs[m:] - xs[:-m]))


def density(x):
    """Ядерная оценка плотности (гауссово ядро) на мелкой сетке.
    Ширина ядра — по правилу Сильвермана, но с масштабом из ширины ядра
    распределения, а не из стандартного отклонения: хвосты BGO и PbF2 иначе
    раздули бы ядро шире самого пика."""
    sigma_core = core_width(x) / 0.6372  # гауссов эквивалент
    h = max(0.9 * sigma_core * len(x) ** (-0.2), 0.5)
    step = h / 4
    lo, hi = np.percentile(x, [0.5, 99.5])
    edges = np.arange(lo - 5 * h, hi + 5 * h + step, step)
    counts, edges = np.histogram(x, bins=edges)
    offsets = np.arange(-4 * h, 4 * h + step, step)
    kernel = np.exp(-0.5 * (offsets / h) ** 2)
    smooth = np.convolve(counts, kernel / kernel.sum(), mode="same")
    return 0.5 * (edges[:-1] + edges[1:]), smooth, h


def fwhm(x):
    """FWHM по ядерной оценке плотности с вычитанием ширины ядра в квадратуре
    (точно для гауссианы). Не предполагает гауссову форму распределения."""
    centers, smooth, h = density(x)
    i = int(np.argmax(smooth))
    half = smooth[i] / 2
    j = i
    while j > 0 and smooth[j] > half:
        j -= 1
    k = i
    while k < len(smooth) - 1 and smooth[k] > half:
        k += 1
    left = np.interp(half, [smooth[j], smooth[j + 1]], [centers[j], centers[j + 1]])
    right = np.interp(half, [smooth[k], smooth[k - 1]], [centers[k], centers[k - 1]])
    return np.sqrt(max((right - left) ** 2 - (2.3548 * h) ** 2, 0.))


def location(x, window):
    """Центр пика: среднее по окну ±window вокруг текущей оценки (итерации от медианы).
    Устойчиво к хвостам и при сдвиге распределения сдвигается вместе с ним."""
    mu = np.median(x)
    for _ in range(20):
        new = np.mean(x[np.abs(x - mu) < window])
        if abs(new - mu) < 1e-3:
            break
        mu = new
    return mu


def stats(x, rng):
    x = x[np.isfinite(x)]
    n = len(x)
    if n < 30:
        return dict(n=n, fwhm=np.nan, fwhm_err=np.nan, mu=np.nan, mu_err=np.nan, sigma=np.nan)
    width = fwhm(x)
    boot = [rng.choice(x, n) for _ in range(BOOTSTRAP)]
    return dict(n=n, fwhm=width, fwhm_err=np.std([fwhm(b) for b in boot]),
                mu=location(x, width), mu_err=np.std([location(b, width) for b in boot]),
                sigma=width / 2.3548)  # гауссов эквивалент


def linear_fit(z, mu, err):
    """Взвешенная прямая mu = slope*z + b; возвращает slope и его ошибку."""
    w = 1 / np.square(err)
    sw, swz, swzz = w.sum(), (w * z).sum(), (w * z * z).sum()
    swm, swzm = (w * mu).sum(), (w * z * mu).sum()
    det = sw * swzz - swz ** 2
    slope = (sw * swzm - swz * swm) / det
    intercept = (swzz * swm - swz * swzm) / det
    return slope, np.sqrt(sw / det), intercept


# ============================================================
#  Сводка
# ============================================================
def analyse(df, args):
    rng = np.random.default_rng(1)
    pairs_all = coincidences(df, args.edep_min)
    lo, hi = args.ewin
    peak = (pairs_all.edep_keV_1.between(lo, hi)) & (pairs_all.edep_keV_2.between(lo, hi))
    selections = {"1+1": pairs_all, f"1+1, фотопик {lo:g}–{hi:g} кэВ": pairs_all[peak]}

    by_pos, by_cfg = [], []
    for sel_name, pairs in selections.items():
        for config, cp in pairs.groupby("config"):
            for est in ESTIMATORS:
                dt = delta_t(cp, est)
                z = cp.src_z_mm.to_numpy()
                ok = np.isfinite(dt)

                rows = []
                for zval in np.unique(z):
                    s = stats(dt[(z == zval) & ok], rng)
                    rows.append(dict(config=config, selection=sel_name, estimator=est, z_mm=zval, **s))
                by_pos.extend(rows)

                # Все положения вместе после вычитания ожидаемого сдвига 2z/c
                pooled = stats((dt - EXPECTED_SLOPE * z)[ok], rng)
                pos = pd.DataFrame(rows).dropna(subset=["mu"])
                slope = slope_err = np.nan
                if len(pos) >= 2:
                    slope, slope_err, _ = linear_fit(pos.z_mm.to_numpy(), pos.mu.to_numpy(),
                                                     pos.mu_err.to_numpy())
                sigma = pooled["sigma"]
                by_cfg.append(dict(
                    config=config, selection=sel_name, estimator=est,
                    material=cp.material.iloc[0], shape_label=config.split(" ", 1)[1],
                    n_pairs=len(cp), n=pooled["n"], usable=pooled["n"] / max(len(cp), 1),
                    ctr_fwhm=pooled["fwhm"], ctr_fwhm_err=pooled["fwhm_err"], sigma=sigma,
                    # пространственное разрешение вдоль линии отклика по одному событию
                    dx_mm=C_MM_PER_PS * pooled["fwhm"] / 2,
                    slope=slope, slope_err=slope_err,
                    # различимость соседних положений (1 мм) по одному событию
                    d_prime=EXPECTED_SLOPE / sigma if sigma else np.nan,
                    # событий на каждое положение, чтобы различить 1 мм на уровне 3σ по средним
                    n_for_3sigma=2 * (3 * sigma / EXPECTED_SLOPE) ** 2 if sigma else np.nan,
                    frac_first_cher=np.mean(np.concatenate([
                        cp.types_1.fillna("").str[:1] == "C", cp.types_2.fillna("").str[:1] == "C"])),
                    n_cher=np.mean(np.concatenate([cp.n_cher_1, cp.n_cher_2])),
                    n_scint=np.mean(np.concatenate([cp.n_scint_1, cp.n_scint_2])),
                ))
    return pd.DataFrame(by_cfg), pd.DataFrame(by_pos), selections


# ============================================================
#  Графики
# ============================================================
def style():
    import matplotlib.pyplot as plt
    plt.rcParams.update({
        "figure.facecolor": SURFACE, "axes.facecolor": SURFACE, "savefig.facecolor": SURFACE,
        "axes.edgecolor": INK_2, "axes.labelcolor": INK, "text.color": INK,
        "xtick.color": INK_2, "ytick.color": INK_2, "axes.grid": True,
        "grid.color": GRID, "grid.linewidth": 0.8, "axes.axisbelow": True,
        "axes.spines.top": False, "axes.spines.right": False, "font.size": 9,
        "axes.titlesize": 10, "legend.frameon": False,
    })
    return plt


def grid_axes(plt, n, width=4.2, height=3.0):
    cols = min(n, 3)
    rows = int(np.ceil(n / cols))
    fig, axes = plt.subplots(rows, cols, figsize=(width * cols, height * rows), squeeze=False)
    for ax in axes.flat[n:]:
        ax.set_visible(False)
    return fig, axes.flat


def ramp(n):
    idx = np.linspace(0, len(RAMP) - 1, n).round().astype(int)
    return [RAMP[i] for i in idx]


def plot_dt_by_position(plt, pairs, out, title):
    configs = sorted(pairs.config.unique())
    fig, axes = grid_axes(plt, len(configs))
    for ax, config in zip(axes, configs):
        cp = pairs[pairs.config == config]
        dt = delta_t(cp, MAIN_ESTIMATOR)
        z = cp.src_z_mm.to_numpy()
        ok = np.isfinite(dt)
        if ok.sum() < 30:
            ax.set_title(f"{config}: мало событий")
            continue
        center = np.median(dt[ok])
        width = fwhm(dt[ok])
        zs = np.unique(z)
        if all(((z == zval) & ok).sum() < 30 for zval in zs):
            ax.text(0.5, 0.5, "меньше 30 совпадений\nна положение", transform=ax.transAxes,
                    ha="center", va="center", color=INK_2)
        for color, zval in zip(ramp(len(zs)), zs):
            sel = (z == zval) & ok
            if sel.sum() < 30:
                continue
            # гладкая оценка плотности вместо гистограмм: 7 наложенных гистограмм нечитаемы
            x, y, _ = density(dt[sel])
            ax.plot(x, y / (y.sum() * (x[1] - x[0])), color=color, linewidth=2, label=f"{zval:+g} мм")
        ax.set_xlim(center - 2.5 * width, center + 2.5 * width)
        ax.set_title(config)
        ax.set_xlabel("t1 − t2, пс")
        ax.set_ylabel("плотность, 1/пс")
    axes[0].legend(title="z источника", fontsize=7, title_fontsize=7)
    fig.suptitle(f"Распределения t1 − t2 по положениям источника ({title}, оценка: первый фотон)")
    fig.tight_layout()
    fig.savefig(out / "dt_by_position.png", dpi=150)


def plot_mean_vs_z(plt, by_pos, by_cfg, out, selection):
    pos = by_pos[(by_pos.selection == selection) & (by_pos.estimator == MAIN_ESTIMATOR)]
    configs = sorted(pos.config.unique())
    fig, axes = grid_axes(plt, len(configs))
    for ax, config in zip(axes, configs):
        p = pos[pos.config == config].dropna(subset=["mu"])
        if p.empty:
            continue
        ax.errorbar(p.z_mm, p.mu, yerr=p.mu_err, fmt="o", color=SERIES[0], markersize=5,
                    capsize=3, linewidth=1.5, label="центр пика ± ошибка")
        zz = np.linspace(p.z_mm.min() - 0.3, p.z_mm.max() + 0.3, 2)
        mean_offset = np.mean(p.mu - EXPECTED_SLOPE * p.z_mm)
        ax.plot(zz, EXPECTED_SLOPE * zz + mean_offset, color=INK_2, linestyle="--", linewidth=1,
                label=f"ожидаемо {EXPECTED_SLOPE:.2f} пс/мм")
        row = by_cfg[(by_cfg.config == config) & (by_cfg.selection == selection) &
                     (by_cfg.estimator == MAIN_ESTIMATOR)].iloc[0]
        ax.set_title(f"{config}\nнаклон {row.slope:.2f} ± {row.slope_err:.2f} пс/мм")
        ax.set_xlabel("z источника, мм")
        ax.set_ylabel("центр t1 − t2, пс")
    axes[0].legend(fontsize=7)
    fig.suptitle(f"Сдвиг пика t1 − t2 при смещении источника ({selection})")
    fig.tight_layout()
    fig.savefig(out / "mean_vs_z.png", dpi=150)


def grouped_bars(ax, table, groups, series):
    width = 0.8 / len(series)
    x = np.arange(len(groups))
    for i, (name, color) in enumerate(zip(series, SERIES)):
        vals = [table.get((g, name), (np.nan, np.nan)) for g in groups]
        h = np.array([v[0] for v in vals], dtype=float)
        e = np.array([v[1] for v in vals], dtype=float)
        xs = x - 0.4 + width * (i + 0.5)
        ax.bar(xs, h, width * 0.92, yerr=e, color=color, label=name, capsize=2,
               error_kw=dict(ecolor=INK_2, linewidth=1))
        for xi, hi in zip(xs, h):
            if np.isfinite(hi):
                ax.annotate(f"{hi:.0f}", (xi, hi), xytext=(0, 6), textcoords="offset points",
                            ha="center", fontsize=7, color=INK_2)
    ax.set_xticks(x, groups)


def plot_ctr_comparison(plt, by_cfg, out):
    cfg = by_cfg[by_cfg.estimator == MAIN_ESTIMATOR]
    materials = list(dict.fromkeys(cfg.sort_values("material").material))
    shapes = list(dict.fromkeys(cfg.shape_label))[:3]  # не больше трёх цветов
    selections = list(dict.fromkeys(cfg.selection))
    fig, axes = plt.subplots(1, len(selections), figsize=(5.5 * len(selections), 3.8), sharey=True)
    for ax, sel in zip(np.atleast_1d(axes), selections):
        s = cfg[cfg.selection == sel]
        table = {(r.material, r.shape_label): (r.ctr_fwhm, r.ctr_fwhm_err) for r in s.itertuples()}
        grouped_bars(ax, table, materials, shapes)
        ax.set_title(sel)
        ax.set_ylabel("CTR (FWHM), пс")
    np.atleast_1d(axes)[0].legend(title="форма", fontsize=7, title_fontsize=7)
    fig.suptitle("Временное разрешение совпадений по материалам и формам (оценка: первый фотон)")
    fig.tight_layout()
    fig.savefig(out / "ctr_comparison.png", dpi=150)


def plot_cherenkov(plt, by_cfg, out, selection):
    cfg = by_cfg[by_cfg.selection == selection]
    configs = sorted(cfg.config.unique())
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 3.8), gridspec_kw={"width_ratios": [2, 1]})
    names = ["first", "cher", "scint"]
    table = {(r.config, ESTIMATORS[r.estimator]): (r.ctr_fwhm, r.ctr_fwhm_err)
             for r in cfg.itertuples() if r.estimator in names}
    grouped_bars(ax1, table, configs, [ESTIMATORS[n] for n in names])
    ax1.set_ylabel("CTR (FWHM), пс")
    ax1.set_title("CTR по типу фотона, задающего временную отметку", pad=24)
    ax1.tick_params(axis="x", labelrotation=20)
    ax1.legend(fontsize=7, ncols=3, loc="lower center", bbox_to_anchor=(0.5, 1.0))

    first = cfg[cfg.estimator == MAIN_ESTIMATOR].set_index("config").loc[configs]
    ax2.barh(configs, 100 * first.frac_first_cher, color=SERIES[0], height=0.6)
    for y, v in enumerate(100 * first.frac_first_cher):
        ax2.annotate(f"{v:.0f} %", (v, y), xytext=(4, 0), textcoords="offset points",
                     va="center", fontsize=7, color=INK_2)
    ax2.set_xlim(0, 110)
    ax2.set_xlabel("%")
    ax2.set_title("Первый фотон — черенковский", pad=24)
    ax2.invert_yaxis()
    fig.suptitle(f"Вклад черенковского излучения ({selection})")
    fig.tight_layout()
    fig.savefig(out / "cherenkov.png", dpi=150)


# ============================================================
#  Markdown-отчёт
# ============================================================
def fmt(v, err=None, digits=1):
    if v is None or not np.isfinite(v):
        return "—"
    return f"{v:.{digits}f} ± {err:.{digits}f}" if err is not None and np.isfinite(err) else f"{v:.{digits}f}"


def write_markdown(by_cfg, by_pos, out, args):
    lines = ["# Анализ CTR (режим tof)", "",
             f"Совпадение: ровно один кристалл с энерговыделением ≥ {args.edep_min:g} кэВ в каждом модуле. "
             f"Ожидаемый сдвиг t1 − t2 при смещении источника на 1 мм: {EXPECTED_SLOPE:.2f} пс.", "",
             "- **CTR** — FWHM распределения t1 − t2 по всем положениям после вычитания ожидаемого сдвига 2z/c.",
             "- **Δx = c·CTR/2** — пространственное разрешение вдоль линии отклика по одному событию.",
             "- **d′ = 6.67 пс / σ** — разделение соседних положений (1 мм) в единицах σ одного события.",
             "- **N₃σ** — событий на положение, чтобы различить сдвиг 1 мм по средним на уровне 3σ.", ""]
    for sel in by_cfg.selection.unique():
        s = by_cfg[(by_cfg.selection == sel) & (by_cfg.estimator == MAIN_ESTIMATOR)].sort_values("ctr_fwhm")
        lines += [f"## {sel}: первый фотон", "",
                  "| Конфигурация | Совпадений | CTR FWHM, пс | σ, пс | Δx, мм | Наклон, пс/мм | d′ | N₃σ | Первый — Черенков | n_cher | n_scint |",
                  "|---|---|---|---|---|---|---|---|---|---|---|"]
        for r in s.itertuples():
            lines.append(f"| {r.config} | {r.n} | {fmt(r.ctr_fwhm, r.ctr_fwhm_err)} | {fmt(r.sigma)} | "
                         f"{fmt(r.dx_mm, digits=2)} | {fmt(r.slope, r.slope_err, 2)} | {fmt(r.d_prime, digits=2)} | "
                         f"{fmt(r.n_for_3sigma, digits=0)} | {100 * r.frac_first_cher:.0f} % | "
                         f"{r.n_cher:.2f} | {r.n_scint:.0f} |")
        lines.append("")

        lines += [f"### {sel}: CTR FWHM (пс) для разных оценок временной отметки", "",
                  "Число в скобках — доля совпадений, где оценка доступна (например, есть черенковский фотон на обеих сторонах).", "",
                  "| Конфигурация | " + " | ".join(ESTIMATORS.values()) + " |",
                  "|---|" + "---|" * len(ESTIMATORS)]
        e = by_cfg[by_cfg.selection == sel]
        for config in sorted(e.config.unique()):
            cells = []
            for est in ESTIMATORS:
                r = e[(e.config == config) & (e.estimator == est)].iloc[0]
                cells.append(f"{fmt(r.ctr_fwhm, r.ctr_fwhm_err)} ({100 * r.usable:.0f} %)")
            lines.append(f"| {config} | " + " | ".join(cells) + " |")
        lines.append("")

        lines += [f"### {sel}: по положениям источника (первый фотон)", "",
                  "| Конфигурация | z, мм | N | CTR FWHM, пс | Центр t1 − t2, пс |", "|---|---|---|---|---|"]
        p = by_pos[(by_pos.selection == sel) & (by_pos.estimator == MAIN_ESTIMATOR)]
        for r in p.sort_values(["config", "z_mm"]).itertuples():
            lines.append(f"| {r.config} | {r.z_mm:+g} | {r.n} | {fmt(r.fwhm, r.fwhm_err)} | {fmt(r.mu, r.mu_err)} |")
        lines.append("")
    (out / "summary.md").write_text("\n".join(lines), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("data", type=Path, help="каталог с tof_*.csv")
    parser.add_argument("--out", type=Path, help="каталог для результатов (по умолчанию <data>/analysis)")
    parser.add_argument("--edep-min", type=float, default=10.0,
                        help="порог энерговыделения сработавшего кристалла, кэВ")
    parser.add_argument("--ewin", type=float, nargs=2, default=[425.0, 650.0], metavar=("LO", "HI"),
                        help="фотопиковое окно по энерговыделению, кэВ")
    parser.add_argument("--show", action="store_true", help="показать графики на экране")
    args = parser.parse_args()

    if not args.show:
        matplotlib.use("Agg")
    out = args.out or args.data / "analysis"
    out.mkdir(parents=True, exist_ok=True)

    df = load(args.data)
    by_cfg, by_pos, selections = analyse(df, args)
    by_cfg.to_csv(out / "by_config.csv", index=False)
    by_pos.to_csv(out / "by_position.csv", index=False)
    write_markdown(by_cfg, by_pos, out, args)

    plt = style()
    peak_sel = list(selections)[1]
    plot_dt_by_position(plt, selections[peak_sel], out, peak_sel)
    plot_mean_vs_z(plt, by_pos, by_cfg, out, peak_sel)
    plot_ctr_comparison(plt, by_cfg, out)
    plot_cherenkov(plt, by_cfg, out, list(selections)[0])

    main_rows = by_cfg[(by_cfg.selection == peak_sel) & (by_cfg.estimator == MAIN_ESTIMATOR)]
    print(f"\nCTR ({peak_sel}, первый фотон):")
    for r in main_rows.sort_values("ctr_fwhm").itertuples():
        print(f"  {r.config:28s} {fmt(r.ctr_fwhm, r.ctr_fwhm_err):>16s} пс   N={r.n:5d}   "
              f"наклон {fmt(r.slope, r.slope_err, 2)} пс/мм   d'={fmt(r.d_prime, digits=2)}")
    print(f"\nРезультаты: {out}")
    if args.show:
        plt.show()


if __name__ == "__main__":
    main()
