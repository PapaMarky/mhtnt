var temp_hist = [];
var temp_max = 0;
var temp_min = 0;

function set_temp() {
    var x = new XMLHttpRequest();
    x.onreadystatechange = function () {
	if (this.readyState == 4 && this.status == 200) {
	    var data = JSON.parse(this.responseText);
	    document.getElementById("tempF").innerHTML = "" + data.f + "F";
	    document.getElementById("tempC").innerHTML = "" + data.c + "C";
	}
    }
    x.open("GET", "get_temp", true);
    x.send();
}

function set_time() {
    var x = new XMLHttpRequest();
    x.onreadystatechange = function() {
	if (this.readyState == 4 && this.status == 200) {
	    var data = JSON.parse(this.responseText);
	    // console.log("time json: " + data);
	    document.getElementById("time").innerHTML = (data.hour < 10 ? "0" : "") + data.hour + ":" + (data.minute < 10 ? "0" : "") + data.minute;
	}
    }
    x.open("GET", "get_time", true);
    x.send();
}

function set_history() {
    var x = new XMLHttpRequest();
    x.onreadystatechange = function() {
	if (this.readyState == 4 && this.status == 200) {
	    console.log("RAW: " + this.responseText);
	    var data = JSON.parse(this.responseText);
	    // console.log("history json: " + data);
	    temp_hist = data.history;
	    if (temp_hist.length > 0) {
		temp_max = data.max;
		temp_min = data.min;
	    }
	    drawChart();
	}
    }
    x.open("GET", "get_history", true);
    x.send();
}

function get_browser_time_settings() {
    var time_settings = {"tz_name": "UTC", "tz_offset": 0, "tz_dst": False},
	options = Intl.DateTimeFormat().resolvedOptions(),
	    d = new Date();

    time_settings.tz_name = options.timeZone;
    time_settings.tz_offset = d.getTimezoneOffset();
    time_settings.tz_dst = d.isDstObserved();
}

function drawChart() {
    if (temp_hist.length < 2) {
	return;
    }

    var high, high_t, low, low_t;
    low_t = temp_hist[temp_min][0];
    low = temp_hist[temp_min][2];

    high_t = temp_hist[temp_max][0];
    high = temp_hist[temp_max][3];

    var data = new google.visualization.DataTable();
    data.addColumn('string', '');
    data.addColumn('number', 'current');
    data.addColumn('number', 'low');
    data.addColumn('number', 'high');
    data.addColumn('number', 'F');

    data.addRows(temp_hist);
    var d1 = new Date(temp_hist[0][0]);
    var d2 = new Date(temp_hist[temp_hist.length - 1][0]);

    low_t = low_t.replace(" ", "<br/>");
    high_t = high_t.replace(" ", "<br/>");
    document.getElementById('low_temp').innerHTML = "Low: " + low + "<br/>" + low_t;
    document.getElementById('high_temp').innerHTML = "High: " + high + "<br/>" + high_t;

    console.log('d1: ' + d1);
    console.log('d2: ' + d2);
    var minutes = (d2 - d1) / 60000;
    var h = Math.floor(minutes / 60.0);
    var m = Math.floor(minutes - (h * 60));
    console.log("raw minutes of data: " + minutes);
    sub_t = "";
    if (h > 0) {
        sub_t = "" + h + " hour";
        if (h != 1) sub_t += "s";
        if (m > 0) {
            sub_t += ", " + m + " minute";
            if (m != 1) sub_t += "s";
        }
    } else if (m > 0) {
        sub_t = "" + m + " minutes"
    }
    console.log("SUBTITLE: " + sub_t);
    var options = {
        chart: {
            title: 'History',
            subtitle: sub_t
        },
	legend: {
	    position: 'none'
	},
	hAxis: {
	    textPosition: 'none',
	    title: '',
	    viewWindowMode: 'maximized',
	    textStyle: {
		fontSize: 0
	    }
	}
    };

    var chart = new google.charts.Line(document.getElementById('history_chart'));

    chart.draw(data, google.charts.Line.convertOptions(options));
}
