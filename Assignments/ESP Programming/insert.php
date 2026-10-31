<?php
/*
 * EE 470 - Programming ESP8266
 * File: insert.php
 * Version: 3.0
 *
 * Purpose:
 * Receive sensor readings from the ESP8266,
 * validate them, and store them in MariaDB.
 *
 * Inputs:
 * Node name, timestamp, temperature,
 * humidity, optional light level, and API key.
 *
 * Outputs:
 * JSON response indicating whether the
 * sensor reading was accepted.
 */

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');
header('Referrer-Policy: no-referrer');

// Load private database configuration
require dirname(__DIR__, 2) . '/db_config.php';

// Return a JSON response
function sendResponse(
    int $status,
    string $message,
    array $extra = []
): void {

    http_response_code($status);

    echo json_encode(
        array_merge(['message' => $message], $extra)
    );

    exit;
}

// Only allow GET requests
if ($_SERVER['REQUEST_METHOD'] !== 'GET') {

    header('Allow: GET');

    sendResponse(
        405,
        'Only GET requests are allowed.'
    );
}

// Check the API key
$receivedKey = $_SERVER['HTTP_X_API_KEY']
    ?? ($_GET['key'] ?? '');

if (!isset($insertKey) ||
    !is_string($insertKey) ||
    strlen($insertKey) < 24 ||
    !is_string($receivedKey) ||
    !hash_equals($insertKey, $receivedKey)) {

    sendResponse(403, 'Access denied.');
}

// Check whether Base64 data was provided
$usingPayload = array_key_exists('payload', $_GET);
$params = $_GET;

if ($usingPayload) {

    if (!is_string($_GET['payload']) ||
        $_GET['payload'] === '') {

        sendResponse(400, 'Invalid Base64 message.');
    }

    // Decode Base64 data
    $decoded = base64_decode(
        $_GET['payload'],
        true
    );

    if ($decoded === false) {
        sendResponse(400, 'Invalid Base64 message.');
    }

    // Extract the decoded parameters
    parse_str($decoded, $params);
}

// Read parameters
$nodeInput = $params['node_name'] ?? null;
$timeInput = $params['time_received'] ?? null;
$tempInput = $params['temperature'] ?? null;
$humidityInput = $params['humidity'] ?? null;

// Validate node name
if (!is_string($nodeInput) ||
    !preg_match('/^node_[0-9]+$/', trim($nodeInput)) ||
    strlen(trim($nodeInput)) > 10) {

    sendResponse(400, 'Invalid node name.');
}

$node = trim($nodeInput);

// Timestamp is now required
if (!is_string($timeInput) ||
    trim($timeInput) === '') {

    sendResponse(400, 'Timestamp is required.');
}

$time = trim($timeInput);

// Validate timestamp format
if (!preg_match(
    '/^\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}$/',
    $time
)) {

    sendResponse(
        400,
        'Invalid time format. Use YYYY-MM-DD HH:MM:SS.'
    );
}

$date = DateTimeImmutable::createFromFormat(
    '!Y-m-d H:i:s',
    $time
);

if (!$date ||
    $date->format('Y-m-d H:i:s') !== $time) {

    sendResponse(
        400,
        'Invalid time format. Use YYYY-MM-DD HH:MM:SS.'
    );
}

// Validate temperature and humidity
if (!is_scalar($tempInput) ||
    !is_scalar($humidityInput) ||
    !is_numeric($tempInput) ||
    !is_numeric($humidityInput)) {

    sendResponse(
        400,
        'Temperature and humidity must be numbers.'
    );
}

$temperature = (float)$tempInput;
$humidity = (float)$humidityInput;

// Check allowed measurement ranges
if (!is_finite($temperature) ||
    !is_finite($humidity) ||
    $temperature < -10 ||
    $temperature > 100 ||
    $humidity < 0 ||
    $humidity > 100) {

    sendResponse(
        400,
        'Temperature or humidity is out of range.'
    );
}

// Read and validate the photoresistor value
// Allow NULL for older readings without light data
$lightLevel = null;

if (array_key_exists('light_level', $params)) {

    $rawLight = $params['light_level'];

    if (!is_string($rawLight) ||
        !preg_match('/^\d{1,4}$/', trim($rawLight))) {

        sendResponse(
            400,
            'Light level must be an integer from 0 to 1023.'
        );
    }

    $lightLevel = (int)trim($rawLight);

    if ($lightLevel > 1023) {

        sendResponse(
            400,
            'Light level must be an integer from 0 to 1023.'
        );
    }
}

$pdo = null;

try {

    // Connect to MariaDB
    $pdo = new PDO(
        "mysql:host=$dbHost;dbname=$dbName;charset=utf8mb4",
        $dbUser,
        $dbPass,
        [
            PDO::ATTR_ERRMODE =>
                PDO::ERRMODE_EXCEPTION,

            PDO::ATTR_DEFAULT_FETCH_MODE =>
                PDO::FETCH_ASSOC
        ]
    );

    // Start database transaction
    $pdo->beginTransaction();

    // Check whether the sensor is registered
    $checkNode = $pdo->prepare(
        "SELECT 1
         FROM sensor_register
         WHERE node_name = ?
         LIMIT 1"
    );

    $checkNode->execute([$node]);

    if ($checkNode->fetchColumn() === false) {

        $pdo->rollBack();

        sendResponse(
            422,
            'Sensor is not registered.'
        );
    }

    // Insert the new sensor reading
    $insert = $pdo->prepare(
        "INSERT INTO sensor_data
         (
             node_name,
             time_received,
             temperature,
             humidity,
             light_level
         )
         VALUES (?, ?, ?, ?, ?)"
    );

    $insert->execute([
        $node,
        $time,
        $temperature,
        $humidity,
        $lightLevel
    ]);

    // Increase the successful message count
    // Also handles newly registered sensor nodes
    $count = $pdo->prepare(
        "INSERT INTO sensor_activity
         (node_name, message_count)
         VALUES (?, 1)
         ON DUPLICATE KEY UPDATE
         message_count = message_count + 1"
    );

    $count->execute([$node]);

    // Save both database changes together
    $pdo->commit();

    // Preserve Base64 debugging information
    $extra = [];

    if ($usingPayload) {

        $extra['decoded_parameters'] = [
            'node_name' => $node,
            'time_received' => $time,
            'temperature' => $temperature,
            'humidity' => $humidity,
            'light_level' => $lightLevel
        ];
    }

    sendResponse(
        201,
        'Sensor reading inserted successfully.',
        $extra
    );

} catch (PDOException $e) {

    // Undo changes if anything failed
    if ($pdo instanceof PDO &&
        $pdo->inTransaction()) {

        $pdo->rollBack();
    }

    // Identify the database error
    $errorCode = (int)($e->errorInfo[1] ?? 0);

    // Duplicate node and timestamp
    if ($errorCode === 1062) {

        sendResponse(
            409,
            'Duplicate reading for this node and time.',
            ['already_stored' => true]
        );
    }

    // Unregistered sensor
    if ($errorCode === 1452) {

        sendResponse(
            422,
            'Sensor is not registered.'
        );
    }

    // Database constraint violation
    if ($errorCode === 4025 ||
        $errorCode === 3819) {

        sendResponse(
            422,
            'Measurement violates database constraints.'
        );
    }

    // Log internal errors without exposing details
    error_log($e->getMessage());

    sendResponse(
        500,
        'Database operation failed.'
    );

} catch (Throwable $e) {

    if ($pdo instanceof PDO &&
        $pdo->inTransaction()) {

        $pdo->rollBack();
    }

    error_log($e->getMessage());

    sendResponse(500, 'Server error.');
}
