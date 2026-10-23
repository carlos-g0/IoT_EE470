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


// Retrieve readings and update the webpage
async function updateSensorChart() {

    // Get the currently selected sensor
    const selectedNode = nodeSelect.value;
    const requestId = ++latestRequest;

    // Update the statistics heading
    document.getElementById('selectedNode').textContent =
        selectedNode;

    document.getElementById('graphNodeName').textContent =
        selectedNode;

    try {

        // Get readings for selected node from PHP
        const url =
            'data.php?node=' +
            encodeURIComponent(selectedNode) +
            '&t=' + Date.now();

        const response = await fetch(url, {
            cache: 'no-store'
        });

        if (!response.ok) {
            throw new Error('Unable to load sensor data');
        }

        const readings = await response.json();

        if (!Array.isArray(readings)) {
            throw new Error('Invalid sensor data');
        }

        // Ignore an older response if the user switched nodes
        if (requestId !== latestRequest ||
            nodeSelect.value !== selectedNode) {
            return;
        }

        // Calculate statistics using ALL readings
        calculateStatistics(readings);

        // Plot only the latest 60 readings
        const graphReadings = readings.slice(-60);

        const times = graphReadings.map(
            row => row.time_received
        );

        const temperatures = graphReadings.map(
            row => Number(row.temperature)
        );

        // Create the graph if it does not exist yet
        if (sensorChart === null) {

            sensorChart = new Chart(
                document.getElementById('sensorChart'),
                {
                    type: 'line',

                    data: {
                        labels: times,

                        datasets: [{
                            label: 'Temperature',
                            data: temperatures,

                            backgroundColor: 'rgba(0, 128, 0, 0.2)',
                            borderColor: 'rgb(0, 128, 0)',
                            pointBackgroundColor: 'rgb(0, 128, 0)',

                            borderWidth: 3,
                            pointRadius: 4,
                            tension: 0.1,
                            fill: false
                        }]
                    },

                    options: {
                        responsive: true,
                        maintainAspectRatio: false,
                        animation: false,

                        plugins: {
                            title: {
                                display: true,
                                text: 'Sensor Node ' + selectedNode
                            }
                        },

                        scales: {
                            x: {
                                title: {
                                    display: true,
                                    text: 'Time Received (UTC)'
                                }
                            },

                            y: {
                                title: {
                                    display: true,
                                    text: 'Temperature (°C)'
                                }
                            }
                        }
                    }
                }
            );

        } else {

            // Update the graph with selected node data
            sensorChart.data.labels = times;
            sensorChart.data.datasets[0].data = temperatures;

            // Update the graph title
            sensorChart.options.plugins.title.text =
                'Sensor Node ' + selectedNode;

            sensorChart.update('none');
        }

        // Show when graph was last updated
        const status = document.getElementById('chartStatus');

        if (readings.length === 0) {
            status.textContent =
                'No readings available for ' + selectedNode;
        } else {
            status.textContent =
                'Last graph update: ' +
                new Date().toLocaleTimeString();
        }

    } catch (error) {

        // Ignore errors from outdated requests
        if (requestId !== latestRequest) {
            return;
        }

        console.error(error);

        document.getElementById('chartStatus').textContent =
            'Unable to update sensor data.';
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
                Number(reading.humidity).toFixed(2)
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

            cell.colSpan = 4;
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

