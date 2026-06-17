from pathlib import Path

Import("env")


def parse_env_file(env_path: Path) -> dict:
    values = {}
    if not env_path.exists():
        return values

    for raw_line in env_path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        key = key.strip()
        value = value.strip()
        if (value.startswith('"') and value.endswith('"')) or (
            value.startswith("'") and value.endswith("'")
        ):
            value = value[1:-1]
        values[key] = value
    return values


def to_build_flag(value: str) -> str:
    escaped = value.replace("\\", "\\\\").replace('"', '\\"')
    return f'\\"{escaped}\\"'


project_dir = Path(env["PROJECT_DIR"])
env_file = project_dir / ".env"
env_values = parse_env_file(env_file)

mapping = {
    "WIFI_SSID": "CAT_WHEEL_WIFI_SSID",
    "WIFI_PASSWORD": "CAT_WHEEL_WIFI_PASSWORD",
    "INFLUX_BASE_URL": "CAT_WHEEL_INFLUX_BASE_URL",
    "INFLUX_ORG": "CAT_WHEEL_INFLUX_ORG",
    "INFLUX_BUCKET": "CAT_WHEEL_INFLUX_BUCKET",
    "INFLUX_TOKEN": "CAT_WHEEL_INFLUX_TOKEN",
}

for env_key, define_name in mapping.items():
    value = env_values.get(env_key, "")
    env.Append(BUILD_FLAGS=[f"-D{define_name}={to_build_flag(value)}"])
