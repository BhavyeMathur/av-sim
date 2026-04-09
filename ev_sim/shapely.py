from typing import Literal

import pandas as pd
import numpy as np

import shapely
from shapely.geometry import shape, Polygon, Point
from scipy.spatial import cKDTree

import osmnx as ox

MODE_TYPE = Literal["all", "outbound", "inbound", "internal", "external"]


def clip_trips_to_region(df: pd.DataFrame,
                         polygon_geojson: dict,
                         pick_lat: str = "pick_lat",
                         pick_lon: str = "pick_lon",
                         drop_lat: str = "drop_lat",
                         drop_lon: str = "drop_lon",
                         keep_radians: bool = True,
                         mode: MODE_TYPE = "all") -> pd.DataFrame:
    """
    Clip trips to a polygon and optionally snap crossing endpoints to the boundary.

    mode:
      - "all": keep crossing + internal trips; drop both-outside trips.
               Snap only crossing rows.
      - "outbound": keep only pick inside & drop outside.
                    Snap drop to polygon boundary.
      - "inbound": keep only pick outside & drop inside.
                   Snap pick to polygon boundary.
      - "internal": keep only pick inside & drop inside.
                    No snapping.
    """
    poly = shape(polygon_geojson)
    boundary = poly.boundary

    # read coordinates in degrees
    if keep_radians:
        p_lat = np.degrees(df[pick_lat].to_numpy())
        p_lon = np.degrees(df[pick_lon].to_numpy())
        d_lat = np.degrees(df[drop_lat].to_numpy())
        d_lon = np.degrees(df[drop_lon].to_numpy())
    else:
        p_lat = df[pick_lat].to_numpy()
        p_lon = df[pick_lon].to_numpy()
        d_lat = df[drop_lat].to_numpy()
        d_lon = df[drop_lon].to_numpy()

    p_in = np.zeros(len(df), dtype=bool)
    d_in = np.zeros(len(df), dtype=bool)

    minx, miny, maxx, maxy = poly.bounds

    # bbox prefilter
    p_bbox = (p_lon >= minx) & (p_lon <= maxx) & (p_lat >= miny) & (p_lat <= maxy)
    d_bbox = (d_lon >= minx) & (d_lon <= maxx) & (d_lat >= miny) & (d_lat <= maxy)

    if p_bbox.any():
        p_pts = shapely.points(p_lon[p_bbox], p_lat[p_bbox])
        p_in[p_bbox] = shapely.covers(poly, p_pts)

    if d_bbox.any():
        d_pts = shapely.points(d_lon[d_bbox], d_lat[d_bbox])
        d_in[d_bbox] = shapely.covers(poly, d_pts)

    both_in = p_in & d_in
    both_out = ~(p_in | d_in)
    outbound = p_in & (~d_in)
    inbound = (~p_in) & d_in

    # mode filter
    if mode == "all":
        keep_mask = ~both_out
    elif mode == "outbound":
        keep_mask = outbound
    elif mode == "inbound":
        keep_mask = inbound
    elif mode == "internal":
        keep_mask = both_in
    elif mode == "external":
        keep_mask = inbound | outbound
    else:
        raise ValueError('mode must be one of {"all", "outbound", "inbound", "internal", "external"}')

    df = df.loc[keep_mask].copy()
    if df.empty:
        return df

    p_lat = p_lat[keep_mask]
    p_lon = p_lon[keep_mask]
    d_lat = d_lat[keep_mask]
    d_lon = d_lon[keep_mask]
    outbound = outbound[keep_mask]
    inbound = inbound[keep_mask]

    crossing = outbound | inbound

    # snap only crossing rows
    if crossing.any():
        idx = np.where(crossing)[0]

        coords = np.empty((len(idx), 2, 2), dtype=np.float64)
        coords[:, 0, 0] = p_lon[idx]
        coords[:, 0, 1] = p_lat[idx]
        coords[:, 1, 0] = d_lon[idx]
        coords[:, 1, 1] = d_lat[idx]

        lines = shapely.linestrings(coords)
        inter = shapely.intersection(lines, boundary)

        hit = shapely.get_geometry(inter, 0)
        hit_x = shapely.get_x(hit)
        hit_y = shapely.get_y(hit)

        ok = np.isfinite(hit_x) & np.isfinite(hit_y)
        if not ok.all():
            # drop degenerate crossing rows with no usable boundary hit
            bad_idx = idx[~ok]
            keep2 = np.ones(len(df), dtype=bool)
            keep2[bad_idx] = False

            df = df.loc[keep2].copy()
            p_lat = p_lat[keep2]
            p_lon = p_lon[keep2]
            d_lat = d_lat[keep2]
            d_lon = d_lon[keep2]
            outbound = outbound[keep2]
            inbound = inbound[keep2]
            crossing = outbound | inbound

            if crossing.any():
                idx = np.where(crossing)[0]
                coords = np.empty((len(idx), 2, 2), dtype=np.float64)
                coords[:, 0, 0] = p_lon[idx]
                coords[:, 0, 1] = p_lat[idx]
                coords[:, 1, 0] = d_lon[idx]
                coords[:, 1, 1] = d_lat[idx]

                lines = shapely.linestrings(coords)
                inter = shapely.intersection(lines, boundary)
                hit = shapely.get_geometry(inter, 0)
                hit_x = shapely.get_x(hit)
                hit_y = shapely.get_y(hit)

        if crossing.any():
            # outbound -> move drop to boundary
            out_mask = outbound[idx]
            j = idx[out_mask]
            d_lon[j] = hit_x[out_mask]
            d_lat[j] = hit_y[out_mask]

            # inbound -> move pick to boundary
            in_mask = inbound[idx]
            j = idx[in_mask]
            p_lon[j] = hit_x[in_mask]
            p_lat[j] = hit_y[in_mask]

    # write back
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
                                  mode: MODE_TYPE = "all") -> pd.DataFrame:
    """
    mode:
      - "all": keep (crossing OR internal). drop both-out. snap only crossing rows.
      - "outbound": keep pick inside & drop outside. snap drop to nearest allowed boundary point.
      - "inbound": keep pick outside & drop inside. snap pick to nearest allowed boundary point.
      - "internal": keep pick inside & drop inside. no snapping.
      - "external": keep inbound and outbound trips
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
        elif mode == "external":
            keep = outbound | inbound
        else:
            raise ValueError('mode must be one of {"all", "outbound", "inbound", "internal", "external"}')

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


def _extract_points(geom) -> list:
    if geom is None or shapely.is_empty(geom):
        return []

    gt = geom.geom_type
    if gt == "Point":
        return [geom]
    if gt == "MultiPoint":
        return list(geom.geoms)
    if gt == "GeometryCollection":
        out = []
        for g in geom.geoms:
            out.extend(_extract_points(g))
        return out

    return []


def _dedupe_points(points: list[Point], tolerance_deg: float = 1e-5) -> list[Point]:
    """
    Deduplicate nearby points by snapping to a grid.
    tolerance_deg=1e-5 is about 1 meter in latitude.
    """
    if not points:
        return []

    seen = {}
    for p in points:
        key = (round(p.x / tolerance_deg), round(p.y / tolerance_deg))
        if key not in seen:
            seen[key] = p
    return list(seen.values())


def get_road_boundary_intersections(polygon_geojson: dict,
                                    network_type: str = "drive",
                                    keep: tuple[str, ...] = ("motorway_link", "primary_link",
                                                             "secondary", "secondary_link"),
                                    dedupe_tol_deg: float = 1e-4) -> pd.DataFrame:
    """
    Return a DataFrame with columns [lat, lon] for road/polygon-boundary crossings.
    Coordinates are in degrees.

    dedupe_tol_deg:
      1e-5  ~ about 1 m in latitude
      5e-5  ~ about 5 m
      1e-4  ~ about 11 m
    """
    poly = shape(polygon_geojson)
    boundary = poly.boundary

    G = ox.graph_from_polygon(poly, network_type=network_type, simplify=False, truncate_by_edge=True)
    edges = ox.graph_to_gdfs(G, nodes=False, fill_edge_geometry=True)
    edges = edges[edges["highway"].apply(lambda x: any(h in keep for h in (x if isinstance(x, list) else [x])))]

    # quick bbox filter
    minx, miny, maxx, maxy = poly.bounds
    edges = edges.cx[minx:maxx, miny:maxy]

    # filter roads to remove self-crossings (basically removing roads that briefly exit and re-enter the polygon)
    # by filtering those such that one endpoint lies outside the polygon and the other lies inside
    cross = []
    for geom in edges.geometry.to_numpy():
        coords = np.asarray(geom.coords)
        p0 = shapely.Point(coords[0])
        p1 = shapely.Point(coords[-1])

        inside0 = poly.covers(p0)  # inside or on boundary
        inside1 = poly.covers(p1)  # inside or on boundary

        cross.append(bool(inside0) ^ bool(inside1))  # ^ ensures bool inside0 != inside1 (i.e. exactly 1 is inside)

    edges = edges[np.array(cross)]

    pts = []
    for geom in edges.geometry.to_numpy():
        inter = shapely.intersection(geom, boundary)
        pts.extend(_extract_points(inter))

    if not pts:
        return pd.DataFrame(columns=["lon", "lat"])

    arr = np.array([(p.x, p.y) for p in pts], dtype=np.float64)

    # deduplication by snapping to a grid
    qx = np.round(arr[:, 0] / dedupe_tol_deg).astype(np.int64)
    qy = np.round(arr[:, 1] / dedupe_tol_deg).astype(np.int64)
    _, keep = np.unique(np.column_stack([qx, qy]), axis=0, return_index=True)
    arr = arr[np.sort(keep)]

    return pd.DataFrame({"lon": arr[:, 0], "lat": arr[:, 1]})


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


__all__ = ["clip_trips_to_region", "snap_trips_to_boundary_points", "sample_points_in_polygon",
           "get_road_boundary_intersections"]
