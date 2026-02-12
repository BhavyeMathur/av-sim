import numpy as np


def equirectangular_distance(lat1: np.ndarray, lon1: np.ndarray,
                             lat2: np.ndarray | float, lon2: np.ndarray | float) -> np.ndarray:
    dlat = lat2 - lat1
    dlon = lon2 - lon1
    avg_lat = (lat1 + lat2) / 2.0

    x = dlon * np.cos(avg_lat)
    return 6371 * np.sqrt(x * x + dlat * dlat)


def distance(lat1, lon1, lat2, lon2):
    return 0.572 + 1.273 * equirectangular_distance(lat1, lon1, lat2, lon2)


__all__ = ["equirectangular_distance", "distance"]
