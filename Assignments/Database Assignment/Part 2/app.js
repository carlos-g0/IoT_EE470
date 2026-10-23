/*
 EE 470 - Database Assignment Part 2
 Display node_1 temperature readings using Chart.js
*/

// Get sensor readings from our PHP file
fetch('data.php', { cache: 'no-store' })
    .then(response => {
        if (!response.ok) {
            throw new Error('Could not load sensor data');
        }
        return response.json();
    })
    .then(readings => {

        // Get timestamps for the x-axis
        const times = readings.map(row => row.time_received);

        // Get temperatures for the y-axis
        const temperatures = readings.map(row => row.temperature);

        // Create the graph
        new Chart(document.getElementById('sensorChart'), {
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
                            text: 'Time Received'
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
        });
    })
    .catch(error => {
        console.error(error);
        document.getElementById('chartStatus').textContent =
            'Unable to load the sensor graph.';
    });
