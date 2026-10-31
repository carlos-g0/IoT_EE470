<?php
/*
 * EE 470 - Programming ESP8266
 * File: activity.php
 * Version: 1.0
 *
 * Purpose:
 * Retrieve the number of successfully
 * stored sensor readings for each node.
 *
 * Input: MariaDB sensor_activity table
 * Output: JSON containing node names
 *         and message counts
 */

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');

// Load database configuration
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

    // Get the message count for every registered node
    $sql = "SELECT
                r.node_name,
                COALESCE(a.message_count, 0) AS message_count
            FROM sensor_register AS r
            LEFT JOIN sensor_activity AS a
                ON r.node_name = a.node_name
            ORDER BY r.node_name";

    $activity = $pdo->query($sql)->fetchAll();

    // Convert counts to integers
    foreach ($activity as &$row) {
        $row['message_count'] = (int)$row['message_count'];
    }
    unset($row);

    // Return the results as JSON
    echo json_encode($activity);

} catch (PDOException $e) {

    error_log($e->getMessage());

    http_response_code(500);

    echo json_encode([
        'error' => 'Unable to retrieve sensor activity.'
    ]);
}
?>
