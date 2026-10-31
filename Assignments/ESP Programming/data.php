<?php
// EE 470 - Database Assignment Part 2
// Retrieve readings for the selected sensor node

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');

// Load database connection information
require dirname(__DIR__, 2) . '/db_config.php';

// Get the selected node from the URL
// Use node_1 if no node is specified
$selectedNode = $_GET['node'] ?? 'node_1';

// Validate the node name
if (!is_string($selectedNode) ||
    !preg_match('/^node_[1-9][0-9]*$/', $selectedNode)) {

    http_response_code(400);
    echo json_encode(['error' => 'Invalid sensor node']);
    exit;
}

try {

    // Connect to MariaDB
    $pdo = new PDO(
        "mysql:host=$dbHost;dbname=$dbName;charset=utf8mb4",
        $dbUser,
        $dbPass,
        [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]
    );

    // Retrieve readings for the selected sensor
    $sql = "SELECT node_name, time_received,
                   temperature, humidity
            FROM sensor_data
            WHERE node_name = ?
            ORDER BY time_received ASC";

    $stmt = $pdo->prepare($sql);

    // Use the selected node instead of fixed node_1
    $stmt->execute([$selectedNode]);

    $data = $stmt->fetchAll(PDO::FETCH_ASSOC);

    // Convert measurements into numbers
    foreach ($data as &$reading) {
        $reading['temperature'] =
            (float)$reading['temperature'];

        $reading['humidity'] =
            (float)$reading['humidity'];
    }
    unset($reading);

    // Return JSON data
    echo json_encode($data);

} catch (PDOException $e) {

    error_log($e->getMessage());

    http_response_code(500);

    echo json_encode([
        'error' => 'Unable to retrieve sensor data'
    ]);
}
?>
