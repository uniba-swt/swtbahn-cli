const serverAddress = "";
const trainEngine = "libtrain_engine_default (unremovable)";
const trackOutput = "master";

function dtISOStr() {
	return (new Date()).toISOString();
}

// Asynchronous wait for a duration in milliseconds.
function wait(duration) {
	return new Promise(resolve => setTimeout(resolve, duration));
}

function adminSetTrainSpeedPromise(train, speed) {
	console.log(dtISOStr() + ": adminSetTrainSpeedPromise(" + train + ", " + speed + ")");
	return $.ajax({
		type: 'POST',
		url: serverAddress + '/admin/set-dcc-train-speed',
		crossDomain: true,
		data: {
			'speed': speed,
			'train': train,
			'track-output': trackOutput
		},
		dataType: 'text',
		success: (responseData, textStatus, jqXHR) => {
			updateTrainStatePromise(train);
		},
		error: (responseData, textStatus, errorThrown) => {
			console.log(dtISOStr() + ": adminSetTrainSpeedPromise ERR: " + responseData.text + "; " + errorThrown);
		}
	});
}

function adminReleaseTrainPromise(train) {
	console.log(dtISOStr() + ": adminReleaseTrainPromise(" + train + ")");
	return $.ajax({
		type: 'POST',
		url: serverAddress + '/admin/release-train',
		crossDomain: true,
		data: {
			'train': train
		},
		dataType: 'text',
		success: (responseData, textStatus, jqXHR) => {
			updateTrainStatePromise(train);
		},
		error: (responseData, textStatus, errorThrown) => {
			console.log(dtISOStr() + ": adminReleaseTrainPromise ERR: " + responseData.text + "; " + errorThrown);
		}
	});
}

function startupPromise() {
	$('#monitoring_status_field').text("Starting...");
	return $.ajax({
		type: 'POST',
		url: '/admin/startup',
		crossDomain: true,
		data: null,
		dataType: 'text',
		success: function (responseData, textStatus, jqXHR) {
			$('#monitoring_status_field').text("Started");
		},
		error: function (responseData, textStatus, errorThrown) {
			alert("Startup Failed");
			$('#monitoring_status_field').text("Error");
		}
	});
}

function shutdownPromise() {
	$('#monitoring_status_field').text("Shutting down...");
	return $.ajax({
		type: 'POST',
		url: '/admin/shutdown',
		crossDomain: true,
		data: null,
		dataType: 'text',
		success: function (responseData, textStatus, jqXHR) {
			$('#monitoring_status_field').text("Stopped");
		},
		error: function (responseData, textStatus, errorThrown) {
			alert("Shutdown Failed");
			$('#monitoring_status_field').text("Error");
		}
	});
}

function adjustValueField(valueFieldSelector, value) {
	$(valueFieldSelector).text(value);
}

function updateTrainStatePromise(train) {
	return $.ajax({
		type: 'POST',
		url: serverAddress + '/monitor/train-state',
		crossDomain: true,
		data: {
			'train': this.trainId
		},
		dataType: 'text',
		success: (responseData, textStatus, jqXHR) => {
			const regexMatches = /grabbed: (.*?) - on segment: (.*?) - on block: (.*?) - orientation: (.*?) - speed step: (.*?) - detected speed (.*?) km\/h - direction: (.*?) /g.exec(responseData);
			if (regexMatches === null) {
				console.log(dtISOStr() + ": updateTrainStatePromise RegexMatches is NULL")
				return;
			}
			const grabbed = regexMatches[1];
			const segment = regexMatches[2];
			const block = regexMatches[3];
			const orientation = regexMatches[4];
			const speedStep = regexMatches[5];
			const detSpeed = regexMatches[6];
			const direction = regexMatches[7];
			console.log(dtISOStr() + ": " + `Extracted: grabbed ${grabbed}, block ${block}, speed step ${speedStep}, detected Speed ${detSpeed}, orientation ${orientation}`);
			$('#' + train + "_grabbed_value").text(grabbed);
			$('#' + train + "_on_block_value").text(block);
			$('#' + train + "_speed_step_value").text(speedStep);
			$('#' + train + "_detected_speed_value").text(detSpeed);
			$('#' + train + "_orientation_value").text(orientation);
			$('#' + train + "_direction_value").text(direction);
			if (grabbed.toLowerCase().includes("yes")) {
				$('#' + train + "_release_btn").classList.remove("disabled");
			} else {
				$('#' + train + "_release_btn").classList.add("disabled");
			}
			if (parseInt(speedStep, 10) > 0) {
				$('#' + train + "_release_btn").classList.remove("disabled");
			} else {
				$('#' + train + "_release_btn").classList.add("disabled");
			}
			$('#monitoring_status_field').text("Active");
		},
		error: (responseData, textStatus, errorThrown) => {
			if ($('#monitoring_status_field').text() !== "Starting...") {
				$('#monitoring_status_field').text("Error");
			}
		}
	});
}

function updateTrainStates() {
	trains = ["cargo_db", "cargo_bayern", "cargo_green", "regional_odeg", "regional_brengdirect"];
	for(train in trains) {
		updateTrainStatePromise(train);
	}
}

$(document).ready(
	async function () {
		await new Promise(r => setTimeout(r, 100));
		updateTrainStates();
		
		let updateInterval = setInterval(function() {
			updateTrainStates();
		}, 5000);
	}
);