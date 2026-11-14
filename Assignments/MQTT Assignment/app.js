/*
 EE 470 - Database Assignment Part 2
 Live Sensor Graph and Statistics

 Features:
 - Select a sensor using a dropdown
 - Display temperature readings using Chart.js
 - Calculate average, minimum, and maximum values
 - Automatically refresh every 5 seconds
*/

let sensorChart = null;
let latestRequest = 0;

// Get the dropdown element
const nodeSelect = document.getElementById('nodeSelect');

// Display a statistic
function setStatistic(id, value) {
    document.getElementById(id).textContent = value;
}

// Calculate average, minimum, and maximum
function calculateStatistics(readings) {

    const temperatures = readings
        .map(row => Number(row.temperature))
        .filter(Number.isFinite);

    const humidities = readings
        .map(row => Number(row.humidity))
        .filter(Number.isFinite);

    if (temperatures.length === 0 ||
        humidities.length === 0) {

        const ids = [
            'avgTemp', 'minTemp', 'maxTemp',
            'avgHumidity', 'minHumidity', 'maxHumidity'
        ];

        ids.forEach(id => setStatistic(id, '--'));
        return;
    }

    // Temperature calculations
    const avgTemp = temperatures.reduce(
        (sum, value) => sum + value, 0
    ) / temperatures.length;

    const minTemp = Math.min(...temperatures);
    const maxTemp = Math.max(...temperatures);

    // Humidity calculations
    const avgHumidity = humidities.reduce(
        (sum, value) => sum + value, 0
    ) / humidities.length;

    const minHumidity = Math.min(...humidities);
    const maxHumidity = Math.max(...humidities);

    // Display temperature statistics
    setStatistic('avgTemp', avgTemp.toFixed(2));
    setStatistic('minTemp', minTemp.toFixed(2));
    setStatistic('maxTemp', maxTemp.toFixed(2));

    // Display humidity statistics
    setStatistic('avgHumidity', avgHumidity.toFixed(2));
    setStatistic('minHumidity', minHumidity.toFixed(2));
    setStatistic('maxHumidity', maxHumidity.toFixed(2));
}


// Retrieve readings and update the shared chart.
// The MQTT option uses its own data endpoint; original sensor nodes
// continue to use the original data.php endpoint.
async function updateSensorChart() {
    const selectedNode = nodeSelect.value;
    const showingMQTT = selectedNode === 'mqtt_pot';
    const requestId = ++latestRequest;
    const chartStatus = document.getElementById('chartStatus');

    chartStatus.textContent = 'Loading readings...';

    try {
        const url = showingMQTT
            ? '../mqtt/mqtt_data.php?t=' + Date.now()
            : 'data.php?node=' + encodeURIComponent(selectedNode) +
              '&t=' + Date.now();

        const response = await fetch(url, { cache: 'no-store' });
        if (!response.ok) {
            throw new Error('Unable to load readings (HTTP ' + response.status + ')');
        }

        const result = await response.json();
        const readings = showingMQTT ? result.readings : result;

        if (!Array.isArray(readings) ||
            (showingMQTT && result.status !== 'success')) {
            throw new Error('Invalid readings returned by server');
        }

        // Prevent a slow response from overwriting a newly selected option.
        if (requestId !== latestRequest || nodeSelect.value !== selectedNode) {
            return;
        }

        document.getElementById('sensorStatsHeading').hidden = showingMQTT;
        document.getElementById('sensorStats').hidden = showingMQTT;
        document.getElementById('mqttStats').hidden = !showingMQTT;

        let labels;
        let values;
        let graphTitle;
        let axisTitle;
        let lineColor;
        let graphDescription;
        let lastRecordText = '';

        if (showingMQTT) {
            // The API sends timestamps in ISO 8601 UTC. Show them in
            // the viewer's local time, with an explicit local-time label.
            const graphReadings = readings.slice(-60);
            labels = graphReadings.map(row =>
                new Date(row.time_received).toLocaleTimeString()
            );
            values = graphReadings.map(row => Number(row.pot_value));

            const allValues = readings.map(row => Number(row.pot_value))
                .filter(Number.isFinite);
            const latest = readings.length ? readings[readings.length - 1] : null;

            document.getElementById('mqttLatest').textContent =
                latest ? String(latest.pot_value) : '--';
            document.getElementById('mqttMin').textContent =
                allValues.length ? String(Math.min(...allValues)) : '--';
            document.getElementById('mqttMax').textContent =
                allValues.length ? String(Math.max(...allValues)) : '--';
            document.getElementById('mqttCount').textContent =
                String(readings.length);
            document.getElementById('mqttUpdated').textContent =
                latest ? new Date(latest.time_received).toLocaleString() : '--';

            graphTitle = 'MQTT Potentiometer Readings';
            axisTitle = 'Potentiometer (ADC, 0–1023)';
            lineColor = 'rgb(34, 94, 160)';
            graphDescription = 'ESP8266 readings received through HiveMQ and the Python relay.';
            lastRecordText = latest
                ? 'Last stored reading: ' + new Date(latest.time_received).toLocaleString()
                : 'No MQTT readings stored yet.';
        } else {
            // The original Database Assignment still behaves as before.
            document.getElementById('selectedNode').textContent = selectedNode;
            calculateStatistics(readings);

            const graphReadings = readings.slice(-60);
            labels = graphReadings.map(row => row.time_received);
            values = graphReadings.map(row => Number(row.temperature));

            graphTitle = 'Sensor Temperature Graph';
            axisTitle = 'Temperature (°C)';
            lineColor = 'rgb(0, 128, 0)';
            graphDescription = 'Temperature readings recorded by ' + selectedNode + '.';
            lastRecordText = readings.length
                ? 'Last graph update: ' + new Date().toLocaleTimeString()
                : 'No readings available for ' + selectedNode;
        }

        document.getElementById('graphTitle').textContent = graphTitle;
        document.getElementById('graphDescription').textContent = graphDescription;

        const dataset = {
            label: showingMQTT ? 'Potentiometer' : 'Temperature',
            data: values,
            borderColor: lineColor,
            backgroundColor: lineColor,
            pointBackgroundColor: lineColor,
            borderWidth: 3,
            pointRadius: 3,
            tension: 0.1,
            fill: false
        };

        if (sensorChart === null) {
            sensorChart = new Chart(
                document.getElementById('sensorChart'),
                {
                    type: 'line',
                    data: { labels, datasets: [dataset] },
                    options: {
                        responsive: true,
                        maintainAspectRatio: false,
                        animation: false,
                        plugins: {
                            title: { display: true, text: graphTitle }
                        },
                        scales: {
                            x: {
                                title: {
                                    display: true,
                                    text: showingMQTT ? 'Time Received (Local Time)' : 'Time Received (UTC)'
                                }
                            },
                            y: {
                                title: { display: true, text: axisTitle },
                                min: showingMQTT ? 0 : undefined,
                                max: showingMQTT ? 1023 : undefined
                            }
                        }
                    }
                }
            );
        } else {
            sensorChart.data.labels = labels;
            sensorChart.data.datasets = [dataset];
            sensorChart.options.plugins.title.text = graphTitle;
            sensorChart.options.scales.x.title.text = showingMQTT
                ? 'Time Received (Local Time)' : 'Time Received (UTC)';
            sensorChart.options.scales.y.title.text = axisTitle;
            sensorChart.options.scales.y.min = showingMQTT ? 0 : undefined;
            sensorChart.options.scales.y.max = showingMQTT ? 1023 : undefined;
            sensorChart.update('none');
        }

        chartStatus.textContent = lastRecordText;
    } catch (error) {
        if (requestId !== latestRequest) return;
        console.error('Chart update failed:', error);
        chartStatus.textContent = 'Unable to update graph. Check the data endpoint.';
    }
}


// Update immediately when dropdown selection changes
nodeSelect.addEventListener('change', updateSensorChart);

// Load sensor data when webpage opens
updateSensorChart();

// Automatically refresh every 5 seconds
setInterval(updateSensorChart, 5000);

/*
 EE 470 - Live Sensor Data Table

 Automatically retrieve all sensor readings
 and update the HTML table every 5 seconds.
*/

let tableUpdating = false;

async function updateSensorTable() {

    // Prevent overlapping requests
    if (tableUpdating) return;

    tableUpdating = true;

    try {

        // Request all readings from PHP
        const response = await fetch(
            'readings.php?t=' + Date.now(),
            { cache: 'no-store' }
        );

        if (!response.ok) {
            throw new Error('Unable to load sensor readings');
        }

        const readings = await response.json();

        if (!Array.isArray(readings)) {
            throw new Error('Invalid sensor data');
        }

        // Find the existing table body
        const tableBody =
            document.getElementById('sensorDataRows');

        // Create the updated table rows
        const fragment = document.createDocumentFragment();

        readings.forEach(reading => {

            const row = document.createElement('tr');

            const values = [
                reading.node_name,
                reading.time_received,
                Number(reading.temperature).toFixed(2),
                Number(reading.humidity).toFixed(2),
                reading.light_level == null
                    ? '--'
                    : String(reading.light_level)
            ];

            values.forEach(value => {

                const cell = document.createElement('td');

                // Safely insert text into the cell
                cell.textContent = value;

                row.appendChild(cell);
            });

            fragment.appendChild(row);
        });

        // Show a message if the table is empty
        if (readings.length === 0) {

            const row = document.createElement('tr');
            const cell = document.createElement('td');

            cell.colSpan = 5;
            cell.textContent = 'No sensor readings available.';

            row.appendChild(cell);
            fragment.appendChild(row);
        }

        // Replace old rows with updated rows
        tableBody.replaceChildren(fragment);

    } catch (error) {

        console.error('Table update failed:', error);

    } finally {

        tableUpdating = false;
    }
}

// Load the table when the webpage opens
updateSensorTable();

// Refresh the table automatically every 5 seconds
setInterval(updateSensorTable, 5000);


/*
 EE 470 - Sensor Activity Table

 Retrieves the number of successfully stored
 readings for each registered sensor node.
*/

let activityUpdating = false;

async function updateActivityTable() {

    // Prevent overlapping requests
    if (activityUpdating) return;

    activityUpdating = true;

    try {

        // Get activity counters from PHP
        const response = await fetch(
            'activity.php?t=' + Date.now(),
            { cache: 'no-store' }
        );

        if (!response.ok) {
            throw new Error('Unable to load sensor activity');
        }

        const activity = await response.json();

        if (!Array.isArray(activity)) {
            throw new Error('Invalid activity data');
        }

        // Find the activity table
        const tableBody =
            document.getElementById('activityRows');

        const fragment = document.createDocumentFragment();

        // Create one row for each registered node
        activity.forEach(sensor => {

            const row = document.createElement('tr');

            const nodeCell = document.createElement('td');
            nodeCell.textContent = sensor.node_name;

            const countCell = document.createElement('td');
            countCell.textContent = sensor.message_count;

            row.appendChild(nodeCell);
            row.appendChild(countCell);

            fragment.appendChild(row);
        });

        // Display a message if no sensors exist
        if (activity.length === 0) {

            const row = document.createElement('tr');
            const cell = document.createElement('td');

            cell.colSpan = 2;
            cell.textContent = 'No registered sensors found.';

            row.appendChild(cell);
            fragment.appendChild(row);
        }

        // Replace the displayed counters
        tableBody.replaceChildren(fragment);

    } catch (error) {

        console.error('Activity update failed:', error);

    } finally {

        activityUpdating = false;
    }
}

// Load counters when the webpage opens
updateActivityTable();

// Refresh counters every 5 seconds
setInterval(updateActivityTable, 5000);
