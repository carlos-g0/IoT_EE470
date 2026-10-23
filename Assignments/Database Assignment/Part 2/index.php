<?php
// EE 470 - Database Assignment Part 2
// Connect to MariaDB using the private configuration file

require dirname(__DIR__, 2) . '/db_config.php';

try {

    // Connect to MariaDB
    $pdo = new PDO(
        "mysql:host=$dbHost;dbname=$dbName;charset=utf8mb4",
        $dbUser,
        $dbPass,
        [
            PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
            PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC
        ]
    );

    // Retrieve registered sensors
    $sql1 = "SELECT node_name, manufacturer,
                    longitude, latitude
             FROM sensor_register
             ORDER BY node_name";

    $sensors = $pdo->query($sql1)->fetchAll();

    // Retrieve readings sorted by node name and time
    $sql2 = "SELECT node_name, time_received,
                    temperature, humidity
             FROM sensor_data
             ORDER BY node_name, time_received";

    $readings = $pdo->query($sql2)->fetchAll();

} catch (PDOException $e) {

    error_log($e->getMessage());

    http_response_code(500);

    exit("Database connection failed. Check configuration.");
}

// Safely display database values
function displayValue($value) {
    return htmlspecialchars(
        (string)$value,
        ENT_QUOTES,
        'UTF-8'
    );
}
?>

<!DOCTYPE html>
<html lang="en">

<head>
    <meta charset="UTF-8">

    <meta name="viewport"
          content="width=device-width, initial-scale=1.0">

    <title>EE 470 - Sensor Database</title>

    <style>

        body {
            font-family: Arial, sans-serif;
            background-color: #f4f6f8;
            margin: 0;
            color: #333;
        }

        header {
            background-color: #183249;
            color: white;
            padding: 30px;
            text-align: center;
        }

        main {
            max-width: 1000px;
            margin: 30px auto;
            padding: 25px;
            background: white;
            border-radius: 10px;
        }

        h2 {
            color: #183249;
        }

        h3 {
            color: #183249;
        }

        .table-container {
            overflow-x: auto;
            margin-bottom: 35px;
        }

        table {
            width: 100%;
            border-collapse: collapse;
        }

        th, td {
            border: 1px solid #ddd;
            padding: 12px;
            text-align: left;
        }

        th {
            background-color: #183249;
            color: white;
        }

        tr:nth-child(even) {
            background-color: #f2f2f2;
        }

        /* Sensor dropdown */

        #nodeSelect {
            padding: 10px;
            font-size: 16px;
            border: 1px solid #ccc;
            border-radius: 6px;
            margin-top: 10px;
            margin-bottom: 20px;
            min-width: 180px;
        }

        /* Statistics container */

        #sensorStats {
            background-color: #f8fafc;
            padding: 20px;
            border: 1px solid #dce4eb;
            border-radius: 10px;
            margin-bottom: 35px;
        }

        #sensorStats p {
            margin: 12px 0;
        }

        .stats-grid {
            display: grid;
            grid-template-columns:
                repeat(auto-fit, minmax(240px, 1fr));
            gap: 20px;
        }

        .stats-group {
            padding: 10px;
        }

        /* Temperature graph */

        .chart-container {
            position: relative;
            width: 100%;
            height: 400px;
            margin-top: 20px;
        }

        #chartStatus {
            margin-top: 15px;
            color: #666;
            font-size: 14px;
        }

    </style>

</head>

<body>

<header>

    <h1>EE 470 - Sensor Database</h1>

    <p>Database Assignment - Part 2</p>

</header>

<main>

    <!-- Registered Sensor Nodes -->

    <h2>Registered Sensor Nodes</h2>

    <div class="table-container">

        <table>

            <thead>
                <tr>
                    <th>Node Name</th>
                    <th>Manufacturer</th>
                    <th>Longitude</th>
                    <th>Latitude</th>
                </tr>
            </thead>

            <tbody>

                <?php foreach ($sensors as $sensor): ?>

                <tr>
                    <td>
                        <?= displayValue($sensor['node_name']) ?>
                    </td>

                    <td>
                        <?= displayValue($sensor['manufacturer']) ?>
                    </td>

                    <td>
                        <?= displayValue($sensor['longitude']) ?>
                    </td>

                    <td>
                        <?= displayValue($sensor['latitude']) ?>
                    </td>
                </tr>

                <?php endforeach; ?>

            </tbody>

        </table>

    </div>


    <!-- Sensor Data Table -->

    <h2>Sensor Data Received</h2>

    <div class="table-container">

        <table>

            <thead>
                <tr>
                    <th>Node Name</th>
                    <th>Time Received</th>
                    <th>Temperature (°C)</th>
                    <th>Humidity (%)</th>
                </tr>
            </thead>

            <tbody id="sensorDataRows">

                <?php foreach ($readings as $reading): ?>

                <tr>
                    <td>
                        <?= displayValue($reading['node_name']) ?>
                    </td>

                    <td>
                        <?= displayValue($reading['time_received']) ?>
                    </td>

                    <td>
                        <?= displayValue($reading['temperature']) ?>
                    </td>

                    <td>
                        <?= displayValue($reading['humidity']) ?>
                    </td>
                </tr>

                <?php endforeach; ?>

            </tbody>

        </table>

    </div>


    <!-- Sensor Selection Dropdown -->

    <h2>Select Sensor Node</h2>

    <label for="nodeSelect">
        Choose a sensor:
    </label>

    <select id="nodeSelect">

        <?php foreach ($sensors as $sensor): ?>

            <?php $node = $sensor['node_name']; ?>

            <option
                value="<?= displayValue($node) ?>"
                <?= $node === 'node_1' ? 'selected' : '' ?>
            >
                <?= displayValue($node) ?>
            </option>

        <?php endforeach; ?>

    </select>


    <!-- Sensor Statistics -->

    <h2>
        Sensor Statistics -
        <span id="selectedNode">node_1</span>
    </h2>

    <div id="sensorStats">

        <div class="stats-grid">

            <!-- Temperature Statistics -->

            <div class="stats-group">

                <h3>Temperature Statistics</h3>

                <p>
                    Average Temperature:
                    <strong>
                        <span id="avgTemp">--</span> °C
                    </strong>
                </p>

                <p>
                    Minimum Temperature:
                    <strong>
                        <span id="minTemp">--</span> °C
                    </strong>
                </p>

                <p>
                    Maximum Temperature:
                    <strong>
                        <span id="maxTemp">--</span> °C
                    </strong>
                </p>

            </div>


            <!-- Humidity Statistics -->

            <div class="stats-group">

                <h3>Humidity Statistics</h3>

                <p>
                    Average Humidity:
                    <strong>
                        <span id="avgHumidity">--</span> %
                    </strong>
                </p>

                <p>
                    Minimum Humidity:
                    <strong>
                        <span id="minHumidity">--</span> %
                    </strong>
                </p>

                <p>
                    Maximum Humidity:
                    <strong>
                        <span id="maxHumidity">--</span> %
                    </strong>
                </p>

            </div>

        </div>

    </div>


    <!-- Live Sensor Temperature Graph -->

    <h2>Sensor Temperature Graph</h2>

    <p>
        Temperature readings recorded by
        <span id="graphNodeName">node_1</span>.
    </p>

    <div class="chart-container">

        <canvas id="sensorChart"></canvas>

    </div>

    <p id="chartStatus" role="status">
        Loading sensor readings...
    </p>

</main>


<!-- Chart.js Library -->

<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>


<!-- Our JavaScript File -->

<script src="app.js?v=7"></script>

</body>

</html>
