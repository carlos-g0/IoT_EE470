<?php
// EE 470 - Database Assignment Part 2
// Retrieve all sensor readings for the live table

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');

// Load private database configuration
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

    // Retrieve all readings
    // Sort first by node name, then timestamp
    $sql = "SELECT node_name, time_received,
                   temperature, humidity
            FROM sensor_data
            ORDER BY node_name ASC, time_received ASC";

    $readings = $pdo->query($sql)->fetchAll();

    // Return the readings as JSON
    echo json_encode($readings);

} catch (PDOException $e) {

    error_log($e->getMessage());

    http_response_code(500);

    echo json_encode([
        'error' => 'Unable to retrieve sensor readings'
    ]);
}
?>
