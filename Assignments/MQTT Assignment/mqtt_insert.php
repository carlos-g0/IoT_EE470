<?php
/*
 * EE 470 - MQTT Assignment
 * Section 4.2 F2
 *
 * Receives potentiometer readings from the
 * Python MQTT relay and stores them in
 * the Hostinger MariaDB database.
 */

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');

// --------------------------------------------------
// Check request method
// --------------------------------------------------

// Only allow POST requests
if ($_SERVER['REQUEST_METHOD'] !== 'POST') {

    header('Allow: POST');
    http_response_code(405);

    echo json_encode([
        'error' => 'POST required'
    ]);

    exit;
}

// --------------------------------------------------
// Database configuration
// --------------------------------------------------

// Reuse the private configuration file
// from the previous EE 470 assignment.
require dirname(__DIR__, 2) . '/db_config.php';

// --------------------------------------------------
// Check API key
// --------------------------------------------------

// Read the API key sent by the Python relay
$receivedKey = $_SERVER['HTTP_X_API_KEY'] ?? '';

// Make sure the API key is valid
if (!isset($insertKey) ||
    !is_string($insertKey) ||
    strlen($insertKey) < 24 ||
    !is_string($receivedKey) ||
    !hash_equals($insertKey, $receivedKey)) {

    http_response_code(403);

    echo json_encode([
        'error' => 'Access denied'
    ]);

    exit;
}

// --------------------------------------------------
// Read the received MQTT data
// --------------------------------------------------

// Read JSON data sent by Python
$input = json_decode(
    file_get_contents('php://input'),
    true
);

// Check that the potentiometer value exists
if (!is_array($input) ||
    !isset($input['pot_value']) ||
    !is_int($input['pot_value'])) {

    http_response_code(400);

    echo json_encode([
        'error' => 'Invalid potentiometer value'
    ]);

    exit;
}

// Store the received value
$potValue = $input['pot_value'];

// Check the valid potentiometer range
if ($potValue < 0 || $potValue > 1023) {

    http_response_code(400);

    echo json_encode([
        'error' => 'Value must be between 0 and 1023'
    ]);

    exit;
}

// --------------------------------------------------
// Connect to Hostinger database
// --------------------------------------------------

try {

    // Use the database credentials from db_config.php
    $pdo = new PDO(
        "mysql:host=$dbHost;dbname=$dbName;charset=utf8mb4",
        $dbUser,
        $dbPass,
        [
            PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
            PDO::ATTR_DEFAULT_FETCH_MODE => PDO::FETCH_ASSOC
        ]
    );

    // Store timestamps using UTC
    $pdo->exec("SET time_zone = '+00:00'");

    // --------------------------------------------------
    // Insert potentiometer reading
    // --------------------------------------------------

    $sql = "INSERT INTO mqtt_pot_readings
            (pot_value)
            VALUES (:pot_value)";

    $stmt = $pdo->prepare($sql);

    $stmt->bindValue(
        ':pot_value',
        $potValue,
        PDO::PARAM_INT
    );

    $stmt->execute();

    // Get the ID of the new reading
    $insertedID = $pdo->lastInsertId();

    // --------------------------------------------------
    // Return successful response
    // --------------------------------------------------

    http_response_code(201);

    echo json_encode([
        'status' => 'success',
        'message' => 'MQTT reading stored',
        'id' => $insertedID,
        'pot_value' => $potValue
    ]);

}

// --------------------------------------------------
// Handle database errors
// --------------------------------------------------

catch (PDOException $e) {

    // Save the error to the server log
    error_log($e->getMessage());

    http_response_code(500);

    echo json_encode([
        'error' => 'Database insert failed'
    ]);
}
