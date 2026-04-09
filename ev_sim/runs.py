import sqlite3
import pandas as pd


class RunDB:
    def __init__(self, path: str):
        self.path = path

    def connect(self):
        conn = sqlite3.connect(self.path)
        conn.row_factory = sqlite3.Row
        return conn

    def query(self):
        return _Query(self)


class _Query:
    def __init__(self, db: RunDB):
        self.db = db
        self._experiment = None
        self._filters = []

    # ----------------------------
    # builder methods
    # ----------------------------

    def experiment(self, name: str):
        self._experiment = name
        return self

    def where_config(self, key: str, op: str, value):
        self._filters.append((key, op, value))
        return self

    # ----------------------------
    # main execution
    # ----------------------------

    def load(self) -> pd.DataFrame:
        where = []
        params = []

        if self._experiment is not None:
            where.append("r.experiment = ?")
            params.append(self._experiment)

        for i, (key, op, value) in enumerate(self._filters):
            alias = f"rc{i}"

            if op in (">", ">=", "<", "<=", "between"):
                col = f"CAST({alias}.value AS REAL)"
            else:
                col = f"{alias}.value"

            if op == "between":
                lo, hi = value
                clause = f"""
                EXISTS (
                    SELECT 1 FROM run_configs {alias}
                    WHERE {alias}.run_id = r.id
                      AND {alias}.key = ?
                      AND {col} BETWEEN ? AND ?
                )
                """
                params.extend([key, lo, hi])

            elif op == "in":
                placeholders = ",".join("?" for _ in value)
                clause = f"""
                EXISTS (
                    SELECT 1 FROM run_configs {alias}
                    WHERE {alias}.run_id = r.id
                      AND {alias}.key = ?
                      AND {col} IN ({placeholders})
                )
                """
                params.append(key)
                params.extend(list(value))

            else:
                clause = f"""
                EXISTS (
                    SELECT 1 FROM run_configs {alias}
                    WHERE {alias}.run_id = r.id
                      AND {alias}.key = ?
                      AND {col} {op} ?
                )
                """
                params.extend([key, value])

            where.append(clause)

        sql = """
              SELECT r.id, \
                     r.duration_ms, \
                     r.output_dir, \
                     s.*
              FROM runs r
                       LEFT JOIN statistics s ON s.run_id = r.id \
              """

        if where:
            sql += " WHERE " + " AND ".join(where)

        with self.db.connect() as conn:
            df = pd.read_sql_query(sql, conn, params=params)
            cfg = pd.read_sql_query(
                "SELECT run_id, key, value FROM run_configs",
                conn,
            )

        if not cfg.empty:
            cfg_wide = (
                cfg.pivot_table(
                    index="run_id",
                    columns="key",
                    values="value",
                    aggfunc="first",
                )
                .reset_index()
            )
            cfg_wide.columns.name = None

            df = df.merge(cfg_wide, left_on="id", right_on="run_id", how="left")
            df.index = df.id

            drop_cols = [c for c in ["run_id_y", "run_id", "run_id_x", "computed_at", "id"] if c in df.columns]
            if drop_cols:
                df = df.drop(columns=drop_cols)

        return df


__all__ = ["RunDB"]
