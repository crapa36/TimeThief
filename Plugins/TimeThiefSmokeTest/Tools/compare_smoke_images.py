#!/usr/bin/env python3
"""Compare two smoke screenshots in an optional rectangular ROI."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image


def parse_roi(value: str) -> tuple[int, int, int, int]:
    parts = tuple(int(part.strip()) for part in value.split(","))
    if len(parts) != 4 or parts[2] <= 0 or parts[3] <= 0:
        raise argparse.ArgumentTypeError("ROI must be x,y,width,height with positive size")
    return parts


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("reference", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--roi", type=parse_roi)
    parser.add_argument("--changed-threshold", type=float, default=0.0)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    reference = np.asarray(Image.open(args.reference).convert("RGB"), dtype=np.float32)
    candidate = np.asarray(Image.open(args.candidate).convert("RGB"), dtype=np.float32)
    if reference.shape != candidate.shape:
        raise SystemExit(f"image sizes differ: {reference.shape} != {candidate.shape}")

    roi = args.roi or (0, 0, reference.shape[1], reference.shape[0])
    x, y, width, height = roi
    if x < 0 or y < 0 or x + width > reference.shape[1] or y + height > reference.shape[0]:
        raise SystemExit(f"ROI {roi} is outside {reference.shape[1]}x{reference.shape[0]}")

    difference = np.abs(
        reference[y : y + height, x : x + width]
        - candidate[y : y + height, x : x + width]
    )
    per_pixel_max = difference.max(axis=2)
    metrics = {
        "reference": str(args.reference.resolve()),
        "candidate": str(args.candidate.resolve()),
        "roi": {"x": x, "y": y, "width": width, "height": height},
        "mae_8bit": float(difference.mean()),
        "p95_8bit": float(np.percentile(difference, 95.0)),
        "max_8bit": float(difference.max()),
        "changed_pixel_ratio": float(np.mean(per_pixel_max > args.changed_threshold)),
        "changed_threshold_8bit": args.changed_threshold,
    }
    serialized = json.dumps(metrics, indent=2)
    print(serialized)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(serialized + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
