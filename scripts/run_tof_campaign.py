#!/usr/bin/env python3
"""
Параллельный запуск серии прогонов режима tof: материал x форма, для каждой
конфигурации — скан положения источника (-3..+3 мм). Каждая конфигурация
считается отдельным процессом PET_simulation, поэтому многоядерный процессор
используется полностью (Geant4 в программе работает в однопоточном режиме).

Примеры:
    python scripts/run_tof_campaign.py
    python scripts/run_tof_campaign.py --materials LYSO BGO --shapes box trd:0.5:source --nev 5000
    python scripts/run_tof_campaign.py --jobs 4 --out results/tof_run1

Форма задаётся как name[:taper[:narrowEnd]], например trd:0.7:sipm.
"""

import argparse
import os
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent


def default_exe():
    for candidate in (REPO / "build" / "Release" / "PET_simulation.exe",
                      REPO / "build" / "PET_simulation"):
        if candidate.exists():
            return candidate
    return REPO / "build" / "Release" / "PET_simulation.exe"


def shape_commands(spec):
    """'trd:0.5:source' -> команды макроса и короткое имя для логов."""
    parts = spec.split(":")
    commands = [f"/tof/det/shape {parts[0]}"]
    if len(parts) > 1:
        commands.append(f"/tof/det/taper {parts[1]}")
    if len(parts) > 2:
        commands.append(f"/tof/det/narrowEnd {parts[2]}")
    return commands, spec.replace(":", "_")


def path_for_geant4(path, exe_dir):
    """Путь относительно папки программы: Geant4 открывает файлы через узкие
    строки, и кириллица в абсолютном пути (C:/Users/Андрей/...) может не пройти."""
    try:
        return Path(os.path.relpath(path, exe_dir)).as_posix()
    except ValueError:  # другой диск в Windows
        return path.as_posix()


def run_config(exe, material, shape_spec, args, out_dir):
    shape_cmds, shape_name = shape_commands(shape_spec)
    name = f"{material}_{shape_name}"
    macro = out_dir / "macros" / f"{name}.mac"
    log = out_dir / "logs" / f"{name}.log"

    lines = ["/control/execute tof/config_default.mac",
             f"/tof/output/dir {path_for_geant4(out_dir, exe.parent)}",
             f"/tof/det/material {material}",
             *shape_cmds,
             f"/control/alias nev {args.nev}",
             *args.extra,
             "/run/initialize",
             f"/control/loop tof/point.mac zpos {args.zmin} {args.zmax} {args.zstep}"]
    macro.write_text("\n".join(lines) + "\n", encoding="ascii")

    start = time.time()
    with open(log, "w", encoding="utf-8", errors="replace") as f:
        # Рабочая папка — папка программы: там лежат макросы tof/*.mac
        result = subprocess.run([str(exe), "-m", "tof", path_for_geant4(macro, exe.parent)], cwd=exe.parent,
                                stdout=f, stderr=subprocess.STDOUT)
    return name, result.returncode, time.time() - start, log


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--exe", type=Path, default=default_exe(), help="путь к PET_simulation")
    parser.add_argument("--materials", nargs="+", default=["LYSO", "BGO", "PbF2"])
    parser.add_argument("--shapes", nargs="+", default=["box", "trd"])
    parser.add_argument("--nev", type=int, default=20000, help="событий на одну точку скана")
    parser.add_argument("--zmin", type=float, default=-3)
    parser.add_argument("--zmax", type=float, default=3)
    parser.add_argument("--zstep", type=float, default=1)
    parser.add_argument("--jobs", type=int, default=6, help="одновременно запущенных процессов")
    parser.add_argument("--out", type=Path, default=REPO / "results" / "tof",
                        help="каталог для CSV, макросов и логов")
    parser.add_argument("--extra", nargs="*", default=[],
                        help='дополнительные команды макроса, например "/tof/sipm/sptr 30 ps"')
    args = parser.parse_args()

    exe = args.exe.resolve()
    if not exe.exists():
        sys.exit(f"Не найден исполняемый файл {exe}. Сначала соберите проект.")
    out_dir = args.out.resolve()
    (out_dir / "macros").mkdir(parents=True, exist_ok=True)
    (out_dir / "logs").mkdir(parents=True, exist_ok=True)

    configs = [(m, s) for m in args.materials for s in args.shapes]
    print(f"{len(configs)} конфигураций, {args.nev} событий на точку, {args.jobs} параллельно -> {out_dir}")

    failed = 0
    start = time.time()
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = [pool.submit(run_config, exe, m, s, args, out_dir) for m, s in configs]
        for future in as_completed(futures):
            name, code, seconds, log = future.result()
            status = "ok" if code == 0 else f"ОШИБКА (код {code}), см. {log}"
            failed += code != 0
            print(f"  {name:24s} {seconds / 60:6.1f} мин  {status}", flush=True)

    print(f"Готово за {(time.time() - start) / 60:.1f} мин; ошибок: {failed}")
    print(f"Анализ: python analysis/tof/tof_analysis.py {out_dir}")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
