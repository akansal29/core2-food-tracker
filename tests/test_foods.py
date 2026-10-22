import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "sync"))

from foods import compile_json, load_foods  # noqa: E402

HEADER = "food_id,name,serving_description,calories_per_serving,protein_g_per_serving,tags,is_combo,ingredients,archived\n"


def write(tmp_path, body):
    p = tmp_path / "foods.csv"
    p.write_text(HEADER + body, encoding="utf-8")
    return p


def test_starter_file_is_valid():
    foods, report = load_foods(ROOT / "data" / "foods.csv")
    assert report.ok, report.errors
    assert len(foods) == 48
    assert not report.warnings, report.warnings


def test_combo_computed_from_ingredients(tmp_path):
    p = write(tmp_path, "a,A,1,100,10,,false,,false\nb,B,1,50,2,,false,,false\n"
                        "c,C,combo,999,99,combo,true,a:1;b:0.5,false\n")
    foods, report = load_foods(p)
    assert report.ok
    assert foods["c"].calories == 125 and foods["c"].protein == 11
    assert any("computed value is used" in w for w in report.warnings)


def test_duplicate_and_bad_ids(tmp_path):
    p = write(tmp_path, "a,A,1,100,10,,false,,false\na,A2,1,100,10,,false,,false\nBad-ID,X,1,1,1,,false,,false\n")
    _, report = load_foods(p)
    assert any("duplicate" in e for e in report.errors)
    assert any("bad food_id" in e for e in report.errors)


def test_out_of_range_and_non_numeric(tmp_path):
    p = write(tmp_path, "a,A,1,9000,10,,false,,false\nb,B,1,abc,1,,false,,false\n")
    _, report = load_foods(p)
    assert len(report.errors) == 2


def test_combo_reference_rules(tmp_path):
    p = write(tmp_path, "a,A,1,100,10,,false,,true\nb,B,c,0,0,combo,true,a:1,false\n"
                        "c,C,c,0,0,combo,true,b:1,false\nd,D,c,0,0,combo,true,zz:1,false\n")
    _, report = load_foods(p)
    text = " ".join(report.errors)
    assert "is archived" in text and "is a combo" in text and "does not exist" in text


def test_unknown_tag_is_warning_only(tmp_path):
    p = write(tmp_path, "a,A,1,100,10,crunchy,false,,false\n")
    _, report = load_foods(p)
    assert report.ok and report.warnings


def test_compile_json_shape():
    foods, _ = load_foods(ROOT / "data" / "foods.csv")
    data = compile_json(foods)
    assert data["v"] == 1
    combo = next(f for f in data["foods"] if f["id"] == "combo_rice_chicken")
    assert combo["c"] == 390 and combo["i"] == [["rice_cup", 1.0], ["cooked_chicken_4oz", 1.0]]
