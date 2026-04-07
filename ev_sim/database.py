from typing import Any, Callable
import sqlite3

from .utils import utc_now_iso

COMPUTE_STATS_FUNC = Callable[[dict[str, Any]], dict[str, Any]]


def ensure_statistics_table(conn: sqlite3.Connection) -> None:
    conn.execute("""CREATE TABLE IF NOT EXISTS statistics
    (
        run_id
        INTEGER
        PRIMARY
        KEY,
        computed_at
        TEXT
        NOT
        NULL,
        FOREIGN
        KEY
                    (
        run_id
                    ) REFERENCES runs
                    (
                        id
                    ) ON DELETE CASCADE )
                 """)
    conn.commit()


def get_table_columns(conn: sqlite3.Connection, table: str) -> set[str]:
    rows = conn.execute(f"PRAGMA table_info({table})").fetchall()
    return {row[1] for row in rows}  # row[1] = column name


def sqlite_type_for_value(value: Any) -> str:
    if isinstance(value, bool):
        return "INTEGER"
    if isinstance(value, int):
        return "INTEGER"
    if isinstance(value, float):
        return "REAL"
    if isinstance(value, (bytes, bytearray)):
        return "BLOB"
    return "TEXT"


def ensure_stat_columns(conn: sqlite3.Connection, stats: dict[str, Any]) -> None:
    existing = get_table_columns(conn, "statistics")

    for key, value in stats.items():
        if key in existing:
            continue
        col_type = sqlite_type_for_value(value)
        conn.execute(f'ALTER TABLE statistics ADD COLUMN "{key}" {col_type}')

    conn.commit()


def normalize_sql_value(value: Any) -> Any:
    if isinstance(value, bool):
        return int(value)
    return value


def get_pending_fresh_run_ids(conn: sqlite3.Connection) -> list[int]:
    query = """
            WITH uncomputed AS (SELECT r.id,
                                       ROW_NUMBER() OVER (
                PARTITION BY r.output_dir
                ORDER BY r.started_at DESC, r.id DESC
            ) AS rn
                                FROM runs r
                                         LEFT JOIN statistics s ON s.run_id = r.id
                                WHERE s.run_id IS NULL)
            SELECT id
            FROM uncomputed
            WHERE rn = 1
            ORDER BY id \
            """
    rows = conn.execute(query).fetchall()
    return [row[0] for row in rows]


def get_fresh_run_ids_for_experiment(conn: sqlite3.Connection, experiment_name: str) -> list[int]:
    query = """
            WITH ranked AS (SELECT r.id,
                                   ROW_NUMBER() OVER (
                PARTITION BY r.output_dir
                ORDER BY r.started_at DESC, r.id DESC
            ) AS rn
                            FROM runs r
                            WHERE r.experiment = ?)
            SELECT id
            FROM ranked
            WHERE rn = 1
            ORDER BY id \
            """
    rows = conn.execute(query, (experiment_name,)).fetchall()
    return [row[0] for row in rows]


def insert_statistics_for_run(conn: sqlite3.Connection, run_id: int, stats: dict[str, Any]) -> None:
    ensure_stat_columns(conn, stats)

    row = {"run_id": run_id,
           "computed_at": utc_now_iso(),
           **{k: normalize_sql_value(v) for k, v in stats.items()}}

    columns = list(row.keys())
    col_sql = ", ".join(f'"{c}"' for c in columns)
    placeholders = ", ".join("?" for _ in columns)

    update_cols = [c for c in columns if c != "run_id"]
    update_sql = ", ".join(f'"{c}" = excluded."{c}"' for c in update_cols)

    sql = f"""
        INSERT INTO statistics ({col_sql})
        VALUES ({placeholders})
        ON CONFLICT(run_id) DO UPDATE SET
        {update_sql}
        """

    conn.execute(sql, [row[c] for c in columns])


def compute_statistics_for_run_ids(db_path: str, run_ids: list[int], compute_stats: COMPUTE_STATS_FUNC):
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row

    try:
        placeholders = ", ".join("?" for _ in run_ids)
        query = f"""
            SELECT *
            FROM runs
            WHERE id IN ({placeholders})
            ORDER BY started_at ASC, id ASC
            """
        runs = conn.execute(query, run_ids).fetchall()

        for run in runs:
            stats = compute_stats(run)
            if not isinstance(stats, dict):
                raise TypeError(f"compute_stats did not return a dict")
            with conn:
                insert_statistics_for_run(conn, run["id"], stats)
    finally:
        conn.close()

    return len(run_ids)


def compute_pending_stats(db_path: str, compute_stats: COMPUTE_STATS_FUNC):
    conn = sqlite3.connect(db_path)
    ensure_statistics_table(conn)

    try:
        run_ids = get_pending_fresh_run_ids(conn)
    finally:
        conn.close()

    return compute_statistics_for_run_ids(db_path, run_ids, compute_stats)


def recompute_experiment_stats(db_path: str, experiment_name: str, compute_stats: COMPUTE_STATS_FUNC):
    conn = sqlite3.connect(db_path)
    ensure_statistics_table(conn)

    try:
        run_ids = get_fresh_run_ids_for_experiment(conn, experiment_name)
    finally:
        conn.close()

    return compute_statistics_for_run_ids(db_path, run_ids, compute_stats)


__all__ = ["compute_statistics_for_run_ids", "compute_pending_stats", "recompute_experiment_stats"]
