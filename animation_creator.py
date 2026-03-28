"""Simple animation creation software.

This module provides a tiny desktop-free animation "studio" that can:
1. Generate a configurable scene with moving circles.
2. Render frames.
3. Export the result as an animated GIF.

Usage examples:
    python animation_creator.py
    python animation_creator.py --width 800 --height 450 --frames 150 --output demo.gif
"""

from __future__ import annotations

import argparse
import math
import random
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, List, Tuple

from PIL import Image, ImageDraw

Color = Tuple[int, int, int]


@dataclass
class MovingCircle:
    """A circle that moves along a smooth oscillating path."""

    radius: int
    color: Color
    speed_x: float
    speed_y: float
    phase_x: float
    phase_y: float
    amplitude_x: float
    amplitude_y: float

    def position(self, frame_index: int, width: int, height: int) -> Tuple[float, float]:
        """Compute the (x, y) position for the given frame."""
        base_x = width / 2
        base_y = height / 2
        x = base_x + math.sin(frame_index * self.speed_x + self.phase_x) * self.amplitude_x
        y = base_y + math.cos(frame_index * self.speed_y + self.phase_y) * self.amplitude_y
        return x, y


@dataclass
class AnimationConfig:
    width: int = 640
    height: int = 360
    frames: int = 120
    fps: int = 24
    circles: int = 7
    seed: int = 42
    output: Path = Path("animation.gif")


class AnimationStudio:
    """Core engine for procedural animation generation."""

    def __init__(self, config: AnimationConfig):
        self.config = config
        self._rng = random.Random(config.seed)
        self.objects = self._build_scene()

    def _random_color(self) -> Color:
        return (
            self._rng.randint(50, 255),
            self._rng.randint(50, 255),
            self._rng.randint(50, 255),
        )

    def _build_scene(self) -> List[MovingCircle]:
        objects: List[MovingCircle] = []
        for _ in range(self.config.circles):
            radius = self._rng.randint(10, 40)
            objects.append(
                MovingCircle(
                    radius=radius,
                    color=self._random_color(),
                    speed_x=self._rng.uniform(0.03, 0.14),
                    speed_y=self._rng.uniform(0.03, 0.14),
                    phase_x=self._rng.uniform(0, math.tau),
                    phase_y=self._rng.uniform(0, math.tau),
                    amplitude_x=self._rng.uniform(30, self.config.width / 2.4),
                    amplitude_y=self._rng.uniform(20, self.config.height / 2.4),
                )
            )
        return objects

    def render_frame(self, frame_index: int) -> Image.Image:
        """Render a single frame as a PIL image."""
        width, height = self.config.width, self.config.height
        image = Image.new("RGB", (width, height), (15, 18, 30))
        draw = ImageDraw.Draw(image, "RGBA")

        # Add a subtle vertical gradient for depth.
        for y in range(height):
            alpha = int(120 * (y / max(height - 1, 1)))
            draw.line([(0, y), (width, y)], fill=(30, 60, 120, alpha))

        # Draw moving circles.
        for obj in self.objects:
            x, y = obj.position(frame_index, width, height)
            r = obj.radius
            bbox = (x - r, y - r, x + r, y + r)

            # glow
            glow_r = int(r * 1.8)
            glow_bbox = (x - glow_r, y - glow_r, x + glow_r, y + glow_r)
            draw.ellipse(glow_bbox, fill=(*obj.color, 35))
            draw.ellipse(bbox, fill=obj.color)

        # Add frame counter text.
        draw.text((10, 10), f"Frame {frame_index + 1}/{self.config.frames}", fill=(230, 230, 240, 230))
        return image

    def render_all_frames(self) -> Iterable[Image.Image]:
        for i in range(self.config.frames):
            yield self.render_frame(i)

    def export_gif(self) -> Path:
        frames = list(self.render_all_frames())
        duration_ms = int(1000 / max(self.config.fps, 1))
        self.config.output.parent.mkdir(parents=True, exist_ok=True)
        frames[0].save(
            self.config.output,
            save_all=True,
            append_images=frames[1:],
            duration=duration_ms,
            loop=0,
            optimize=False,
            disposal=2,
        )
        return self.config.output


def parse_args() -> AnimationConfig:
    parser = argparse.ArgumentParser(description="Create a procedural animated GIF.")
    parser.add_argument("--width", type=int, default=640, help="Frame width in pixels.")
    parser.add_argument("--height", type=int, default=360, help="Frame height in pixels.")
    parser.add_argument("--frames", type=int, default=120, help="Number of frames to render.")
    parser.add_argument("--fps", type=int, default=24, help="Frames per second.")
    parser.add_argument("--circles", type=int, default=7, help="Number of moving circles.")
    parser.add_argument("--seed", type=int, default=42, help="Random seed for reproducibility.")
    parser.add_argument("--output", type=Path, default=Path("animation.gif"), help="Output GIF path.")

    args = parser.parse_args()
    return AnimationConfig(
        width=max(100, args.width),
        height=max(100, args.height),
        frames=max(1, args.frames),
        fps=max(1, args.fps),
        circles=max(1, args.circles),
        seed=args.seed,
        output=args.output,
    )


def main() -> None:
    config = parse_args()
    studio = AnimationStudio(config)
    output_path = studio.export_gif()
    print(f"Animation saved to: {output_path.resolve()}")


if __name__ == "__main__":
    main()
