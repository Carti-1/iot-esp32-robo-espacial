"""API de ingestao e consulta das leituras do robo explorador."""

import os
import sqlite3
from datetime import datetime, timezone
from pathlib import Path

from flask import Flask, jsonify, request


BASE_DIR = Path(__file__).resolve().parent
DB_PATH = Path(os.environ.get("LEITURAS_DB", BASE_DIR / "leituras.db"))
SCHEMA_PATH = BASE_DIR.parent / "sql" / "schema.sql"

app = Flask(__name__)


def conectar_banco():
    con = sqlite3.connect(DB_PATH, timeout=10)
    con.row_factory = sqlite3.Row
    con.execute("PRAGMA foreign_keys = ON")
    return con


def inicializar_banco():
    DB_PATH.parent.mkdir(parents=True, exist_ok=True)
    with conectar_banco() as con:
        con.executescript(SCHEMA_PATH.read_text(encoding="utf-8"))


def validar_leitura(dados):
    if not isinstance(dados, dict):
        return None, "O corpo da requisicao deve ser um objeto JSON."

    campos = ("temperatura_c", "umidade_pct", "luminosidade", "presenca", "probabilidade_vida")
    faltando = [campo for campo in campos if campo not in dados]
    if faltando:
        return None, f"Campos obrigatorios ausentes: {', '.join(faltando)}."

    try:
        temperatura = float(dados["temperatura_c"])
        umidade = float(dados["umidade_pct"])
        luminosidade = int(dados["luminosidade"])
        presenca = int(dados["presenca"])
        probabilidade = float(dados["probabilidade_vida"])
    except (TypeError, ValueError, OverflowError):
        return None, "Valores numericos invalidos nos campos da leitura."

    if not all(map(lambda x: x == x and abs(x) != float("inf"), (temperatura, umidade, probabilidade))):
        return None, "Valores numericos devem ser finitos."
    if not 0 <= umidade <= 100:
        return None, "umidade_pct deve estar entre 0 e 100."
    if luminosidade < 0:
        return None, "luminosidade nao pode ser negativa."
    if presenca not in (0, 1):
        return None, "presenca deve ser 0 ou 1."
    if not 0 <= probabilidade <= 100:
        return None, "probabilidade_vida deve estar entre 0 e 100."

    timestamp = dados.get("timestamp")
    if timestamp is None:
        timestamp = datetime.now(timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z")
    elif not isinstance(timestamp, str) or not timestamp.strip():
        return None, "timestamp deve ser uma string ISO 8601."
    else:
        try:
            datetime.fromisoformat(timestamp.strip().replace("Z", "+00:00"))
        except ValueError:
            return None, "timestamp deve estar no formato ISO 8601."
        timestamp = timestamp.strip()

    return {
        "timestamp": timestamp,
        "temperatura_c": temperatura,
        "umidade_pct": umidade,
        "luminosidade": luminosidade,
        "presenca": presenca,
        "probabilidade_vida": probabilidade,
    }, None


@app.post("/leituras")
def criar_leitura():
    leitura, erro = validar_leitura(request.get_json(silent=True))
    if erro:
        return jsonify({"erro": erro}), 400

    with conectar_banco() as con:
        cursor = con.execute(
            """INSERT INTO leituras
               (timestamp, temperatura_c, umidade_pct, luminosidade, presenca, probabilidade_vida)
               VALUES (?, ?, ?, ?, ?, ?)""",
            tuple(leitura[campo] for campo in (
                "timestamp", "temperatura_c", "umidade_pct", "luminosidade", "presenca", "probabilidade_vida"
            )),
        )
        leitura_id = cursor.lastrowid

    return jsonify({"mensagem": "Leitura armazenada.", "id": leitura_id}), 201


@app.get("/leituras")
def listar_leituras():
    with conectar_banco() as con:
        rows = con.execute(
            """SELECT id, timestamp, temperatura_c, umidade_pct, luminosidade, presenca, probabilidade_vida
               FROM leituras ORDER BY id DESC LIMIT 100"""
        ).fetchall()
    return jsonify([dict(row) for row in rows])


@app.get("/health")
def health():
    return jsonify({"status": "ok"})


if __name__ == "__main__":
    inicializar_banco()
    app.run(host="0.0.0.0", port=int(os.environ.get("PORT", "5000")), debug=False)
