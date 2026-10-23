/*
 EE 470 - Database Assignment Part 2
 Live Temperature Graph for node_1
*/

// Store our Chart.js graph
let sensorChart = null;
let updating = false;

// Retrieve new readings and update the graph
async function updateSensorChart() {

    if (updating) return;
    updating = true;

    try {

        // Request sensor readings from MariaDB via PHP
        const response = await fetch(
            'data.php?t=' + Date.now(),
            { cache: 'no-store' }
        );

        if (!response.ok) {
            throw new Error('Unable to retrieve sensor data');
        }

        const allReadings = await response.json();

        if (!Array.isArray(allReadings)) {
            throw new Error('Invalid JSON data');
        }

        // Display only the latest 60 readings
        const readings = allReadings.slice(-60);

        // Extract timestamps and temperatures
        const times = readings.map(
            row => row.time_received
        );

        const temperatures = readings.map(
            row => Number(row.temperature)
        );

        // Create the graph the first time
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
                                text: 'Sensor Node node_1'
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

            // Update the existing graph
            sensorChart.data.labels = times;
            sensorChart.data.datasets[0].data = temperatures;

            sensorChart.update('none');
        }

        // Display the latest update time
        const status = document.getElementById('chartStatus');

        if (status) {
            status.textContent =
                'Last graph update: ' +
                new Date().toLocaleTimeString();
        }

    } catch (error) {

        console.error(error);

        const status = document.getElementById('chartStatus');

        if (status) {
            status.textContent =
                'Unable to update sensor graph.';
        }

    } finally {
        updating = false;
    }
}

// Load readings immediately
updateSensorChart();

// Refresh graph every 5 seconds
setInterval(updateSensorChart, 5000);
