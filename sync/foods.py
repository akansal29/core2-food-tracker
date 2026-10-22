"""Food database loading, validation, and compilation for the Core2 food tracker.

The laptop owns food definitions in data/foods.csv (see the implementation guide,
Section 3). This module validates that file and compiles it to the foods.json the
device loads. Combo nutrition is always computed from ingredients.
"""
from __future__ import annotations

import csv
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

KNOWN_TAGS = {
    "portable", "no-prep", "calorie-dense", "drinkable", "refrigeration-required",
    "sweet", "savory", "small-portion", "meal", "protein-rich", "combo",
}
REQUIRED_COLUMNS = [
    "food_id", "name", "serving_description", "calories_per_serving",
    "protein_g_per_serving", "tags", "is_combo", "ingredients", "archived",
]
ID_RE = re.compile(r"^[a-z0-9_]{1,48}$")
SCHEMA_VERSION = 1


@dataclass
class Food:
    food_id: str
    name: str
    serving: str
    calories: float
    protein: float
    tags: list[str]
    is_combo: bool
    ingredients: list[tuple[str, float]]
    archived: bool
    source_status: str = ""
    row: int = 0


@dataclass
class Report:
    errors: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)

    @property
    def ok(self) -> bool:
        return not self.errors


def _bool(value: str) -> bool | None:
    v = (value or "").strip().lower()
    if v in ("true", "1", "yes"):
        return True
    if v in ("false", "0", "no", ""):
        return False
    return None


def _num(value: str, lo: float, hi: float) -> float | None:
    try:
        x = float(value)
    except (TypeError, ValueError):
        return None
    return x if lo <= x <= hi else None


def load_foods(path: Path) -> tuple[dict[str, Food], Report]:
    report = Report()
    foods: dict[str, Food] = {}
    with open(path, newline="", encoding="utf-8-sig") as fh:
        reader = csv.DictReader(fh)
        missing = [c for c in REQUIRED_COLUMNS if c not in (reader.fieldnames or [])]
        if missing:
            report.errors.append(f"missing columns: {', '.join(missing)}")
            return foods, report
        for i, r in enumerate(reader, start=2):  # row 1 is the header
            where = f"row {i}"
            fid = (r["food_id"] or "").strip()
            if not ID_RE.match(fid):
                report.errors.append(f"{where}: bad food_id {fid!r} (use a-z, 0-9, _)")
                continue
            if fid in foods:
                report.errors.append(f"{where}: duplicate food_id {fid!r} (first at row {foods[fid].row})")
                continue
            name = (r["name"] or "").strip()
            if not name:
                report.errors.append(f"{where} ({fid}): empty name")
            is_combo, archived = _bool(r["is_combo"]), _bool(r["archived"])
            if is_combo is None or archived is None:
                report.errors.append(f"{where} ({fid}): is_combo/archived must be true or false")
                continue
            cal = _num(r["calories_per_serving"], 0, 5000)
            pro = _num(r["protein_g_per_serving"], 0, 300)
            if not is_combo and (cal is None or pro is None):
                report.errors.append(f"{where} ({fid}): calories must be 0-5000 and protein 0-300")
                continue
            tags = [t.strip() for t in (r["tags"] or "").split("|") if t.strip()]
            for t in tags:
                if t not in KNOWN_TAGS:
                    report.warnings.append(f"{where} ({fid}): unknown tag {t!r}")
            ingredients: list[tuple[str, float]] = []
            if is_combo:
                for part in (r["ingredients"] or "").split(";"):
                    part = part.strip()
                    if not part:
                        continue
                    ing, _, qty = part.partition(":")
                    q = _num(qty, 0.01, 20)
                    if q is None:
                        report.errors.append(f"{where} ({fid}): bad ingredient quantity in {part!r}")
                        continue
                    ingredients.append((ing.strip(), q))
                if not ingredients:
                    report.errors.append(f"{where} ({fid}): combo has no ingredients")
            foods[fid] = Food(fid, name, (r["serving_description"] or "").strip(),
                              cal if cal is not None else 0.0, pro if pro is not None else 0.0,
                              tags, is_combo, ingredients, archived,
                              (r.get("nutrition_source_status") or "").strip(), i)
    _check_combos(foods, report)
    return foods, report


def combo_totals(food: Food, foods: dict[str, Food]) -> tuple[float, float]:
    cal = sum(foods[i].calories * q for i, q in food.ingredients)
    pro = sum(foods[i].protein * q for i, q in food.ingredients)
    return cal, pro


def _check_combos(foods: dict[str, Food], report: Report) -> None:
    for f in foods.values():
        if not f.is_combo:
            continue
        bad = False
        for ing, _ in f.ingredients:
            target = foods.get(ing)
            if target is None:
                report.errors.append(f"row {f.row} ({f.food_id}): ingredient {ing!r} does not exist"); bad = True
            elif target.is_combo:
                report.errors.append(f"row {f.row} ({f.food_id}): ingredient {ing!r} is a combo (no nesting)"); bad = True
            elif target.archived and not f.archived:
                report.errors.append(f"row {f.row} ({f.food_id}): ingredient {ing!r} is archived"); bad = True
        if bad:
            continue
        cal, pro = combo_totals(f, foods)
        if abs(cal - f.calories) > 2 or abs(pro - f.protein) > 0.5:
            report.warnings.append(
                f"row {f.row} ({f.food_id}): stated {f.calories:g} kcal / {f.protein:g} g, "
                f"ingredients add to {cal:.0f} kcal / {pro:.1f} g (computed value is used)")
        f.calories, f.protein = cal, pro


def compile_json(foods: dict[str, Food]) -> dict:
    """Compact JSON for the device. Short keys keep the file small on flash."""
    out = []
    for f in foods.values():
        item = {"id": f.food_id, "n": f.name, "s": f.serving,
                "c": round(f.calories, 1), "p": round(f.protein, 1), "t": f.tags}
        if f.is_combo:
            item["i"] = [[i, q] for i, q in f.ingredients]
        if f.archived:
            item["a"] = 1
        out.append(item)
    return {"v": SCHEMA_VERSION, "foods": out}


def main(argv: list[str]) -> int:
    import argparse
    ap = argparse.ArgumentParser(description="Validate foods.csv and optionally compile foods.json")
    ap.add_argument("csv", nargs="?", default="data/foods.csv")
    ap.add_argument("--out", help="write compiled foods.json here")
    args = ap.parse_args(argv)
    foods, report = load_foods(Path(args.csv))
    for w in report.warnings:
        print("warning:", w)
    for e in report.errors:
        print("ERROR:  ", e)
    if not report.ok:
        print(f"{len(report.errors)} error(s); nothing written.")
        return 1
    print(f"OK: {len(foods)} foods ({sum(f.is_combo for f in foods.values())} combos, "
          f"{sum(f.archived for f in foods.values())} archived), {len(report.warnings)} warning(s)")
    if args.out:
        data = json.dumps(compile_json(foods), separators=(",", ":"), ensure_ascii=False)
        Path(args.out).write_text(data, encoding="utf-8")
        print(f"wrote {args.out} ({len(data.encode())} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
