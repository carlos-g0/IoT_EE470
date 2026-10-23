
<?php
// Connect to the database using the private config file
require dirname(__DIR__, 2) . '/db_config.php';

try {
    $pdo = new PDO(
        "mysql:host=$dbHost;dbname=$dbName;charset=utf8mb4",
        $dbUser,
        $dbPass,
        [
            PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
            PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC
        ]
    );

    // Get registered sensor information
    $sql1 = "SELECT node_name, manufacturer, longitude, latitude
             FROM sensor_register
             ORDER BY node_name";

    $sensors = $pdo->query($sql1)->fetchAll();

    // Get readings sorted by node name and time
    $sql2 = "SELECT node_name, time_received, temperature, humidity
             FROM sensor_data
             ORDER BY node_name, time_received";

    $readings = $pdo->query($sql2)->fetchAll();
    
    
    // Calculate average temperature and humidity for node_1
    $avgSQL = "SELECT
                   AVG(temperature) AS avg_temperature,
                   AVG(humidity) AS avg_humidity
               FROM sensor_data
               WHERE node_name = 'node_1'";

    $averages = $pdo->query($avgSQL)->fetch();


} catch (PDOException $e) {
    error_log($e->getMessage());
    http_response_code(500);
    exit("Database connection failed. Check the configuration.");
}

// Safely display text from the database
function displayValue($value) {
    return htmlspecialchars((string)$value, ENT_QUOTES, 'UTF-8');
}
?>

<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
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
    </style>
</head>

<body>

<header>
    <h1>EE 470 - Sensor Database</h1>
    <p>Database Assignment - Part 2</p>
</header>

<main>

    <h2>Registered Sensor Nodes</h2>

    <div class="table-container">
        <table>
            <tr>
                <th>Node Name</th>
                <th>Manufacturer</th>
                <th>Longitude</th>
                <th>Latitude</th>
            </tr>

            <?php foreach ($sensors as $sensor): ?>
            <tr>
                <td><?= displayValue($sensor['node_name']) ?></td>
                <td><?= displayValue($sensor['manufacturer']) ?></td>
                <td><?= displayValue($sensor['longitude']) ?></td>
                <td><?= displayValue($sensor['latitude']) ?></td>
            </tr>
            <?php endforeach; ?>
        </table>
    </div>

    <h2>Sensor Data Received</h2>

    <div class="table-container">
        <table>
            <tr>
                <th>Node Name</th>
                <th>Time Received</th>
                <th>Temperature (C)</th>
                <th>Humidity (%)</th>
            </tr>

            <?php foreach ($readings as $reading): ?>
            <tr>
                <td><?= displayValue($reading['node_name']) ?></td>
                <td><?= displayValue($reading['time_received']) ?></td>
                <td><?= displayValue($reading['temperature']) ?></td>
                <td><?= displayValue($reading['humidity']) ?></td>
            </tr>
            <?php endforeach; ?>
        </table>
    </div>
    
    <h2>Average Sensor Readings - node_1</h2>

    <p>
        <strong>Average Temperature:</strong>
        <?= number_format((float)$averages['avg_temperature'], 2) ?> &deg;C
    </p>

    <p>
        <strong>Average Humidity:</strong>
        <?= number_format((float)$averages['avg_humidity'], 2) ?>%
    </p>


    <h2>Sensor Temperature Graph</h2>

    <p>Temperature readings recorded by node_1.</p>

    <div style="position: relative; height: 400px; width: 100%;">
        <canvas id="sensorChart"></canvas>
    </div>

    <p id="chartStatus"></p>

</main>

<script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
<script src="app.js?v=2"></script>

</body>
</html>
