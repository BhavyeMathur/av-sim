from PIL import Image as _Image
import numpy as _np


def _to_float(img):
    return _np.asarray(img).astype(_np.float32) / 255.0


def _to_img(arr):
    return _Image.fromarray(_np.clip(arr * 255, 0, 255).astype(_np.uint8), "RGBA")


def blend_add(bottom, top):
    a = _to_float(bottom)
    b = _to_float(top)

    out = a.copy()
    out[..., :3] = _np.clip(a[..., :3] + b[..., :3], 0, 1)
    out[..., 3] = _np.clip(a[..., 3] + b[..., 3], 0, 1)

    return _to_img(out)


def blend_add_weighted(bottom, top, strength=0.5):
    a = _to_float(bottom)
    b = _to_float(top)

    out = a.copy()
    out[..., :3] = _np.clip(a[..., :3] + strength * b[..., :3], 0, 1)
    out[..., 3] = _np.clip(a[..., 3] + strength * b[..., 3], 0, 1)

    return _to_img(out)


def blend_add_normalized(bottom, top):
    a = _to_float(bottom)
    b = _to_float(top)

    out = a.copy()
    out[..., :3] = (a[..., :3] + b[..., :3]) / (1 + b[..., :3])
    out[..., 3] = _np.maximum(a[..., 3], b[..., 3])

    return _to_img(out)


def blend_add_soft(bottom, top, strength=1.0):
    a = _to_float(bottom)
    b = _to_float(top)

    out = a.copy()
    summed = a[..., :3] + strength * b[..., :3]

    out[..., :3] = summed / (1 + summed)  # soft compression
    out[..., 3] = _np.maximum(a[..., 3], b[..., 3])

    return _to_img(out)


def blend_add_gamma(strength=1.0, gamma=1.5):
    def _closure(bottom, top):
        a = _to_float(bottom)
        b = _to_float(top)

        out = a.copy()
        summed = a[..., :3] + strength * b[..., :3]

        out[..., :3] = _np.clip(summed, 0, 1) ** (1 / gamma)
        out[..., 3] = _np.maximum(a[..., 3], b[..., 3])

        return _to_img(out)

    return _closure


def blend_screen(bottom, top):
    a = _to_float(bottom)
    b = _to_float(top)

    out = a.copy()
    out[..., :3] = 1 - (1 - a[..., :3]) * (1 - b[..., :3])
    out[..., 3] = _np.maximum(a[..., 3], b[..., 3])

    return _to_img(out)


def blend_lighten(bottom, top):
    a = _to_float(bottom)
    b = _to_float(top)

    out = a.copy()
    out[..., :3] = _np.maximum(a[..., :3], b[..., :3])
    out[..., 3] = _np.maximum(a[..., 3], b[..., 3])

    return _to_img(out)


def blend_multiply(bottom, top):
    a = _to_float(bottom)
    b = _to_float(top)

    out = a.copy()
    out[..., :3] = a[..., :3] * b[..., :3]
    out[..., 3] = _np.maximum(a[..., 3], b[..., 3])

    return _to_img(out)


def blend_difference(bottom, top):
    a = _to_float(bottom)
    b = _to_float(top)

    out = a.copy()
    out[..., :3] = _np.abs(a[..., :3] - b[..., :3])
    out[..., 3] = _np.maximum(a[..., 3], b[..., 3])

    return _to_img(out)


def blend_subtract(bottom, top):
    a = _to_float(bottom)
    b = _to_float(top)

    out = a.copy()
    out[..., :3] = _np.clip(a[..., :3] - b[..., :3], 0, 1)
    out[..., 3] = _np.maximum(a[..., 3], b[..., 3])

    return _to_img(out)


def blend_overlay(bottom, top):
    a = _to_float(bottom)
    b = _to_float(top)

    out = a.copy()
    mask = a[..., :3] <= 0.5

    out[..., :3] = _np.where(
        mask,
        2 * a[..., :3] * b[..., :3],
        1 - 2 * (1 - a[..., :3]) * (1 - b[..., :3])
    )

    out[..., 3] = _np.maximum(a[..., 3], b[..., 3])

    return _to_img(out)


def blend_soft_light(bottom, top):
    a = _to_float(bottom)
    b = _to_float(top)

    A = a[..., :3]
    B = b[..., :3]

    out = a.copy()

    # g(A)
    g = _np.where(
        A <= 0.25,
        ((16 * A - 12) * A + 4) * A,
        _np.sqrt(A)
    )

    result = _np.where(
        B <= 0.5,
        A - (1 - 2 * B) * A * (1 - A),
        A + (2 * B - 1) * (g - A)
    )

    out[..., :3] = result
    out[..., 3] = _np.maximum(a[..., 3], b[..., 3])

    return _to_img(out)


BLEND_FUNCS = {
    "alpha": lambda a, b: _Image.alpha_composite(a, b),
    "add": blend_add,
    "add_normalized": blend_add_normalized,
    "add_weighted": blend_add_weighted,
    "add_soft": blend_add_soft,
    "add_gamma": blend_add_gamma(strength=1.0, gamma=1.5),
    "screen": blend_screen,
    "lighten": blend_lighten,
    "multiply": blend_multiply,
    "difference": blend_difference,
    "subtract": blend_subtract,
    "overlay": blend_overlay,
    "soft_light": blend_soft_light,
}
