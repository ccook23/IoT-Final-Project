from flask import Flask, request, jsonify
import sqlite3
from datetime import datetime

app = Flask(__name__)

DB_FILE = "thermostat.db"


def get_db():
    conn = sqlite3.connect(DB_FILE)
    conn.row_factory = sqlite3.Row
    return conn


def init_db():
    conn = get_db()

    conn.execute("""
        CREATE TABLE IF NOT EXISTS readings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            device_id TEXT NOT NULL,
            temperature REAL NOT NULL,
            heater TEXT NOT NULL,
            timestamp TEXT NOT NULL
        )
    """)

    conn.execute("""
        CREATE TABLE IF NOT EXISTS programs (
            device_id TEXT PRIMARY KEY,
            target_temp REAL NOT NULL,
            mode TEXT NOT NULL,
            updated_at TEXT NOT NULL
        )
    """)

    # Default thermostat program.
    conn.execute("""
        INSERT OR IGNORE INTO programs
        (device_id, target_temp, mode, updated_at)
        VALUES (?, ?, ?, ?)
    """, ("thermostat1", 72.0, "auto", datetime.utcnow().isoformat()))

    conn.commit()
    conn.close()


@app.route("/", methods=["GET"])
def home():
    return jsonify({
        "message": "Thermostat cloud server is running"
    })


@app.route("/api/status", methods=["POST"])
def receive_status():
    data = request.get_json()

    if not data:
        return jsonify({"error": "Missing JSON body"}), 400

    device_id = data.get("device_id")
    temperature = data.get("temperature")
    heater = data.get("heater")

    if device_id is None or temperature is None or heater is None:
        return jsonify({
            "error": "device_id, temperature, and heater are required"
        }), 400

    timestamp = datetime.utcnow().isoformat()

    conn = get_db()
    conn.execute("""
        INSERT INTO readings
        (device_id, temperature, heater, timestamp)
        VALUES (?, ?, ?, ?)
    """, (device_id, float(temperature), heater, timestamp))
    conn.commit()
    conn.close()

    return jsonify({
        "message": "status stored",
        "device_id": device_id,
        "temperature": temperature,
        "heater": heater,
        "timestamp": timestamp
    })


@app.route("/api/program/<device_id>", methods=["GET"])
def get_program(device_id):
    conn = get_db()
    row = conn.execute("""
        SELECT device_id, target_temp, mode, updated_at
        FROM programs
        WHERE device_id = ?
    """, (device_id,)).fetchone()
    conn.close()

    if row is None:
        return jsonify({
            "device_id": device_id,
            "target_temp": 72.0,
            "mode": "auto"
        })

    return jsonify(dict(row))


@app.route("/api/program/<device_id>", methods=["PUT"])
def update_program(device_id):
    data = request.get_json()

    if not data:
        return jsonify({"error": "Missing JSON body"}), 400

    target_temp = data.get("target_temp")
    mode = data.get("mode", "auto")

    if target_temp is None:
        return jsonify({"error": "target_temp is required"}), 400

    timestamp = datetime.utcnow().isoformat()

    conn = get_db()
    conn.execute("""
        INSERT INTO programs
        (device_id, target_temp, mode, updated_at)
        VALUES (?, ?, ?, ?)
        ON CONFLICT(device_id)
        DO UPDATE SET
            target_temp = excluded.target_temp,
            mode = excluded.mode,
            updated_at = excluded.updated_at
    """, (device_id, float(target_temp), mode, timestamp))
    conn.commit()
    conn.close()

    return jsonify({
        "message": "program updated",
        "device_id": device_id,
        "target_temp": target_temp,
        "mode": mode,
        "updated_at": timestamp
    })


@app.route("/api/latest/<device_id>", methods=["GET"])
def latest_reading(device_id):
    conn = get_db()
    row = conn.execute("""
        SELECT device_id, temperature, heater, timestamp
        FROM readings
        WHERE device_id = ?
        ORDER BY id DESC
        LIMIT 1
    """, (device_id,)).fetchone()
    conn.close()

    if row is None:
        return jsonify({"error": "No readings found"}), 404

    return jsonify(dict(row))


if __name__ == "__main__":
    init_db()
    app.run(host="0.0.0.0", port=3000)
