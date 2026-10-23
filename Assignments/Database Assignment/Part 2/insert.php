
<?php
// EE 470 Database Assignment - Part 2
// Receives sensor readings using HTTP GET
// and inserts valid readings into MariaDB.

header('Content-Type: application/json; charset=utf-8');
header('Cache-Control: no-store');
header('Referrer-Policy: no-referrer');

// Load the private database configuration
require dirname(__DIR__, 2) . '/db_config.php';

// Function to return a response
function sendResponse($status, $message) {
    http_response_code($status);
    echo json_encode(['message' => $message]);
    exit;
}

// Only allow GET requests for now
if ($_SERVER['REQUEST_METHOD'] !== 'GET') {
    header('Allow: GET');
    sendResponse(405, 'Only GET requests are allowed.');
}

// Check the access key
$receivedKey = $_SERVER['HTTP_X_API_KEY']
    ?? ($_GET['key'] ?? '');

if (!isset($insertKey) ||
    strlen($insertKey) < 24 ||
    !hash_equals($insertKey, (string)$receivedKey)) {
    sendResponse(403, 'Access denied.');
}

 // Check whether the request contains Base64 data
 $params = $_GET;

 if (isset($_GET['payload'])) {

     // Decode the Base64 message
     $decoded = base64_decode($_GET['payload'], true);

     if ($decoded === false) {
         sendResponse(400, 'Invalid Base64 message.');
     }

     // Convert the decoded message into PHP variables
     parse_str($decoded, $params);
 }

// Read parameters from the regular or decoded request
$node = trim((string)($params['node_name'] ?? ''));
$time = trim((string)($params['time_received'] ?? ''));
$tempInput = $params['temperature'] ?? '';
$humidityInput = $params['humidity'] ?? '';

// Check the node name format
if (!preg_match('/^node_[0-9]+$/', $node) ||
    strlen($node) > 10) {
    sendResponse(400, 'Invalid node name.');
}

// Check that both measurements are numbers
if (!is_numeric($tempInput) ||
    !is_numeric($humidityInput)) {
    sendResponse(400, 'Temperature and humidity must be numbers.');
}

$temperature = (float)$tempInput;
$humidity = (float)$humidityInput;

// Check measurement ranges
if (!is_finite($temperature) ||
    !is_finite($humidity) ||
    $temperature < -10 || $temperature > 100 ||
    $humidity < 0 || $humidity > 100) {
    sendResponse(400, 'Temperature or humidity is out of range.');
}

// Check the date and time format if provided
if ($time !== '') {
    $date = DateTimeImmutable::createFromFormat(
        '!Y-m-d H:i:s', $time
    );

    if (!$date || $date->format('Y-m-d H:i:s') !== $time) {
        sendResponse(400, 'Invalid time format. Use YYYY-MM-DD HH:MM:SS.');
    }
}

try {
    // Connect to MariaDB
    $pdo = new PDO(
        "mysql:host=$dbHost;dbname=$dbName;charset=utf8mb4",
        $dbUser,
        $dbPass,
        [PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION]
    );

    // Use the supplied time, or let the database
    // generate its current timestamp
    if ($time === '') {
        $sql = "INSERT INTO sensor_data
                (node_name, time_received, temperature, humidity)
                VALUES (?, CURRENT_TIMESTAMP, ?, ?)";

        $stmt = $pdo->prepare($sql);
        $stmt->execute([$node, $temperature, $humidity]);

    } else {
        $sql = "INSERT INTO sensor_data
                (node_name, time_received, temperature, humidity)
                VALUES (?, ?, ?, ?)";

        $stmt = $pdo->prepare($sql);
        $stmt->execute([$node, $time, $temperature, $humidity]);
    }
    
    if (isset($_GET['payload'])) {
        http_response_code(201);
    
        echo json_encode([
            'message' => 'Sensor reading inserted successfully.',
            'decoded_parameters' => [
                'node_name' => $node,
                'time_received' => $time,
                'temperature' => $temperature,
                'humidity' => $humidity
            ]
        ], JSON_PRETTY_PRINT);
    
        exit;
    }

sendResponse(201, 'Sensor reading inserted successfully.');

} catch (PDOException $e) {
    // Handle database constraint errors
    $errorCode = (int)($e->errorInfo[1] ?? 0);

    if ($errorCode === 1062) {
        sendResponse(409, 'Duplicate reading for this node and time.');
    }

    if ($errorCode === 1452) {
        sendResponse(422, 'Sensor is not registered.');
    }

    if ($errorCode === 4025 || $errorCode === 3819) {
        sendResponse(422, 'Measurement violates database constraints.');
    }

    // Keep internal database errors private
    error_log($e->getMessage());
    sendResponse(500, 'Database operation failed.');
}
