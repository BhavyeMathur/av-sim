from typing import Literal

import pandas as pd
import numpy as np

import shapely
from shapely.geometry import shape, Polygon, Point
from scipy.spatial import cKDTree


def clip_trips_to_region(df: pd.DataFrame, polygon_geojson: dict,
                         pick_lat="pick_lat", pick_lon="pick_lon",
                         drop_lat="drop_lat", drop_lon="drop_lon",
                         keep_radians: bool = True) -> pd.DataFrame:
    poly = shape(polygon_geojson)  # degrees
    boundary = poly.boundary

    p_lat = np.degrees(df[pick_lat].values)
    p_lon = np.degrees(df[pick_lon].values)
    d_lat = np.degrees(df[drop_lat].values)
    d_lon = np.degrees(df[drop_lon].values)

    p_in = np.zeros(len(df), dtype=bool)
    d_in = np.zeros(len(df), dtype=bool)

    minx, miny, maxx, maxy = poly.bounds

    # quick check if points lie inside bounding rectangle
    p_bbox = (p_lon >= minx) & (p_lon <= maxx) & (p_lat >= miny) & (p_lat <= maxy)
    d_bbox = (d_lon >= minx) & (d_lon <= maxx) & (d_lat >= miny) & (d_lat <= maxy)

    # for the points that lie inside the rectangle, check if they lie inside the polygon
    p_pts = shapely.points(p_lon[p_bbox], p_lat[p_bbox])
    p_in[p_bbox] = shapely.covers(poly, p_pts)

    d_pts = shapely.points(d_lon[d_bbox], d_lat[d_bbox])
    d_in[d_bbox] = shapely.covers(poly, d_pts)

    # filter points that lie completely outside the polygon
    keep_mask = p_in | d_in

    df = df[keep_mask]
    p_lat = p_lat[keep_mask]
    p_lon = p_lon[keep_mask]
    d_lat = d_lat[keep_mask]
    d_lon = d_lon[keep_mask]
    p_in = p_in[keep_mask]
    d_in = d_in[keep_mask]

    # snap outside endpoints to boundary of polygon
    cross = p_in ^ d_in
    idx = np.where(cross)[0]
    p_in = p_in[idx]
    d_in = d_in[idx]

    coords = np.empty((len(idx), 2, 2), dtype=np.float32)  # shape: (m, 2, 2) where last dim is (x=lon, y=lat)
    coords[:, 0, 0] = p_lon[idx]
    coords[:, 0, 1] = p_lat[idx]
    coords[:, 1, 0] = d_lon[idx]
    coords[:, 1, 1] = d_lat[idx]

    lines = shapely.linestrings(coords)  # create lines from pickup/drop

    inter = shapely.intersection(lines, boundary)  # get intersection between lines and polygon
    hit = shapely.get_geometry(inter, 0)
    hit_x = shapely.get_x(hit)
    hit_y = shapely.get_y(hit)

    # if pick is inside -> move drop to boundary
    move_drop = p_in & (~d_in)
    j = idx[move_drop]
    d_lon[j] = hit_x[move_drop]
    d_lat[j] = hit_y[move_drop]

    # if drop is inside -> move pick to boundary
    move_pick = d_in & (~p_in)
    j = idx[move_pick]
    p_lon[j] = hit_x[move_pick]
    p_lat[j] = hit_y[move_pick]

    # write back (either radians or degrees)
    if keep_radians:
        df.loc[:, pick_lat] = np.radians(p_lat)
        df.loc[:, pick_lon] = np.radians(p_lon)
        df.loc[:, drop_lat] = np.radians(d_lat)
        df.loc[:, drop_lon] = np.radians(d_lon)
    else:
        df.loc[:, pick_lat] = p_lat
        df.loc[:, pick_lon] = p_lon
        df.loc[:, drop_lat] = d_lat
        df.loc[:, drop_lon] = d_lon

    return df


def snap_trips_to_boundary_points(df: pd.DataFrame,
                                  polygon_geojson: dict,
                                  boundary_points_deg: np.ndarray,  # (M,2) [[lon, lat], ...] degrees
                                  pick_lat="pick_lat", pick_lon="pick_lon",
                                  drop_lat="drop_lat", drop_lon="drop_lon",
                                  keep_radians: bool = True,
                                  chunksize: int = 2_000_000,
                                  mode: Literal["all", "outbound", "inbound", "internal"] = "all") -> pd.DataFrame:
    """
    mode:
      - "all": keep (crossing OR internal). drop both-out. snap only crossing rows.
      - "outbound": keep pick inside & drop outside. snap drop to nearest allowed boundary point.
      - "inbound": keep pick outside & drop inside. snap pick to nearest allowed boundary point.
      - "internal": keep pick inside & drop inside. no snapping.
    """
    poly = shape(polygon_geojson)
    boundary = poly.boundary

    boundary_points_deg = np.asarray(boundary_points_deg, dtype=np.float32)
    if boundary_points_deg.ndim != 2 or boundary_points_deg.shape[1] != 2:
        raise ValueError("boundary_points_deg must have shape (M, 2) as [[lon, lat], ...].")
    tree = cKDTree(boundary_points_deg)

    minx, miny, maxx, maxy = poly.bounds

    def compute_inside_masks(p_lon, p_lat, d_lon, d_lat):
        # bbox prefilter (cheap)
        p_bbox = (p_lon >= minx) & (p_lon <= maxx) & (p_lat >= miny) & (p_lat <= maxy)
        d_bbox = (d_lon >= minx) & (d_lon <= maxx) & (d_lat >= miny) & (d_lat <= maxy)

        p_in = np.zeros(p_lon.shape[0], dtype=bool)
        d_in = np.zeros(d_lon.shape[0], dtype=bool)

        if p_bbox.any():
            p_pts = shapely.points(p_lon[p_bbox], p_lat[p_bbox])
            p_in[p_bbox] = shapely.covers(poly, p_pts)  # includes boundary
        if d_bbox.any():
            d_pts = shapely.points(d_lon[d_bbox], d_lat[d_bbox])
            d_in[d_bbox] = shapely.covers(poly, d_pts)

        return p_in, d_in

    out = []
    n = len(df)

    for s in range(0, n, chunksize):
        chunk = df.iloc[s:s + chunksize].copy()

        # radians -> degrees
        p_lat = np.degrees(chunk[pick_lat].to_numpy(np.float32))
        p_lon = np.degrees(chunk[pick_lon].to_numpy(np.float32))
        d_lat = np.degrees(chunk[drop_lat].to_numpy(np.float32))
        d_lon = np.degrees(chunk[drop_lon].to_numpy(np.float32))

        p_in, d_in = compute_inside_masks(p_lon, p_lat, d_lon, d_lat)

        both_in = p_in & d_in
        both_out = ~(p_in | d_in)
        outbound = p_in & (~d_in)
        inbound = (~p_in) & d_in

        # ---- filter by requested mode
        if mode == "all":
            keep = ~both_out  # keep crossing + both_in
        elif mode == "outbound":
            keep = outbound
        elif mode == "inbound":
            keep = inbound
        elif mode == "internal":
            keep = both_in
        else:
            raise ValueError('mode must be one of {"all","outbound","inbound","internal"}')

        if not keep.any():
            continue

        chunk = chunk.loc[keep].copy()
        p_lat = p_lat[keep]
        p_lon = p_lon[keep]
        d_lat = d_lat[keep]
        d_lon = d_lon[keep]
        outbound = outbound[keep]
        inbound = inbound[keep]
        crossing = outbound | inbound

        # ---- snap only crossing rows (outbound/inbound)
        if crossing.any():
            idx = np.where(crossing)[0]

            coords = np.empty((len(idx), 2, 2), dtype=np.float64)
            coords[:, 0, 0] = p_lon[idx]
            coords[:, 0, 1] = p_lat[idx]
            coords[:, 1, 0] = d_lon[idx]
            coords[:, 1, 1] = d_lat[idx]
            lines = shapely.linestrings(coords)

            inter = shapely.intersection(lines, boundary)
            hit0 = shapely.get_geometry(inter, 0)
            hit_x = shapely.get_x(hit0)  # lon
            hit_y = shapely.get_y(hit0)  # lat

            ok = np.isfinite(hit_x) & np.isfinite(hit_y)
            if not ok.all():
                # drop degenerate crossing rows; keep non-crossing rows in chunk
                bad_idx = idx[~ok]
                keep2 = np.ones(len(chunk), dtype=bool)
                keep2[bad_idx] = False

                chunk = chunk.loc[keep2].copy()
                p_lat = p_lat[keep2]
                p_lon = p_lon[keep2]
                d_lat = d_lat[keep2]
                d_lon = d_lon[keep2]
                outbound = outbound[keep2]
                inbound = inbound[keep2]
                crossing = outbound | inbound

                if crossing.any():
                    idx = np.where(crossing)[0]
                    coords = np.empty((len(idx), 2, 2), dtype=np.float32)
                    coords[:, 0, 0] = p_lon[idx]
                    coords[:, 0, 1] = p_lat[idx]
                    coords[:, 1, 0] = d_lon[idx]
                    coords[:, 1, 1] = d_lat[idx]
                    lines = shapely.linestrings(coords)
                    inter = shapely.intersection(lines, boundary)
                    hit0 = shapely.get_geometry(inter, 0)
                    hit_x = shapely.get_x(hit0)
                    hit_y = shapely.get_y(hit0)

            if crossing.any():
                q = np.column_stack([hit_x, hit_y])  # lon,lat degrees
                _, nn = tree.query(q, k=1)
                snap = boundary_points_deg[nn]
                snap_lon = snap[:, 0]
                snap_lat = snap[:, 1]

                # orientation preserved:
                # outbound -> move drop; inbound -> move pick
                out_mask = outbound[idx]  # for each crossing row (aligned with idx)
                in_mask = inbound[idx]

                j = idx[out_mask]
                d_lon[j] = snap_lon[out_mask]
                d_lat[j] = snap_lat[out_mask]

                j = idx[in_mask]
                p_lon[j] = snap_lon[in_mask]
                p_lat[j] = snap_lat[in_mask]

        # ---- write back
        if keep_radians:
            chunk[pick_lat] = np.radians(p_lat)
            chunk[pick_lon] = np.radians(p_lon)
            chunk[drop_lat] = np.radians(d_lat)
            chunk[drop_lon] = np.radians(d_lon)
        else:
            chunk[pick_lat] = p_lat
            chunk[pick_lon] = p_lon
            chunk[drop_lat] = d_lat
            chunk[drop_lon] = d_lon

        out.append(chunk)

    return pd.concat(out, ignore_index=True) if out else df.iloc[0:0].copy()


def sample_points_in_polygon(polygon_geojson, n, radians=False, seed=None) -> pd.DataFrame:
    if seed is not None:
        np.random.seed(seed)

    poly = Polygon(polygon_geojson["coordinates"][0])
    min_lon, min_lat, max_lon, max_lat = poly.bounds

    lats = []
    lons = []

    # rejection sampling
    while len(lats) < n:
        lon = np.random.uniform(min_lon, max_lon)
        lat = np.random.uniform(min_lat, max_lat)

        if poly.contains(Point(lon, lat)):
            lats.append(lat)
            lons.append(lon)

    df = pd.DataFrame({"lat": lats, "lon": lons})

    if radians:
        df["lat"] = np.deg2rad(df["lat"])
        df["lon"] = np.deg2rad(df["lon"])

    df["lat"] = df["lat"].astype(np.float32)
    df["lon"] = df["lon"].astype(np.float32)
    return df


__all__ = ["clip_trips_to_region", "snap_trips_to_boundary_points", "sample_points_in_polygon"]
