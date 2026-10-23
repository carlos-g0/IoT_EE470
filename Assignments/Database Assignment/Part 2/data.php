
<?php
// EE 470 - Database Assignment Part 2
// Retrieve sensor readings and return them as JSON

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');

// Load our database connection information
require dirname(__DIR__, 2) . '/db_config.php';

try {
    // Connect to MariaDB
    $pdo = new PDO(
        "mysql:host=$dbHost;dbname=$dbName;charset=utf8mb4",
        $dbUser,
        $dbPass,
        [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]
    );

    // Retrieve data from node_1, sorted by time
    $sql = "SELECT node_name, time_received,
                   temperature, humidity
            FROM sensor_data
            WHERE node_name = ?
            ORDER BY time_received ASC";

    $stmt = $pdo->prepare($sql);
    $stmt->execute(['node_1']);

    $data = $stmt->fetchAll(PDO::FETCH_ASSOC);

    // Convert measurements to numeric values
    foreach ($data as &$reading) {
        $reading['temperature'] = (float)$reading['temperature'];
        $reading['humidity'] = (float)$reading['humidity'];
    }
    unset($reading);

    // Return the readings as JSON
    echo json_encode($data);

} catch (PDOException $e) {
    error_log($e->getMessage());
    http_response_code(500);
    echo json_encode(['error' => 'Unable to retrieve sensor data']);
}
?>
