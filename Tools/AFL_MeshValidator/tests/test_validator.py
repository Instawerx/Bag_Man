"""Tests for afl_mesh_validator (AFL-0217)."""
from __future__ import annotations

import json
import shutil
from pathlib import Path

import pytest

import afl_mesh_validator as v

FIXTURES = Path(__file__).parent / "fixtures"


def _budgets():
    return v.load_budgets()


def test_passing_kit_piece_validates():
    row = v.validate_file(FIXTURES / "SM_AFL_Crate_Kit.gltf", _budgets())
    assert row["pass"] is True, row["reasons"]
    assert row["class"] == "kit_piece"
    poly = next(c for c in row["checks"] if c["name"] == "POLY_BUDGET")
    assert poly["pass"] is True


def test_over_budget_hero_fails(capsys):
    rc = v.main([str(FIXTURES / "SKM_AFL_Pilot_Hero.gltf")])
    assert rc == 1
    out = capsys.readouterr().out
    assert "POLY_BUDGET" in out
    assert "FAIL" in out


def test_misnamed_prop_fails():
    row = v.validate_file(FIXTURES / "SM_AFL_thing_Kit_extra_bad_segments.gltf", _budgets())
    assert row["pass"] is False
    assert any("NAMING" in r for r in row["reasons"]), row["reasons"]


def test_directory_mode_aggregates(capsys):
    rc = v.main([str(FIXTURES)])
    assert rc == 1  # any failure in the dir → exit 1


def test_blender_missing_skips_fbx(monkeypatch, tmp_path):
    monkeypatch.setattr(shutil, "which", lambda _name: None)
    fbx = tmp_path / "SM_AFL_Foo_Kit.fbx"
    fbx.write_bytes(b"not really fbx")
    rc = v.main([str(fbx)])
    assert rc == 2
