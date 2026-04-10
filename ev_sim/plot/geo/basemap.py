from dataclasses import dataclass, field

from PIL import Image
import contextily as cx


@dataclass
class BasemapStyle:
    base_provider: object = field(default_factory=lambda: cx.providers.CartoDB.DarkMatterNoLabels)
    labels_provider: object | None = field(default_factory=lambda: cx.providers.CartoDB.DarkMatterOnlyLabels)
    base_zoom: int = 14
    labels_zoom: int = 13


def crop_img_to_mercator_extent(img: Image.Image, img_extent, target_extent) -> Image.Image:
    left, right, bottom, top = img_extent
    x0, x1, y0, y1 = target_extent

    w, h = img.size

    px0 = round((x0 - left) / (right - left) * w)
    px1 = round((x1 - left) / (right - left) * w)

    py0 = round((top - y1) / (top - bottom) * h)
    py1 = round((top - y0) / (top - bottom) * h)

    return img.crop((px0, py0, px1, py1))


def render_basemap(basemap: BasemapStyle,
                   x_range: tuple[float, float],
                   y_range: tuple[float, float],
                   size: tuple[int, int],
                   draw_labels: bool = True) -> tuple[Image.Image, Image.Image | None]:
    target_extent = (x_range[0], x_range[1], y_range[0], y_range[1])

    base, base_extent = cx.bounds2img(
        x_range[0], y_range[0], x_range[1], y_range[1],
        ll=False,
        source=basemap.base_provider,
        zoom=basemap.base_zoom,
    )
    base_img = Image.fromarray(base).convert("RGBA")
    base_img = crop_img_to_mercator_extent(base_img, base_extent, target_extent)
    base_img = base_img.resize(size, Image.Resampling.BILINEAR)

    labels_img = None
    if draw_labels and basemap.labels_provider is not None:
        labels, labels_extent = cx.bounds2img(x_range[0], y_range[0], x_range[1], y_range[1],
                                              ll=False, source=basemap.labels_provider, zoom=basemap.labels_zoom)
        labels_img = Image.fromarray(labels).convert("RGBA")
        labels_img = crop_img_to_mercator_extent(labels_img, labels_extent, target_extent)
        labels_img = labels_img.resize(size, Image.Resampling.LANCZOS)

    return base_img, labels_img


__all__ = ["BasemapStyle", "render_basemap"]
