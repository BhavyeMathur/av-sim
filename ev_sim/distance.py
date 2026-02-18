import numpy as np


def equirectangular_distance(lat1: np.ndarray, lon1: np.ndarray,
                             lat2: np.ndarray | float, lon2: np.ndarray | float,
                             is_radians: bool = True) -> np.ndarray:
    if not is_radians:
        lat1 = np.deg2rad(lat1)
        lon1 = np.deg2rad(lon1)
        lat2 = np.deg2rad(lat2)
        lon2 = np.deg2rad(lon2)

    dlat = lat2 - lat1
    dlon = lon2 - lon1
    avg_lat = (lat1 + lat2) / 2.0

    x = dlon * np.cos(avg_lat)
    return 6371 * np.sqrt(x * x + dlat * dlat)


def distance(lat1, lon1, lat2, lon2, is_radians: bool = True):
    return 0.572 + 1.273 * equirectangular_distance(lat1, lon1, lat2, lon2, is_radians=is_radians)


__all__ = ["equirectangular_distance", "distance"]
