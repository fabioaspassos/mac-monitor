"""Configuração do agente em ~/.config/mac-monitor/config.toml (nenhum segredo aqui).

Sem o arquivo valem os padrões abaixo. O TOML é lido com tomllib (Python 3.11+)
ou com o pacote tomli nas versões anteriores.
"""

import copy
import logging
import os

CONFIG_PATH = os.path.expanduser("~/.config/mac-monitor/config.toml")

log = logging.getLogger(__name__)

DEFAULTS = {
    "claude": {
        "enabled": True,
        "projects_dir": "~/.claude/projects",  # CLAUDE_CONFIG_DIR, se definido, tem prioridade
        "status_cache_dir": "~/Library/Caches/mac-monitor/claude",
        "send_interval_s": 10,
        "scan_interval_s": 5,
        "active_window_min": 5,
        "rate_window_min": 5,
        "count_cache_read": False,
        # Endpoint de uso do Claude Code (não documentado; token OAuth lido do Keychain).
        # Desligado: o percentual oficial vem só do status line.
        "oauth_usage": True,
        "oauth_interval_s": 120,
    },
}


def _load_toml(path):
    try:
        import tomllib
    except ImportError:
        try:
            import tomli as tomllib
        except ImportError:
            log.warning("%s ignorado (instale tomli ou use Python 3.11+)", path)
            return {}
    with open(path, "rb") as f:
        return tomllib.load(f)


def load_config(path=CONFIG_PATH):
    config = copy.deepcopy(DEFAULTS)
    if not os.path.exists(path):
        return config
    try:
        data = _load_toml(path)
    except Exception as e:
        log.warning("erro ao ler %s (%s), usando os padrões", path, e)
        return config
    for section, values in data.items():
        if isinstance(values, dict) and isinstance(config.get(section), dict):
            config[section].update(values)
    return config
