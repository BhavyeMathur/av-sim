from __future__ import annotations
from typing import Any

import re
from copy import deepcopy
from itertools import product
from pathlib import Path
import shutil

import yaml


def get_by_path(data: dict[str, Any], path: str):
    """
    Returns the value located at a dot-separated nested key `path` inside a nested dictionary `data`.
    For example, data = {"fleet": {"fleet_size": 15000}} and path = "fleet.fleet_size" returns 15000.
    """
    cur = data
    for part in path.split("."):
        if not isinstance(cur, dict) or part not in cur:
            raise KeyError(path)
        cur = cur[part]
    return cur


def has_path(data: dict[str, Any], path: str) -> bool:
    """
    Checks whether a dot-separated nested key `path` exists inside a nested dictionary `data`.
    For example, data = {"fleet": {"fleet_size": 15000}} and path = "fleet.fleet_size" returns 15000.
    """
    try:
        get_by_path(data, path)
        return True
    except KeyError:
        return False


def set_by_path(data: dict[str, Any], path: str, value) -> None:
    """
    Sets the value at a dot-separated nested key `path` inside a nested dictionary `data`.
    """
    parts = path.split(".")
    cur = data
    for part in parts[:-1]:
        if part not in cur or not isinstance(cur[part], dict):
            cur[part] = {}
        cur = cur[part]
    cur[parts[-1]] = value


def delete_by_path(data: dict[str, Any], path: str) -> None:
    """
    Deletes the value at a dot-separated nested key `path` inside a nested dictionary `data`.
    """
    parts = path.split(".")
    cur = data
    for part in parts[:-1]:
        if part not in cur or not isinstance(cur[part], dict):
            return
        cur = cur[part]
    cur.pop(parts[-1], None)


# node type detectors
#
# a base config is just a single fixed value.
#
# a sweep node is a configuration of the form
#   fleet:
#       fleet_size:
#           values: [ 10000, 12500, 15000 ]
#
# while a template is something like
#   riders_file:
#       template: "data/riders/ann-arbor-{fleet.fleet_size}.parquet"

def is_sweep_node(obj) -> bool:
    return (isinstance(obj, dict)
            and "values" in obj
            and isinstance(obj["values"], list))


def is_template_node(obj) -> bool:
    return (isinstance(obj, dict)
            and "template" in obj
            and isinstance(obj["template"], str))


def collect_spec(raw: dict[str, Any]) -> tuple[dict[str, Any], list[dict[str, Any]], list[dict[str, Any]]]:
    """
    Returns:
      base_config
      sweep_specs: [{path, values, default, only_if}]
      template_specs: [{path, template}]
    """
    base: dict[str, Any] = {}
    sweeps: list[dict[str, Any]] = []
    templates: list[dict[str, Any]] = []

    def walk(obj: Any, path_prefix: str = "") -> None:
        if not isinstance(obj, dict):
            if path_prefix:
                set_by_path(base, path_prefix, deepcopy(obj))
            else:
                raise ValueError("Root YAML must be a mapping")
            return

        # special case with dotted keys at any level
        for key, value in obj.items():
            path = f"{path_prefix}.{key}" if path_prefix else key

            if is_sweep_node(value):
                sweeps.append({
                    "path": path,
                    "values": deepcopy(value["values"]),
                    "default": deepcopy(value.get("default")),
                    "only_if": deepcopy(value.get("only_if")),
                })
            elif is_template_node(value):
                templates.append({
                    "path": path,
                    "template": value["template"],
                })
            elif isinstance(value, dict):
                walk(value, path)
            else:
                set_by_path(base, path, deepcopy(value))

    walk(raw)
    return base, sweeps, templates


def condition_matches(config: dict[str, Any], only_if: dict[str, list[Any]] | None) -> bool:
    if not only_if:
        return True

    for cond_path, allowed_values in only_if.items():
        if not has_path(config, cond_path):
            return False
        current = get_by_path(config, cond_path)
        if current not in allowed_values:
            return False

    return True


_TEMPLATE_RE = re.compile(r"\{([^{}]+)\}")


def render_template(template: str, context: dict[str, Any]) -> str:
    def replace(match: re.Match[str]) -> str:
        path = match.group(1).strip()
        try:
            value = get_by_path(context, path)
        except KeyError as e:
            raise KeyError(f"Template placeholder {{{path}}} could not be resolved") from e
        return str(value)

    return _TEMPLATE_RE.sub(replace, template)


def expand_experiment(raw: dict[str, Any]) -> list[dict[str, Any]]:
    base, sweeps, templates = collect_spec(raw)

    # First pass: determine which sweeps are unconditional vs conditional.
    # We only immediately classify unconditional ones.
    unconditional_sweeps = [s for s in sweeps if not s["only_if"]]
    conditional_sweeps = [s for s in sweeps if s["only_if"]]

    # Expand over unconditional sweeps first
    if unconditional_sweeps:
        unconditional_paths = [s["path"] for s in unconditional_sweeps]
        unconditional_values = [s["values"] for s in unconditional_sweeps]
        partial_configs = []

        for combo in product(*unconditional_values):
            cfg = deepcopy(base)
            for path, value in zip(unconditional_paths, combo):
                set_by_path(cfg, path, value)
            partial_configs.append(cfg)
    else:
        partial_configs = [deepcopy(base)]

    resolved_configs: list[dict[str, Any]] = []

    # For each partial config, decide which conditional sweeps are active
    for partial in partial_configs:
        currently_active = []
        currently_inactive = []

        for sweep in conditional_sweeps:
            if condition_matches(partial, sweep["only_if"]):
                currently_active.append(sweep)
            else:
                currently_inactive.append(sweep)

        cfg = deepcopy(partial)

        def sweep_inactive():
            for sweep in currently_inactive:
                if "default" in sweep and sweep["default"] is not None:
                    set_by_path(cfg, sweep["path"], deepcopy(sweep["default"]))
                else:
                    delete_by_path(cfg, sweep["path"])

            for tpl in templates:
                rendered = render_template(tpl["template"], cfg)
                set_by_path(cfg, tpl["path"], rendered)

            resolved_configs.append(cfg)

        if currently_active:
            active_paths = [s["path"] for s in currently_active]
            active_values = [s["values"] for s in currently_active]

            for combo in product(*active_values):
                for path, value in zip(active_paths, combo):
                    set_by_path(cfg, path, value)
                sweep_inactive()

        else:
            sweep_inactive()

    return resolved_configs


def load_experiment_yaml(path: str | Path) -> dict[str, Any]:
    with open(path, "r") as f:
        raw = yaml.safe_load(f)
    if not isinstance(raw, dict):
        raise ValueError("Experiment YAML root must be a mapping")
    return raw


def generate_runs_from_experiment_yaml(experiment_yaml_path: str | Path,
                                       output_root: str | Path = "runs") -> list[dict[str, Any]]:
    experiment_yaml_path = Path(experiment_yaml_path)
    experiment_name = experiment_yaml_path.stem
    raw = load_experiment_yaml(experiment_yaml_path)

    base_dir = Path(output_root) / experiment_name
    if base_dir.exists():
        shutil.rmtree(base_dir)

    runs = []
    for i, config in enumerate(expand_experiment(raw)):
        run_name = f"run_{i:04d}"
        run_dir = base_dir / run_name

        run_dir.mkdir(parents=True, exist_ok=True)
        (run_dir / "raw").mkdir(parents=True, exist_ok=True)
        (run_dir / "derived").mkdir(parents=True, exist_ok=True)

        config["sim"]["output"] = str(run_dir / "raw/")

        config_path = run_dir / "config.yaml"
        with open(config_path, "w") as f:
            yaml.safe_dump(config, f, sort_keys=False)

        runs.append({"run_name": run_name, "config_path": config_path, "config": config})

    return runs


__all__ = ["generate_runs_from_experiment_yaml"]
