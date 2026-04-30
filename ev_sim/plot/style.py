import matplotlib.dates as mdates
import numpy as np


def _is_datetime_axis(ax) -> bool:
    try:
        converter = ax.xaxis.converter
        return converter is not None or (len(ax.lines) > 0 and any(
            np.issubdtype(np.asarray(line.get_xdata()).dtype, np.datetime64) for line in ax.lines))
    except Exception:
        return False


def _format_datetime_axis(ax, spine_color="#7b8290"):
    locator = mdates.AutoDateLocator()
    formatter = mdates.ConciseDateFormatter(locator)
    ax.xaxis.set_major_locator(locator)
    ax.xaxis.set_major_formatter(formatter)

    ax.tick_params(axis="x", colors=spine_color)
    ax.tick_params(axis="y", colors=spine_color)


def style_title(ax, title: str, subtitle: str | None = None, *, title_color="#111827", subtitle_color="#6b7280",
                subtitle_size: int = 9, ):
    ax.set_title(title, loc="left", fontweight="bold", color=title_color, pad=20)
    if subtitle:
        ax.text(0.0, 1.02, subtitle, transform=ax.transAxes, ha="left", va="bottom", fontsize=subtitle_size,
                color=subtitle_color, )


def style_plot(ax, title: str, subtitle: str | None = None, *, spine_color="#7b8290", grid_color="#374151",
               title_color="#111827", subtitle_color="#6b7280", ax_color="#000", ):
    ax.grid(axis="y", linestyle=(0, (1.2, 2.4)), linewidth=1.0, color=grid_color, alpha=0.55)

    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.spines["left"].set_color(spine_color)
    ax.spines["bottom"].set_color(spine_color)

    ax.tick_params(axis="x", colors=ax_color)
    ax.tick_params(axis="y", colors=ax_color)

    if _is_datetime_axis(ax):
        _format_datetime_axis(ax, spine_color=spine_color)

    style_title(ax, title, subtitle=subtitle, title_color=title_color, subtitle_color=subtitle_color)


def style_pie(ax, title: str, subtitle: str | None = None, *, title_color="#111827", subtitle_color="#6b7280"):
    style_title(ax, title, subtitle=subtitle, title_color=title_color, subtitle_color=subtitle_color)


__all__ = ["style_title", "style_plot", "style_pie"]
