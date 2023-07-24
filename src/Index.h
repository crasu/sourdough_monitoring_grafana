#pragma once

const char MAIN_PAGE[] PROGMEM = R"=====(
<!DOCTYPE HTML>
<html>

<head>
    <meta charset='utf-8'>
    <title>Sourdough Monitor</title>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/Chart.js/4.2.1/chart.umd.min.js" integrity="sha512-GCiwmzA0bNGVsp1otzTJ4LWQT2jjGJENLGyLlerlzckNI30moi2EQT0AfRI7fLYYYDKR+7hnuh35r3y1uJzugw==" crossorigin="anonymous" referrerpolicy="no-referrer"></script>
    <script src="https://cdnjs.cloudflare.com/ajax/libs/date-fns/1.30.1/date_fns.min.js" integrity="sha512-F+u8eWHrfY8Xw9BLzZ8rG/0wIvs0y+JyRJrXjp3VjtFPylAEEGwKbua5Ip/oiVhaTDaDs4eU2Xtsxjs/9ag2bQ==" crossorigin="anonymous" referrerpolicy="no-referrer"></script>

    <style>
        body {
            min-width: 310px;
            max-width: 800px;
            height: 400px;
            margin: 0 auto;
        }

        h2 {
            font-family: Arial;
            font-size: 2.5rem;
            text-align: center;
        }
        .chart {
            margin: 20px;
        }
        .button-container {
            text-align: center;
        }
        .button {
            margin: 5px;
            background-color: rgb(135 170 193);
            border: none;
            color: white;
            display: inline-block;
            font-size: 1em;
            padding: 10px;
            transition: 0.35s;
            text-decoration: none;
            border-radius: 10px;
            font-family: sans-serif;
        }
        .button:hover {
            text-decoration: none;
            background-color: rgb(75 171 236);
            border-color: rgb(75 171 236);;
        }
    </style>
</head>

<body>
    <div><canvas class="chart" id="historyChart"></canvas></div>
    <div><canvas class="chart" id="currentChart"></canvas></div>
    <div class="button-container">
        <a class="button" href="./stats/week">Week json</a>
        <a class="button" href="./stats/current">Current json</a>
        <a class="button" href="./stats/clear">Clear Week json</a>
    </div>
</body>

<script>
    const historyChartElement = document.getElementById('historyChart');
    const currentChartElement = document.getElementById('currentChart');

    const CHART_COLORS = {
        red: 'rgb(255, 99, 132)',
        orange: 'rgb(255, 159, 64)',
        yellow: 'rgb(255, 205, 86)',
        green: 'rgb(75, 192, 192)',
        blue: 'rgb(54, 162, 235)',
        purple: 'rgb(153, 102, 255)',
        grey: 'rgb(201, 203, 207)'
    };

    const NAMED_COLORS = [
        CHART_COLORS.red,
        CHART_COLORS.orange,
        CHART_COLORS.yellow,
        CHART_COLORS.green,
        CHART_COLORS.blue,
        CHART_COLORS.purple,
        CHART_COLORS.grey,
    ];

    function namedColor(index) {
        return NAMED_COLORS[index % NAMED_COLORS.length];
    }

    const historyChartData = {
        labels: ["a", "b"],
        datasets: [],
    };

    let historyChart = new Chart(historyChartElement, {
        type: 'line',
        data: historyChartData,
        options: {
            scales: {
                y: {
                    beginAtZero: true
                }
            }
        }
    });

    const currentChartData = {
        labels: ["Humidty", "Temperature", "Height"],
        datasets: []
    };

    let currentChart = new Chart(currentChartElement, {
        type: 'bar',
        data: currentChartData,
        options: {
            indexAxis: 'y',
            scales: {
                y: {
                    beginAtZero: true
                }
            },
            elements: {
                bar: {
                    borderWidth: 2,
                }
            },
            plugins: {
                legend: {
                    display: false,
                }
            }
        }
    });

    function extractCurrentLabels(parsedJson) {
        return Object.keys(parsedJson).filter(key => key !== "time");
    }

    function extractHistoryLabels(parsedJson) {
        let labels = [];

        for(obj of parsedJson) {
            var d = new Date(0); 
            d.setUTCSeconds(obj["time"]);
            var datestr = dateFns.format(d, "HH:MM:ss");
            labels.push(datestr);
        }

        return labels;
    }

    function extractCurrentDatasets(parsedJson) {
        let data = [];
        let color = [];
        for (var key in parsedJson) {
            if(key === "time") {
                continue;
            } 

            data.push(parsedJson[key]);
            color.push(namedColor(data.length))
        }

        return [{
            label: "My First dataset",
            data: data,
            backgroundColor: color, 
        }];
    }

    function extractHistoryDatasets(parsedJson) {
        let datasets = [];

        var keys = Object.keys(parsedJson[0]);

        for (var key of keys) {
            if(key === "time") {
                continue;
            } 

            const newDataset = {
                label: key,
                data: parsedJson.map((obj) => obj[key]),
                fill: false,
                borderColor: namedColor(datasets.length),
                tension: 0.1
            };

            datasets.push(newDataset);
        }

        return datasets;
    }

    async function retrieveCurrentStats() {
        const response = await fetch("./stats/current");
        return parsedJson = await response.json();
    }

    async function retrieveWeeklyStats() {
        const response = await fetch("./stats/week");
        return parsedJson = await response.json();
    }

    async function updateChart() {
        let historyJson, currentJson;

        try {
            [currentJson, historyJson]=await Promise.all([retrieveCurrentStats(), retrieveWeeklyStats()]);
        } catch (e) {
            console.error(e);
            return;
        }

        historyChartData.labels = extractHistoryLabels(historyJson);
        historyChartData.datasets = extractHistoryDatasets(historyJson);

        currentChartData.labels = extractCurrentLabels(currentJson);
        currentChartData.datasets = extractCurrentDatasets(currentJson);

        historyChart.update();
        currentChart.update();
    }

    setTimeout(updateChart, 1000);
    setInterval(updateChart, 30000);
</script>
</html>
)=====";