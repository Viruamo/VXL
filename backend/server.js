require('dotenv').config();
const express = require('express');
const http = require('http');
const socketIo = require('socket.io');
const mqtt = require('mqtt');
const cors = require('cors');
const pool = require('./db');

const app = express();
const server = http.createServer(app);
const io = socketIo(server, {
  cors: { origin: '*' }
});

app.use(cors());
app.use(express.json());

// MQTT client
const mqttClient = mqtt.connect(process.env.MQTT_BROKER || 'mqtt://localhost');
mqttClient.on('connect', () => {
  console.log('MQTT connected');
  mqttClient.subscribe('e-nose/+/data');
});

mqttClient.on('message', async (topic, msg) => {
  try {
    const data = JSON.parse(msg);
    const deviceId = data.device_id;
    // Tìm device_id trong DB, nếu chưa có thì insert
    const devRes = await pool.query(
      'INSERT INTO devices(device_id) VALUES($1) ON CONFLICT DO NOTHING RETURNING id',
      [deviceId]
    );
    let devDbId = devRes.rows[0]?.id;
    if (!devDbId) {
      const getDev = await pool.query('SELECT id FROM devices WHERE device_id=$1', [deviceId]);
      devDbId = getDev.rows[0].id;
    }

    const ts = data.ts ? new Date(data.ts * 1000).toISOString() : new Date().toISOString();
    const gasValues = JSON.stringify(data.gas || []);
    const result = await pool.query(
      `INSERT INTO measurements(device_id, ts, temperature, humidity, gas_values)
       VALUES ($1, $2, $3, $4, $5) RETURNING id, ts, temperature, humidity, gas_values`,
      [devDbId, ts, data.temp, data.hum, gasValues]
    );
    const newRow = result.rows[0];
    // Phát sự kiện realtime
    io.emit('new_measurement', newRow);
  } catch (e) {
    console.error('MQTT message error:', e);
  }
});

// REST API
app.get('/api/measurements', async (req, res) => {
  try {
    const { device_id, from, to, page = 1, limit = 50 } = req.query;
    let query = `SELECT id, device_id, ts, temperature, humidity, gas_values, note FROM measurements WHERE 1=1`;
    const params = [];
    let paramIdx = 1;

    if (device_id) {
      query += ` AND device_id=(SELECT id FROM devices WHERE device_id=$${paramIdx})`;
      params.push(device_id);
      paramIdx++;
    }
    if (from) {
      query += ` AND ts >= $${paramIdx}`;
      params.push(from);
      paramIdx++;
    }
    if (to) {
      query += ` AND ts <= $${paramIdx}`;
      params.push(to);
      paramIdx++;
    }
    query += ` ORDER BY ts DESC LIMIT $${paramIdx} OFFSET $${paramIdx+1}`;
    params.push(limit, (page-1)*limit);

    const result = await pool.query(query, params);
    res.json(result.rows);
  } catch (err) {
    console.error(err);
    res.status(500).json({ error: 'Internal server error' });
  }
});

app.get('/api/measurements/:id', async (req, res) => {
  try {
    const { id } = req.params;
    const result = await pool.query('SELECT * FROM measurements WHERE id=$1', [id]);
    if (result.rows.length === 0) return res.status(404).json({ error: 'Not found' });
    res.json(result.rows[0]);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

app.patch('/api/measurements/:id', async (req, res) => {
  try {
    const { id } = req.params;
    const { note } = req.body;
    await pool.query('UPDATE measurements SET note=$1 WHERE id=$2', [note, id]);
    res.json({ success: true });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

app.get('/api/devices', async (req, res) => {
  try {
    const result = await pool.query('SELECT * FROM devices ORDER BY created_at DESC');
    res.json(result.rows);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

const PORT = process.env.PORT || 3000;
server.listen(PORT, () => {
  console.log(`Backend running on port ${PORT}`);
});
