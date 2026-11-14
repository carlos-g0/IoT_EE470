<?php
/*
 * EE 470 - MQTT Assignment
 * Section 4.2 F2
 *
 * Retrieves potentiometer readings
 * from the Hostinger database.
 *
 * The data will be used to display
 * a graph on our MQTT webpage.
 */

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');

// Load existing database configuration
require dirname(__DIR__, 2) . '/db_config.php';

try {

    // Connect to Hostinger database
    $pdo = new PDO(
        "mysql:host=$dbHost;dbname=$dbName;charset=utf8mb4",
        $dbUser,
        $dbPass,
        [
            PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
            PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC
        ]
    );

    // Use UTC for consistent timestamps
    $pdo->exec("SET time_zone = '+00:00'");

    // Get the latest 100 potentiometer readings
    $sql = "
        SELECT
            id,
            pot_value,
            DATE_FORMAT(
                time_received,
                '%Y-%m-%dT%H:%i:%sZ'
            ) AS time_received
        FROM mqtt_pot_readings
        ORDER BY id DESC
        LIMIT 100
    ";

    $stmt = $pdo->query($sql);

    $readings = $stmt->fetchAll();

    // Display readings in chronological order
    $readings = array_reverse($readings);

    // Convert numeric values to integers
    foreach ($readings as &$reading) {
        $reading['id'] = (int)$reading['id'];
        $reading['pot_value'] = (int)$reading['pot_value'];
    }
    unset($reading);

    // Send the readings as JSON
    echo json_encode([
        'status' => 'success',
        'count' => count($readings),
        'readings' => $readings
    ]);

} catch (PDOException $e) {

    error_log($e->getMessage());

    http_response_code(500);

    echo json_encode([
        'status' => 'error',
        'message' => 'Unable to retrieve readings'
    ]);
}
?>
